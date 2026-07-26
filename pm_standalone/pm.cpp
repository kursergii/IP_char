// ─────────────────────────────────────────────────────────────────────────────
// pm — bulletproof standalone reader for the Thorlabs PM101A power meter.
//
// The PM101A's USBTMC firmware is flaky on Linux: it can power up wedged and
// sometimes ignores transfers. This tool connects "in any case" by escalating
// through every recovery we found:
//     1. open + *IDN?
//     2. on failure: USB bus reset (libusb_reset_device) + retry  (often clears it)
//     3. still failing: VBUS power-cycle the meter's hub port via uhubctl + retry
// It loops until the meter answers, then streams MEAS:POW? once per interval.
//
// No Qt — just libusb-1.0. uhubctl is optional (only used as last-resort recovery);
// it needs passwordless sudo (see build.sh deps).
//
// Build:  g++ -O2 -o pm pm.cpp $(pkg-config --cflags --libs libusb-1.0)
// Run:    ./pm            (Ctrl-C to stop)
// ─────────────────────────────────────────────────────────────────────────────
#include <libusb-1.0/libusb.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <csignal>
#include <unistd.h>

static const uint16_t VID = 0x1313;   // Thorlabs

// ── Device state ─────────────────────────────────────────────────────────────
static libusb_context       *ctx    = nullptr;
static libusb_device_handle *handle = nullptr;
static int      g_iface   = 0;
static uint8_t  g_ep_out  = 0, g_ep_in = 0;
static int      g_max_in  = 64;
static uint8_t  g_btag    = 0;
static volatile sig_atomic_t g_stop = 0;

// Cached hub location + port of the meter (so we can power-cycle it even when it
// has momentarily dropped off the bus and uhubctl can no longer see it).
static char g_hub[32] = {0};
static int  g_port    = 0;

static void on_sig(int) { g_stop = 1; }

// ── Find + open the USBTMC interface (class 0xFE, subclass 3) ────────────────
static bool open_meter()
{
    libusb_device **list = nullptr;
    ssize_t n = libusb_get_device_list(ctx, &list);
    libusb_device *target = nullptr;

    for (ssize_t i = 0; i < n; ++i) {
        libusb_device_descriptor d;
        if (libusb_get_device_descriptor(list[i], &d) != 0 || d.idVendor != VID)
            continue;
        libusb_config_descriptor *cfg = nullptr;
        if (libusb_get_active_config_descriptor(list[i], &cfg) != 0) continue;
        bool ok = false;
        for (int ii = 0; ii < cfg->bNumInterfaces && !ok; ++ii)
            for (int a = 0; a < cfg->interface[ii].num_altsetting; ++a) {
                const libusb_interface_descriptor &id = cfg->interface[ii].altsetting[a];
                if (id.bInterfaceClass != 0xFE || id.bInterfaceSubClass != 3) continue;
                g_iface = id.bInterfaceNumber; g_ep_in = g_ep_out = 0;
                for (int e = 0; e < id.bNumEndpoints; ++e) {
                    const libusb_endpoint_descriptor &ep = id.endpoint[e];
                    if ((ep.bmAttributes & 3) != LIBUSB_TRANSFER_TYPE_BULK) continue;
                    if (ep.bEndpointAddress & 0x80) { g_ep_in = ep.bEndpointAddress; g_max_in = ep.wMaxPacketSize; }
                    else                              g_ep_out = ep.bEndpointAddress;
                }
                ok = true; break;
            }
        libusb_free_config_descriptor(cfg);
        if (ok && g_ep_in && g_ep_out) { target = list[i]; break; }
    }
    if (!target) { if (list) libusb_free_device_list(list, 1); return false; }

    int r = libusb_open(target, &handle);
    libusb_free_device_list(list, 1);
    if (r != 0) { handle = nullptr; return false; }

    if (libusb_kernel_driver_active(handle, g_iface) == 1)
        libusb_detach_kernel_driver(handle, g_iface);
    if (libusb_claim_interface(handle, g_iface) != 0) {
        libusb_close(handle); handle = nullptr; return false;
    }
    return true;
}

static void close_meter()
{
    if (handle) {
        libusb_release_interface(handle, g_iface);
        libusb_close(handle);
        handle = nullptr;
    }
}

// ── USBTMC bulk protocol ─────────────────────────────────────────────────────
static void put32(uint8_t *p, uint32_t v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }

static bool tmc_write(const std::string &cmd)
{
    std::string msg = cmd + "\n";
    g_btag = (g_btag % 255) + 1;
    uint8_t hdr[12] = { 1, g_btag, uint8_t(~g_btag), 0, 0,0,0,0, 1, 0,0,0 };
    put32(hdr + 4, uint32_t(msg.size()));
    std::string pkt(reinterpret_cast<char*>(hdr), 12);
    pkt += msg;
    while (pkt.size() % 4) pkt.push_back('\0');
    int sent = 0;
    return libusb_bulk_transfer(handle, g_ep_out, (unsigned char*)pkt.data(),
                                int(pkt.size()), &sent, 1000) == 0;
}

static std::string tmc_query(const std::string &cmd)
{
    if (!tmc_write(cmd)) return {};
    const uint32_t maxlen = 1024;
    g_btag = (g_btag % 255) + 1;
    uint8_t req[12] = { 2, g_btag, uint8_t(~g_btag), 0, 0,0,0,0, 0, 0,0,0 };
    put32(req + 4, maxlen);
    int sent = 0;
    if (libusb_bulk_transfer(handle, g_ep_out, req, 12, &sent, 1000) != 0) return {};
    int chunk = ((int(maxlen) + 12) / g_max_in + 1) * g_max_in;
    std::string buf(chunk, '\0');
    int got = 0;
    if (libusb_bulk_transfer(handle, g_ep_in, (unsigned char*)buf.data(), chunk, &got, 2000) != 0
        || got < 12) return {};
    uint32_t sz = uint8_t(buf[4]) | (uint8_t(buf[5])<<8) | (uint8_t(buf[6])<<16) | (uint32_t(uint8_t(buf[7]))<<24);
    if (sz > uint32_t(got - 12)) sz = got - 12;
    std::string r = buf.substr(12, sz);
    while (!r.empty() && (r.back()=='\n' || r.back()=='\r' || r.back()==' ')) r.pop_back();
    return r;
}

// Update the cached hub/port from uhubctl if the meter is currently visible.
static bool detect_port()
{
    FILE *f = popen("sudo -n uhubctl 2>/dev/null | awk "
                    "'/Current status for hub/{h=$5} index($0,\"1313:\"){gsub(/:/,\"\",$2);print h,$2;exit}'",
                    "r");
    char buf[64] = {0}, hub[32] = {0}; int port = 0;
    if (f) { if (fgets(buf, sizeof buf, f) == nullptr) buf[0] = 0; pclose(f); }
    if (sscanf(buf, "%31s %d", hub, &port) == 2) {
        snprintf(g_hub, sizeof g_hub, "%s", hub);
        g_port = port;
    }
    return g_port != 0;
}

// Make sure the meter's port is powered ON (used at exit so an interrupted
// cycle never leaves it stuck off).
static void ensure_port_on()
{
    if (g_port == 0) return;
    char cmd[160];
    snprintf(cmd, sizeof cmd, "sudo -n uhubctl -a on -l %s -p %d >/dev/null 2>&1", g_hub, g_port);
    system(cmd);
}

// USBTMC ABORT_BULK_IN recovery (USBTMC 1.00 §4.2.1.4/4.2.1.5). Clears a stuck
// Bulk-IN transfer — the "SPLIT_IN_PROGRESS" wedge. This is what pyvisa-py does
// on a read error, and it un-wedges the PM101A without a power-cycle.
//   bmRequestType 0xA2 = device-to-host | class | ENDPOINT recipient
static void tmc_abort_in()
{
    if (!handle) return;
    unsigned char st[2] = {0};
    int n = libusb_control_transfer(handle, 0xA2, 3 /*INITIATE_ABORT_BULK_IN*/,
                                    g_btag, g_ep_in, st, 2, 3000);
    if (n < 1 || st[0] != 1 /*STATUS_SUCCESS*/) return;

    unsigned char drain[512]; int got = 0;                 // drain leftover data
    libusb_bulk_transfer(handle, g_ep_in, drain, g_max_in, &got, 500);

    for (int i = 0; i < 100; ++i) {                        // CHECK_ABORT_BULK_IN_STATUS
        unsigned char cs[8] = {0};
        libusb_control_transfer(handle, 0xA2, 4, 0x0000, g_ep_in, cs, 8, 3000);
        if (cs[0] != 2 /*STATUS_PENDING*/) break;
        usleep(50 * 1000);
    }
}

// ── Recovery: VBUS power-cycle the meter's hub port via uhubctl ──────────────
static bool power_cycle()
{
    detect_port();                 // refresh cache while the meter is visible
    if (g_port == 0) {
        printf("  [recover] meter's hub/port not known (plug it into a ppps hub once; "
               "check 'sudo uhubctl' and passwordless sudo)\n");
        return false;
    }
    char cmd[160];
    snprintf(cmd, sizeof cmd, "sudo -n uhubctl -a cycle -l %s -p %d -d 2 >/dev/null 2>&1", g_hub, g_port);
    printf("  [recover] VBUS power-cycle: hub %s port %d\n", g_hub, g_port);
    system(cmd);
    sleep(6);   // let the meter re-enumerate before we try to talk to it
    return true;
}

// Try to open + identify the meter once. Returns true (and leaves it open) on
// success; closes it again on failure.
static bool try_connect_once()
{
    if (!open_meter()) return false;

    // Prime the meter exactly like pyvisa-py does: issue the USBTMC/USB488 class
    // control requests GET_CAPABILITIES then REN_CONTROL *before* any bulk query.
    // The PM101A firmware needs these — without them it silently ignores bulk
    // transfers. (This is why it "only works after running the pyvisa test".)
    //   bmRequestType 0xA1 = device-to-host | class | interface recipient
    unsigned char buf[24] = {0};
    libusb_control_transfer(handle, 0xA1,   7, 0x0000, g_iface, buf, 24, 1000); // GET_CAPABILITIES
    libusb_control_transfer(handle, 0xA1, 160, 0x0001, g_iface, buf,  1, 1000); // REN_CONTROL (enable)

    // Try to identify; if it's wedged, run the USBTMC abort recovery and retry
    // a couple of times before giving up (this is what un-wedges the PM101A).
    for (int i = 0; i < 3; ++i) {
        std::string idn = tmc_query("*IDN?");
        if (idn.size() >= 8 && (idn.compare(0,8,"THORLABS")==0 || idn.compare(0,8,"Thorlabs")==0)) {
            printf("connected: %s\n", idn.c_str());
            return true;
        }
        tmc_abort_in();       // clear the stuck Bulk-IN, then retry
        usleep(100 * 1000);
    }
    close_meter();
    return false;
}

// ── Connect "in any case" ────────────────────────────────────────────────────
// Fast path: if the meter is already clean, connect immediately. Otherwise the
// only recovery that reliably works on the PM101A is a VBUS power-cycle of its
// hub port (a USB bus reset actually RE-wedges it, so we never use one here).
static bool connect_in_any_case()
{
    for (int i = 0; i < 3 && !g_stop; ++i) {          // fast path
        if (try_connect_once()) return true;
        usleep(300 * 1000);
    }
    for (int round = 1; round <= 10 && !g_stop; ++round) {  // recovery
        printf("  meter not responding -> VBUS power-cycle (round %d)\n", round);
        if (!power_cycle())
            usleep(2000 * 1000);   // no ppps hub / no sudo: just wait and retry
        for (int i = 0; i < 6 && !g_stop; ++i) {
            if (try_connect_once()) return true;
            usleep(1500 * 1000);
        }
    }
    return false;
}

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);   // unbuffered: readings show live even when piped
    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);
    if (libusb_init(&ctx) != 0) { fprintf(stderr, "libusb_init failed\n"); return 1; }

    detect_port();   // cache the meter's hub/port up front, while it's visible
    printf("pm: connecting to Thorlabs power meter (in any case)...\n");
    if (!connect_in_any_case()) {
        fprintf(stderr, "pm: could not connect after all recovery attempts\n");
        close_meter(); libusb_exit(ctx); return 1;
    }

    tmc_write("SENS:POW:RANG:AUTO ON");
    tmc_write("SENS:POW:UNIT W");
    tmc_write("SENS:CORR:WAV 1550");
    tmc_write("SENS:AVER:COUN 100");
    printf("pm: streaming power (Ctrl-C to stop)\n");

    int bad = 0;
    while (!g_stop) {
        std::string r = tmc_query("MEAS:POW?");
        char *end = nullptr;
        double w = r.empty() ? 0 : strtod(r.c_str(), &end);
        if (!r.empty() && end != r.c_str()) {
            printf("  %.4f mW\n", w * 1000.0);
            bad = 0;
        } else {
            tmc_abort_in();                 // stuck bulk-in — clear it in place
            if (++bad >= 3) {
                printf("pm: lost the meter -> reconnecting...\n");
                close_meter();
                if (!connect_in_any_case()) break;
                tmc_write("SENS:POW:UNIT W");
                bad = 0;
            }
        }
        usleep(500 * 1000);
    }

    printf("\npm: stopping\n");
    close_meter();
    ensure_port_on();   // never leave the meter's port powered off
    libusb_exit(ctx);
    return 0;
}
