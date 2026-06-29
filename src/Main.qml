import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import QtGraphs

ApplicationWindow {
    id: win
    visible: true
    width: 1024
    height: 600
    minimumWidth: 900
    minimumHeight: 540
    title: "WM — I-P Characterisation"

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
        var xs = (xMax - xMin < 1e-9) ? 1.0 : (xMax - xMin) * 0.05
        var ys = (yMax - yMin < 1e-9) ? 1.0 : (yMax - yMin) * 0.05
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
        anchors.margins: 8
        spacing: 8

        // Top bar: connection controls
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Button {
                text: controller.allConnected ? "Connected" : "Connect"
                enabled: !controller.allConnected
                onClicked: controller.connectDevices(portCombo.currentText)
            }
            ComboBox {
                id: portCombo
                Layout.preferredWidth: 180
                model: controller.ports
            }
            Button {
                text: "↺"
                ToolTip.visible: hovered
                ToolTip.text: "Refresh serial port list"
                onClicked: controller.refreshPorts()
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "clear data"
                onClicked: controller.clearData()
            }
        }

        // Plot + control panel
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

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
                        width: 10; height: 10; radius: 5
                        color: "#1f77b4"
                        border.color: "#10456e"
                        border.width: 1
                    }
                }
            }

            // Control panel
            ColumnLayout {
                Layout.preferredWidth: 240
                Layout.fillHeight: true
                enabled: controller.allConnected
                spacing: 10

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    font.pointSize: 18
                    font.bold: true
                    text: controller.measuring ? "Pause"
                                               : (controller.paused ? "Resume" : "Start")
                    onClicked: controller.startOrPause(sbFrom.value, sbTo.value,
                                                       sbStep.value, sbB.value)
                }
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    font.pointSize: 18
                    font.bold: true
                    text: "Stop"
                    enabled: controller.measuring || controller.paused
                    onClicked: controller.stop()
                }

                // HV sweep parameters
                GroupBox {
                    Layout.fillWidth: true
                    title: "HV sweep (n, b)"
                    GridLayout {
                        anchors.fill: parent
                        columns: 3
                        columnSpacing: 8
                        rowSpacing: 6

                        Label { text: "from" }
                        SpinBox { id: sbFrom; from: 1; to: 4095; value: 1; editable: true
                                  Layout.fillWidth: true }
                        SpinBox { id: sbB; from: 1; to: 2; value: 1 }

                        Label { text: "to" }
                        SpinBox { id: sbTo; from: 1; to: 4095; value: 4095; editable: true
                                  Layout.fillWidth: true }
                        Label { text: "b" }

                        Label { text: "step" }
                        SpinBox { id: sbStep; from: 1; to: 500; value: 100; editable: true
                                  Layout.fillWidth: true }
                        Item {}
                    }
                }

                // Live readings
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 4

                    Label { text: "Current, V:"; font.bold: true }
                    Label { text: controller.currentValue.toFixed(4)
                            font.pointSize: 13; font.bold: true }

                    Label { text: "Power, mW:"; font.bold: true }
                    Label { text: controller.powerMW.toFixed(3)
                            font.pointSize: 13; font.bold: true }
                }

                Label {
                    Layout.fillWidth: true
                    text: controller.hvLabel
                    font.pointSize: 12
                    font.bold: true
                }

                CheckBox {
                    id: laserCheck
                    text: "laser ON"
                    font.bold: true
                    onToggled: controller.setLaser(checked)
                    Component.onCompleted: checked = controller.laserOn
                    Connections {
                        target: controller
                        function onLaserOnChanged() { laserCheck.checked = controller.laserOn }
                    }
                }

                Item { Layout.fillHeight: true }  // spacer

                Label {
                    text: controller.pointCount + " points"
                    color: "#666"
                }
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
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
