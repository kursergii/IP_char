import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import QtGraphs

ApplicationWindow {
    id: win
    visible: true
    width: 1100
    height: 680
    minimumWidth: 960
    minimumHeight: 600
    title: "WM — I-P Characterisation" + (controller.simulated ? "  [SIMULATION]" : "")

    // ── Plot auto-range bookkeeping ──────────────────────────────────────────────
    property real xMin: 0
    property real xMax: 1
    property real yMin: 0
    property real yMax: 1

    function addPoint(x, y) {
        ipSeries.append(x, y)
        if (controller.pointCount === 1) {
            xMin = xMax = x
            yMin = yMax = y
        } else {
            xMin = Math.min(xMin, x); xMax = Math.max(xMax, x)
            yMin = Math.min(yMin, y); yMax = Math.max(yMax, y)
        }
        var xs = (xMax - xMin < 1e-9) ? 1.0 : (xMax - xMin) * 0.08
        var ys = (yMax - yMin < 1e-9) ? 1.0 : (yMax - yMin) * 0.08
        axX.min = xMin - xs; axX.max = xMax + xs
        axY.min = yMin - ys; axY.max = yMax + ys
    }

    function resetPlot() {
        ipSeries.replace([])
        xMin = 0; xMax = 1; yMin = 0; yMax = 1
        axX.min = 0; axX.max = 1
        axY.min = 0; axY.max = 1
    }

    Connections {
        target: controller
        function onPointAdded(x, y) { win.addPoint(x, y) }
        function onDataCleared()    { win.resetPlot() }
        function onWarning(title, text) {
            msgDialog.title = title
            msgDialog.text = text
            msgDialog.open()
        }
    }

    // ── Dialogs ──────────────────────────────────────────────────────────────────
    MessageDialog {
        id: msgDialog
        buttons: MessageDialog.Ok
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        nameFilters: ["CSV files (*.csv)", "All files (*)"]
        defaultSuffix: "csv"
        onAccepted: controller.saveData(selectedFile)
    }

    // ── Layout ───────────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // Top bar: connection controls
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: controller.allConnected ? "Connected" : "Connect"
                enabled: !controller.allConnected
                implicitWidth: 110
                onClicked: controller.connectDevices(portCombo.currentText)
            }
            ComboBox {
                id: portCombo
                Layout.preferredWidth: 200
                model: controller.ports
            }
            Button {
                text: "↺"
                implicitWidth: 40
                ToolTip.visible: hovered
                ToolTip.text: "Refresh serial port list"
                onClicked: controller.refreshPorts()
            }

            // Connection status pills
            Row {
                spacing: 6
                Layout.leftMargin: 12
                Repeater {
                    model: [
                        { name: "Pulser", on: controller.pulserConnected },
                        { name: "Osc",    on: controller.oscConnected },
                        { name: "PM",     on: controller.pmConnected }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        width: lbl.implicitWidth + 18
                        height: 26
                        radius: 13
                        color: modelData.on ? "#2e7d32" : "#9e9e9e"
                        Label {
                            id: lbl
                            anchors.centerIn: parent
                            text: modelData.name
                            color: "white"
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "Clear data"
                onClicked: controller.clearData()
            }
        }

        // Plot + control panel
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            GraphsView {
                id: graph
                Layout.fillWidth: true
                Layout.fillHeight: true

                axisX: ValueAxis { id: axX; min: 0; max: 1; titleText: "Current, V" }
                axisY: ValueAxis { id: axY; min: 0; max: 1; titleText: "Power, mW" }

                ScatterSeries {
                    id: ipSeries
                    color: "#1f77b4"
                    pointDelegate: Rectangle {
                        width: 11; height: 11; radius: 6
                        color: "#1f77b4"
                        border.color: "#10456e"
                        border.width: 1
                    }
                }
            }

            // ── Control panel ────────────────────────────────────────────────────
            Frame {
                Layout.preferredWidth: 290
                Layout.fillHeight: true
                enabled: controller.allConnected

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 14

                    // Run / Stop
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60
                            font.pointSize: 16
                            font.bold: true
                            text: controller.measuring ? "Pause"
                                       : (controller.paused ? "Resume" : "Start")
                            highlighted: true
                            onClicked: controller.startOrPause(sbFrom.value, sbTo.value,
                                                               sbStep.value, sbB.value)
                        }
                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60
                            font.pointSize: 16
                            font.bold: true
                            text: "Stop"
                            enabled: controller.measuring || controller.paused
                            onClicked: controller.stop()
                        }
                    }

                    // HV sweep parameters
                    GroupBox {
                        Layout.fillWidth: true
                        title: "HV sweep"

                        GridLayout {
                            anchors.fill: parent
                            columns: 3
                            columnSpacing: 12
                            rowSpacing: 10

                            // header row
                            Item { Layout.preferredWidth: 42 }
                            Label { text: "n"; font.bold: true; Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter }
                            Label { text: "b"; font.bold: true
                                    horizontalAlignment: Text.AlignHCenter }

                            Label { text: "from" }
                            SpinBox { id: sbFrom; from: 1; to: 4095; value: 1
                                      editable: true; Layout.fillWidth: true }
                            SpinBox { id: sbB; from: 1; to: 2; value: 1
                                      Layout.rowSpan: 3; Layout.alignment: Qt.AlignTop }

                            Label { text: "to" }
                            SpinBox { id: sbTo; from: 1; to: 4095; value: 4095
                                      editable: true; Layout.fillWidth: true }

                            Label { text: "step" }
                            SpinBox { id: sbStep; from: 1; to: 500; value: 100
                                      editable: true; Layout.fillWidth: true }
                        }
                    }

                    // Live readings
                    GroupBox {
                        Layout.fillWidth: true
                        title: "Readings"

                        GridLayout {
                            anchors.fill: parent
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8

                            Label { text: "Current, V" }
                            Label { Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignRight
                                    text: controller.currentValue.toFixed(4)
                                    font.pointSize: 14; font.bold: true }

                            Label { text: "Power, mW" }
                            Label { Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignRight
                                    text: controller.powerMW.toFixed(3)
                                    font.pointSize: 14; font.bold: true }

                            Label { text: "HV"; }
                            Label { Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignRight
                                    text: controller.hvLabel.replace("HV: ", "")
                                    font.bold: true }
                        }
                    }

                    CheckBox {
                        id: laserCheck
                        text: "Laser ON"
                        font.bold: true
                        onToggled: controller.setLaser(checked)
                        Component.onCompleted: checked = controller.laserOn
                        Connections {
                            target: controller
                            function onLaserOnChanged() { laserCheck.checked = controller.laserOn }
                        }
                    }

                    Item { Layout.fillHeight: true }  // spacer pushes Save to bottom

                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: controller.pointCount + " points collected"
                        color: "#888"
                    }
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        font.pointSize: 14
                        font.bold: true
                        text: "Save data"
                        onClicked: {
                            saveDialog.selectedFile =
                                saveDialog.currentFolder + "/" + controller.suggestedFileName()
                            saveDialog.open()
                        }
                    }
                }
            }
        }
    }
}
