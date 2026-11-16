pragma ComponentBehavior: Bound
import QtQuick
import XType.Settings 1.0

Item {
    id: sidebar
    implicitWidth: 220

    property string currentPage: ""
    signal pageChanged(pageId: string)

    readonly property var navModel: [
        { id: "general",  label: "General" },
        { id: "person",   label: "Personalisation" },
        { id: "block",    label: "Block list" },
        { id: "apps",     label: "Per-app" },
        { id: "model",    label: "Model" },
        { id: "about",    label: "About" }
    ]

    // Background
    Rectangle { anchors.fill: parent; color: Theme.ink03 }

    // Right border
    Rectangle {
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: 1; color: Theme.ink06
    }

    // ── Brand + nav ────────────────────────────────────────────────────────
    Column {
        id: topCol
        anchors { top: parent.top; left: parent.left; right: parent.right }
        anchors.topMargin: 18
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 2

        // Brand block
        Item {
            width: parent.width
            height: brandRow.implicitHeight + 22

            Row {
                id: brandRow
                x: 10; y: 6
                spacing: 11

                Rectangle {
                    id: brandMark
                    width: 28; height: 28; radius: 7
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#9b88ff" }
                        GradientStop { position: 1.0; color: "#35409d" }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "X"; color: "white"
                        font.family: Theme.sansFamily
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        renderType: Text.NativeRendering
                    }
                }

                Column {
                    anchors.verticalCenter: brandMark.verticalCenter
                    spacing: 1
                    Text {
                        text: "XType"; color: Theme.ink100
                        font.family: Theme.sansFamily
                        font.pixelSize: 15; font.weight: Font.DemiBold
                        renderType: Text.NativeRendering
                    }
                    Text {
                        text: "v0.1.0"; color: Theme.ink45
                        font.family: Theme.monoFamily; font.pixelSize: 11
                        renderType: Text.NativeRendering
                    }
                }
            }
        }

        // Nav items
        Repeater {
            model: sidebar.navModel
            delegate: NavItem {
                required property var modelData
                navId: modelData.id
                navLabel: modelData.label
                active: modelData.id === sidebar.currentPage
                width: parent ? parent.width : 0
                onNavClicked: (id) => sidebar.pageChanged(id)
            }
        }
    }

    // ── Status block ───────────────────────────────────────────────────────
    Column {
        id: statusCol
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        anchors.bottomMargin: 18
        anchors.leftMargin: 23
        anchors.rightMargin: 23
        spacing: 10

        Rectangle { width: parent.width; height: 1; color: Theme.ink06 }

        // Model row
        Row {
            width: parent.width
            spacing: 4
            Text {
                text: "Model"
                font.family: Theme.sansFamily; font.pixelSize: 10
                font.capitalization: Font.AllUppercase
                color: Theme.ink45; font.weight: Font.DemiBold
                renderType: Text.NativeRendering
                anchors.verticalCenter: parent.verticalCenter
            }
            Item { width: parent.width - modelLabel.implicitWidth - modelPill.implicitWidth - 8; height: 1 }
            Rectangle {
                id: modelPill
                radius: 999
                implicitWidth: modelLabel.implicitWidth + 14
                implicitHeight: modelLabel.implicitHeight + 4
                color: "transparent"
                border.color: Qt.rgba(0.655, 0.545, 0.980, 0.18)
                border.width: 1
                Text {
                    id: modelLabel
                    anchors.centerIn: parent
                    text: "Qwen3-1.7B"
                    font.family: Theme.monoFamily; font.pixelSize: 10
                    color: Theme.purpleSoft
                    renderType: Text.NativeRendering
                }
            }
        }

        // Resource rows. GPU/VRAM only appear on machines with an NVIDIA GPU.
        StatRow {
            caption: "CPU"
            fraction: Engine.cpuPct / 100.0
            valueText: Engine.cpuPct + "%"
        }
        StatRow {
            caption: "RAM"
            fraction: Math.min(1.0, Engine.ramGb / 4.0)
            valueText: Engine.ramGb.toFixed(1) + " GB"
        }
        StatRow {
            visible: Engine.gpuAvailable
            caption: "GPU"
            fraction: Engine.gpuPct / 100.0
            valueText: Engine.gpuPct + "%"
        }
        StatRow {
            visible: Engine.gpuAvailable
            caption: "VRAM"
            fraction: Engine.vramTotalGb > 0 ? Math.min(1.0, Engine.vramGb / Engine.vramTotalGb) : 0
            valueText: Engine.vramGb.toFixed(1) + " GB"
        }

        // Engine line
        Row {
            spacing: 8
            XEngineDot { active: Engine.state === "ready"; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: Engine.state === "ready" ? "engine running" : Engine.state
                font.family: Theme.sansFamily; font.pixelSize: 11
                color: Theme.ink65
                renderType: Text.NativeRendering
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // ── Inline components ──────────────────────────────────────────────────
    // Caption and value columns have fixed widths so the bars line up.
    TextMetrics { id: captionMetrics; font.family: Theme.sansFamily; font.pixelSize: 10; font.weight: Font.DemiBold; text: "VRAM" }
    TextMetrics { id: valueMetrics;   font.family: Theme.monoFamily; font.pixelSize: 11; text: "00.0 GB" }

    component StatRow: Row {
        id: statRow
        property string caption: ""
        property real   fraction: 0.0   // 0.0 – 1.0
        property string valueText: ""
        width: parent ? parent.width : 0
        spacing: 8
        Text {
            width: captionMetrics.advanceWidth
            text: statRow.caption
            font.family: Theme.sansFamily; font.pixelSize: 10
            font.capitalization: Font.AllUppercase
            color: Theme.ink45; font.weight: Font.DemiBold
            renderType: Text.NativeRendering
            anchors.verticalCenter: parent.verticalCenter
        }
        XMiniBar {
            value: statRow.fraction
            anchors.verticalCenter: parent.verticalCenter
            width: statRow.width - captionMetrics.advanceWidth - valueMetrics.advanceWidth - 2 * statRow.spacing
        }
        Text {
            width: valueMetrics.advanceWidth
            horizontalAlignment: Text.AlignRight
            text: statRow.valueText
            font.family: Theme.monoFamily; font.pixelSize: 11
            color: Theme.ink80
            renderType: Text.NativeRendering
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    component NavItem: Item {
        id: navRoot
        property string navId: ""
        property string navLabel: ""
        property bool active: false
        signal navClicked(id: string)

        implicitHeight: 36

        // Active left border (declared first so it's behind the bg)
        Rectangle {
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
            width: 2; radius: 1
            color: navRoot.active ? Theme.purple : "transparent"
        }

        // Row background
        Rectangle {
            anchors { fill: parent; leftMargin: 0 }
            radius: Theme.radius.btn
            color: navRoot.active ? Theme.purpleTint
                 : (navMa.containsMouse ? Theme.ink06 : "transparent")
            Behavior on color { ColorAnimation { duration: 180 } }
        }

        // Icon placeholder (Step 5 will replace with proper SVG icons)
        Rectangle {
            id: navIcon
            width: 14; height: 14
            radius: 2
            anchors { left: parent.left; leftMargin: 13; verticalCenter: parent.verticalCenter }
            color: navRoot.active ? Theme.purpleSoft : Theme.ink45
            opacity: navRoot.active ? 1.0 : 0.55
        }

        Text {
            anchors { left: navIcon.right; leftMargin: 10; verticalCenter: parent.verticalCenter }
            text: navRoot.navLabel
            font.family: Theme.sansFamily
            font.pixelSize: 13
            font.weight: navRoot.active ? Font.DemiBold : Font.Medium
            color: navRoot.active ? Theme.purpleSoft : Theme.ink80
            renderType: Text.NativeRendering
        }

        MouseArea {
            id: navMa
            anchors.fill: parent
            hoverEnabled: true
            onClicked: navRoot.navClicked(navRoot.navId)
        }
    }
}
