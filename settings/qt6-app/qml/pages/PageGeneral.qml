import QtQuick
import QtQuick.Controls.Basic
import XType.Settings 1.0

Item {
    id: page
    anchors.fill: parent

    // Popup options matter when the default style or any app's override is popup.
    readonly property bool popupInUse: Config.ghostStyle === "popup"
        || Object.values(Config.apps).some(a => a && a.ghost_style === "popup")

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: col.implicitHeight + 44
        clip: true

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle { radius: 3; color: Theme.ink12 }
        }

        Column {
            id: col
            x: 28; y: 22
            width: flick.width - 56
            spacing: Theme.spacing.section

            // ── 01 Engine ──────────────────────────────────────────────────
            XSection { title: "Engine"; num: "01"; width: parent.width }

            Column {
                width: parent.width
                spacing: 0
                XRow {
                    label: "Enable XType globally"
                    desc: "Master on/off for ghost-text suggestions"
                    width: parent.width
                    XToggle {
                        on: Config.engineEnabled
                        onToggled: (v) => Config.engineEnabled = v
                    }
                }

                XRow {
                    label: "Trigger delay"
                    desc: "Wait after keystroke before requesting"
                    width: parent.width
                    XSlider {
                        min: 100; max: 800; step: 25
                        value: Config.debounceMs
                        onCommitted: (v) => Config.debounceMs = v
                        width: 200
                        // Custom label formatter (override internal Text with a binding)
                        Component.onCompleted: {}
                    }
                }
                XRow {
                    label: "Max suggestion length"
                    isLast: true
                    width: parent.width
                    XSlider {
                        min: 4; max: 48; step: 1
                        value: Config.numPredict
                        onCommitted: (v) => Config.numPredict = v
                        width: 200
                    }
                }
            }

            // ── 02 Acceptance ──────────────────────────────────────────────
            XSection { title: "Acceptance"; num: "02"; width: parent.width }

            Column {
                width: parent.width
                spacing: 0
                XRow {
                    label: "Accept key"
                    desc: "Key that accepts the full suggestion"
                    width: parent.width
                    Row {
                        spacing: 6
                        XPill {
                            label: "Tab"
                            on: Config.acceptKey === "tab"
                            onClicked: Config.acceptKey = "tab"
                        }
                        XPill {
                            label: "Enter"
                            on: Config.acceptKey === "enter"
                            onClicked: Config.acceptKey = "enter"
                        }
                        XPill {
                            label: "→"
                            on: Config.acceptKey === "right"
                            onClicked: Config.acceptKey = "right"
                        }
                    }
                }
                XRow {
                    label: "Suggestion style"
                    desc: "Inline: in the app's own text (usually underlined). Popup: gray text drawn by Fcitx5 at the caret."
                    width: parent.width
                    Row {
                        spacing: 6
                        XPill {
                            label: "Inline"
                            on: Config.ghostStyle !== "popup"
                            onClicked: Config.ghostStyle = "inline"
                        }
                        XPill {
                            label: "Popup"
                            on: Config.ghostStyle === "popup"
                            onClicked: Config.ghostStyle = "popup"
                        }
                    }
                }
                XRow {
                    visible: page.popupInUse
                    label: "Popup font"
                    desc: "Font of the gray text, e.g. \"Sans 11\". Match your apps' text size."
                    width: parent.width
                    XInput {
                        width: 200
                        placeholderText: "Sans 11"
                        text: Config.ghostPopupFont
                        onEditingFinished: Config.ghostPopupFont = text.trim().length > 0 ? text.trim() : "Sans 11"
                    }
                }
                XRow {
                    visible: page.popupInUse
                    label: "On the caret line (Hyprland)"
                    desc: "Moves the popup onto the line you type. Not every app: only native Wayland apps that report their caret; not X11, Electron/Chromium or Qt apps using Fcitx5's Qt module."
                    width: parent.width
                    XToggle {
                        on: Config.ghostHyprlandInline
                        onToggled: (v) => Config.ghostHyprlandInline = v
                    }
                }
                XRow {
                    label: "Partial accept (word-by-word)"
                    desc: "Accept one word at a time"
                    width: parent.width
                    XToggle {
                        on: Config.partialAccept
                        onToggled: (v) => Config.partialAccept = v
                    }
                }
                XRow {
                    label: "Dismiss on Esc"
                    isLast: true
                    width: parent.width
                    XToggle {
                        on: Config.escDismisses
                        onToggled: (v) => Config.escDismisses = v
                    }
                }
            }

            // ── 03 Live preview ────────────────────────────────────────────
            XSection { title: "Live preview"; num: "03"; width: parent.width }

            XCard {
                width: parent.width
                height: 110

                Text {
                    x: 0; y: 0
                    text: "Type here to test suggestions"
                    font.family: Theme.sansFamily; font.pixelSize: 11
                    font.capitalization: Font.AllUppercase; font.letterSpacing: 1.2
                    color: Theme.ink45
                    renderType: Text.NativeRendering
                }

                ScrollView {
                    id: liveScroll
                    anchors {
                        top: parent.top; topMargin: 22
                        left: parent.left; right: parent.right; bottom: parent.bottom
                    }
                    clip: true
                    enabled: Config.engineEnabled
                    opacity: Config.engineEnabled ? 1.0 : 0.45
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    TextArea {
                        id: liveInput
                        width: liveScroll.availableWidth
                        placeholderText: "Start typing…"
                        font.family: Theme.monoFamily; font.pixelSize: 13
                        color: Theme.ink100
                        wrapMode: TextArea.Wrap
                        background: null
                        renderType: Text.NativeRendering
                    }
                }
            }

            Item { height: 8 }
        }
    }
}
