#include "wmcore.h"

#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QStringList>
#include <QHash>
#include <QList>
#include <cstdio>
#include <unistd.h>

// Non-GUI entry point for the Raspberry Pi. Owns the hardware directly (same
// Osc/Powermeter/Pulser classes WM uses) and takes commands as plain text
// lines — over stdin (for local/SSH use) and over TCP (for a remote client,
// e.g. the PC GUI). Both transports share one WMCore and one command
// dispatcher, so behaviour is identical either way. The TCP socket is the
// one Bluetooth PAN will carry once that's wired up — this isn't throwaway:
// a Bluetooth-PAN connection is just another network interface, so nothing
// here needs to change when that lands, only how the socket gets reached.

static const quint16 kDefaultPort = 8765;

static void printLine(const QString &s)
{
    static QTextStream out(stdout);
    out << s << '\n';
    out.flush();
}

static void writeLine(QTcpSocket *sock, const QString &s)
{
    sock->write((s + '\n').toUtf8());
}

static QStringList helpLines()
{
    return {
        "OK commands:",
        "  ports                          list available serial ports",
        "  connect [port]                 connect pulser/oscilloscope/power meter",
        "  start <from> <to> <step> <b>    begin an HV sweep (b = 1 or 2)",
        "  pause | resume | stop           control a running sweep",
        "  laser on|off                    laser independent of any sweep",
        "  clear                           discard collected points",
        "  save <file>                     write collected points as CSV (on this machine)",
        "  status                          one-line snapshot",
        "  quit                            end this session (stdin: exit the program)",
    };
}

// Processes one command line and returns its reply (to the caller only — not
// broadcast). Sets quitRequested if the caller asked to end its session.
static QStringList dispatch(WMCore &core, const QString &line, bool &quitRequested)
{
    quitRequested = false;
    QStringList tok = line.split(' ', Qt::SkipEmptyParts);
    QString cmd = tok.isEmpty() ? QString() : tok.takeFirst().toLower();

    if (cmd == "quit" || cmd == "exit") {
        quitRequested = true;
        return {"OK bye"};
    }
    if (cmd == "help")
        return helpLines();
    if (cmd == "ports")
        return {core.cmdPorts()};
    if (cmd == "connect")
        return {core.cmdConnect(tok.value(0))};
    if (cmd == "start" && tok.size() == 4)
        return {core.cmdStart(tok[0].toInt(), tok[1].toInt(), tok[2].toInt(), tok[3].toInt())};
    if (cmd == "pause")
        return {core.cmdPause()};
    if (cmd == "resume")
        return {core.cmdResume()};
    if (cmd == "stop")
        return {core.cmdStop()};
    if (cmd == "laser" && tok.size() == 1)
        return {core.cmdLaser(tok[0].toLower() == "on")};
    if (cmd == "clear")
        return {core.cmdClear()};
    if (cmd == "save" && tok.size() == 1)
        return {core.cmdSave(tok[0])};
    if (cmd == "status")
        return {core.cmdStatus()};
    return {"ERROR unknown command: '" + line + "' (try 'help')"};
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QStringList args = QCoreApplication::arguments();
    const bool simulate = args.contains("-sim") || args.contains("--sim");

    quint16 port = kDefaultPort;
    int portArgIdx = args.indexOf("-port");
    if (portArgIdx >= 0 && portArgIdx + 1 < args.size())
        port = quint16(args.at(portArgIdx + 1).toUInt());

    WMCore core(simulate);

    // Every connected transport (stdout + each TCP client) hears LOG/POINT/DONE
    // as they happen — replies to a specific command go only to whoever asked.
    QList<QTcpSocket *> clients;
    auto broadcast = [&](const QString &s) {
        printLine(s);
        for (QTcpSocket *sock : clients)
            writeLine(sock, s);
    };

    QObject::connect(&core, &WMCore::log, [&](const QString &m) {
        broadcast("LOG " + m);
    });
    QObject::connect(&core, &WMCore::pointCollected,
        [&](int idx, double cur, double avgMw, double peakMw, int hv) {
            broadcast(QString("POINT idx=%1 hv=%2 current=%3 power_avg=%4 power_peak=%5")
                .arg(idx).arg(hv)
                .arg(cur, 0, 'f', 4).arg(avgMw, 0, 'f', 3).arg(peakMw, 0, 'f', 1));
        });
    QObject::connect(&core, &WMCore::sweepStopped,
        [&](const QString &reason, int total) {
            broadcast(QString("DONE reason=\"%1\" points=%2").arg(reason).arg(total));
        });
    QObject::connect(&core, &WMCore::deviceStatus, [&](const QString &device, bool ok) {
        broadcast(QString("DEVICE name=%1 ok=%2").arg(device).arg(ok ? 1 : 0));
    });

    printLine(QString("READY wm_headless%1 — TCP port %2")
        .arg(simulate ? " [SIMULATION]" : "").arg(port));

    // ── stdin transport (local console / SSH) ───────────────────────────────
    // Only wired up when stdin is an actual terminal. Launched non-interactively
    // (a script, a background job, systemd — the realistic Pi deployment case),
    // stdin is at EOF from the start; without this guard the notifier would fire
    // immediately and quit the whole server the instant it started.
    if (isatty(fileno(stdin))) {
        auto *notifier = new QSocketNotifier(fileno(stdin), QSocketNotifier::Read, &app);
        QObject::connect(notifier, &QSocketNotifier::activated, [&]() {
            char buf[512];
            if (!fgets(buf, sizeof(buf), stdin)) {
                QCoreApplication::quit();
                return;
            }
            QString line = QString::fromLocal8Bit(buf).trimmed();
            if (line.isEmpty())
                return;

            bool quit = false;
            for (const QString &out : dispatch(core, line, quit))
                printLine(out);
            if (quit)
                QCoreApplication::quit();   // stdin "quit" ends the whole server
        });
    } else {
        printLine("LOG stdin is not a terminal — stdin commands disabled, TCP only");
    }

    // ── TCP transport (remote client, e.g. the PC GUI) ──────────────────────
    QTcpServer server;
    QHash<QTcpSocket *, QByteArray> pending;   // per-client partial-line buffer

    QObject::connect(&server, &QTcpServer::newConnection, [&]() {
        while (QTcpSocket *sock = server.nextPendingConnection()) {
            clients << sock;
            writeLine(sock, QString("READY wm_headless%1").arg(simulate ? " [SIMULATION]" : ""));
            broadcast("LOG client connected from " + sock->peerAddress().toString());

            QObject::connect(sock, &QTcpSocket::readyRead, [&, sock]() {
                pending[sock] += sock->readAll();
                while (true) {
                    int nl = pending[sock].indexOf('\n');
                    if (nl < 0) break;
                    QString line = QString::fromUtf8(pending[sock].left(nl)).trimmed();
                    pending[sock].remove(0, nl + 1);
                    if (line.isEmpty()) continue;

                    bool quit = false;
                    for (const QString &out : dispatch(core, line, quit))
                        writeLine(sock, out);
                    if (quit)
                        sock->disconnectFromHost();   // TCP "quit" only ends that client
                }
            });
            QObject::connect(sock, &QTcpSocket::disconnected, [&, sock]() {
                clients.removeAll(sock);
                pending.remove(sock);
                broadcast("LOG client disconnected: " + sock->peerAddress().toString());
                sock->deleteLater();
            });
        }
    });

    if (!server.listen(QHostAddress::Any, port)) {
        printLine(QString("ERROR cannot listen on TCP port %1: %2")
            .arg(port).arg(server.errorString()));
        return 1;
    }

    return app.exec();
}
