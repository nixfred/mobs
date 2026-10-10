import QtQuick

// MOBS screen design prototype. SAMPLE DATA ONLY: it records nothing and talks to nothing.
// Colours are the live Omarchy "Liftoff" theme (colors.toml on gus, 2026-10-09); the real app
// reads colors.toml and Hyprland's rounding live, like Monologue's theme.cpp does.
// Screens: 1 choose, 2 ready, 3 recording, 4 review (Monologue's real EditBar.qml), 5 ready + Pro drawer.
Window {
    id: win
    visible: true
    width: 1180
    height: 720
    title: "mobs (design prototype)"
    color: bg

    // Omarchy tokens
    readonly property color bg: "#151E26"
    readonly property color bgDark: "#10171E"
    readonly property color bgDarker: "#0B1117"
    readonly property color bgLight: "#253440"
    readonly property color selection: "#334D5D"
    readonly property color fg: "#ECE5D8"
    readonly property color fgDim: "#A2ACAE"
    readonly property color muted: "#88979F"
    readonly property color accent: "#F3B77B"
    readonly property color red: "#EF8D80"
    readonly property color green: "#A9C59A"
    readonly property string mono: "JetBrainsMono Nerd Font"
    readonly property int rounding: 24

    property int screen: startScreen
    readonly property bool pro: screen === 5

    // ---- building blocks -------------------------------------------------
    component Txt: Text {
        font.family: win.mono
        color: win.fg
        font.pixelSize: 14
    }
    component Glyph: Text {
        font.family: win.mono
        color: win.accent
        font.pixelSize: 20
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    component Key: Rectangle {
        property alias text: kt.text
        implicitWidth: Math.max(kt.implicitWidth + 16, 26)
        implicitHeight: 24
        radius: 6
        color: "transparent"
        border.color: win.muted
        border.width: 1
        Txt { id: kt; anchors.centerIn: parent; font.pixelSize: 12; color: win.fgDim }
    }
    component Chip: Rectangle {
        property string glyph
        property alias text: ct.text
        property color tint: win.accent
        implicitWidth: cr.implicitWidth + 28
        implicitHeight: 38
        radius: 19
        color: win.bgLight
        border.color: win.selection
        border.width: 1
        Row {
            id: cr
            anchors.centerIn: parent
            spacing: 10
            Glyph { text: parent.parent.glyph; color: parent.parent.tint; font.pixelSize: 16 }
            Txt { id: ct; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 13 }
        }
    }
    component Btn: Rectangle {
        property alias text: bt.text
        property bool primary: false
        implicitWidth: bt.implicitWidth + 44
        implicitHeight: 46
        radius: 23
        color: primary ? win.accent : "transparent"
        border.color: primary ? win.accent : win.selection
        border.width: 2
        Txt { id: bt; anchors.centerIn: parent; font.pixelSize: 15; font.bold: parent.primary; color: parent.primary ? win.bgDark : win.fg }
    }
    component Card: Rectangle {
        property string glyph
        property string title
        property string desc
        property string keyName
        property bool focusRing: false
        width: 252
        height: 306
        radius: win.rounding
        color: win.bgLight
        border.width: focusRing ? 2 : 1
        border.color: focusRing ? win.accent : win.selection
        Column {
            anchors.fill: parent
            anchors.margins: 26
            spacing: 14
            Glyph { text: parent.parent.glyph; font.pixelSize: 54; horizontalAlignment: Text.AlignLeft }
            Item { width: 1; height: 6 }
            Txt { text: parent.parent.title; font.pixelSize: 22; font.bold: true }
            Txt { text: parent.parent.desc; color: win.fgDim; font.pixelSize: 13; width: parent.width; wrapMode: Text.WordWrap }
        }
        Key { text: parent.keyName; anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 22 }
    }

    // ---- header ------------------------------------------------------------
    Item {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 56
        Row {
            anchors { left: parent.left; leftMargin: 28; verticalCenter: parent.verticalCenter }
            spacing: 10
            Glyph { text: ""; font.pixelSize: 14; color: win.accent }
            Txt { text: "mobs"; font.pixelSize: 17; font.bold: true }
            Txt { text: win.screen === 1 ? "new" : win.screen === 3 ? "recording" : win.screen === 4 ? "review" : "ready"; color: win.muted; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
        }
        Row {
            anchors { right: parent.right; rightMargin: 28; verticalCenter: parent.verticalCenter }
            spacing: 18
            Rectangle {
                width: pt.implicitWidth + 24; height: 30; radius: 15
                color: win.pro ? win.accent : "transparent"
                border.color: win.pro ? win.accent : win.selection; border.width: 1
                Txt { id: pt; anchors.centerIn: parent; text: "pro"; font.pixelSize: 13; color: win.pro ? win.bgDark : win.fgDim; font.bold: win.pro }
            }
            Row {
                spacing: 8
                anchors.verticalCenter: parent.verticalCenter
                Txt { text: "full obs"; color: win.fgDim; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                Glyph { text: ""; font.pixelSize: 13; color: win.fgDim }
            }
            Glyph { text: ""; font.pixelSize: 17; color: win.fgDim }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: win.selection; opacity: 0.6 }
    }

    // ---- 1: choose ---------------------------------------------------------
    Item {
        visible: win.screen === 1
        anchors { top: header.bottom; bottom: footer.top; left: parent.left; right: parent.right }
        Column {
            anchors.centerIn: parent
            spacing: 36
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10
                Txt { text: "what are we making?"; font.pixelSize: 34; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
                Txt { text: "pick one. you can change it any time."; color: win.fgDim; font.pixelSize: 15; anchors.horizontalCenter: parent.horizontalCenter }
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 26
                Card { glyph: ""; title: "my screen"; desc: "everything on your screen, with your mic. the one most people want."; keyName: "1"; focusRing: true }
                Card { glyph: ""; title: "screen + me"; desc: "your screen with your camera in the corner. good for walk-throughs."; keyName: "2" }
                Card { glyph: ""; title: "just my camera"; desc: "you and your mic, nothing else. quick messages and takes."; keyName: "3" }
            }
        }
    }

    // ---- 2 and 5: ready (5 adds the Pro drawer) ----------------------------
    Item {
        id: ready
        visible: win.screen === 2 || win.screen === 5
        anchors { top: header.bottom; bottom: footer.top; left: parent.left; right: parent.right; rightMargin: win.pro ? 420 : 0 }
        Column {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -28
            spacing: 22
            Item {
                width: 190; height: 190
                anchors.horizontalCenter: parent.horizontalCenter
                Rectangle { anchors.fill: parent; radius: 95; color: "transparent"; border.color: win.accent; border.width: 4 }
                Rectangle { anchors.centerIn: parent; width: 128; height: 128; radius: 64; color: win.red }
            }
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 6
                Txt { text: "ready to record"; font.pixelSize: 24; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
                Txt { text: "my screen, with mic"; color: win.fgDim; font.pixelSize: 14; anchors.horizontalCenter: parent.horizontalCenter }
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8
                Key { text: "Alt" }
                Key { text: "Shift" }
                Key { text: "V" }
                Txt { text: "starts it from anywhere"; color: win.muted; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter; leftPadding: 8 }
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12
                Chip { glyph: ""; text: "DP-3  5120x2160" }
                Chip {
                    glyph: ""
                    text: "built-in mic"
                }
                Chip { glyph: ""; text: "camera off"; tint: win.muted }
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 4
                Repeater {
                    model: 24
                    Rectangle {
                        required property int index
                        width: 10; height: 6; radius: 2
                        color: index < 14 ? (index < 10 ? win.green : win.accent) : win.selection
                    }
                }
            }
        }
        // recent takes
        Row {
            anchors { bottom: parent.bottom; bottomMargin: 22; horizontalCenter: parent.horizontalCenter }
            spacing: 14
            Repeater {
                model: [["today 15:42", "02:14"], ["today 11:08", "00:47"], ["yesterday", "12:30"]]
                Rectangle {
                    required property var modelData
                    width: 210; height: 52; radius: 16
                    color: win.bgLight; border.color: win.selection; border.width: 1
                    Row {
                        anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                        spacing: 12
                        Glyph { text: ""; font.pixelSize: 14 }
                        Txt { text: modelData[0]; font.pixelSize: 13 }
                        Txt { text: modelData[1]; color: win.muted; font.pixelSize: 13 }
                    }
                }
            }
        }
    }

    // pro drawer (screen 5)
    Rectangle {
        visible: win.pro
        anchors { top: header.bottom; bottom: footer.top; right: parent.right }
        width: 420
        color: win.bgDark
        Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: win.selection }
        Column {
            anchors { fill: parent; margins: 24 }
            spacing: 14
            Row {
                spacing: 24
                Repeater {
                    model: ["sources", "audio", "output", "keys"]
                    Column {
                        required property string modelData
                        required property int index
                        spacing: 6
                        Txt { text: modelData; font.pixelSize: 14; color: index === 0 ? win.accent : win.muted; font.bold: index === 0 }
                        Rectangle { width: parent.width; height: 2; color: index === 0 ? win.accent : "transparent" }
                    }
                }
            }
            Repeater {
                model: [["", "display", "DP-3, 5120x2160"], ["", "microphone", "built-in, -14 dB"], ["", "camera", "off (no device picked)"]]
                Rectangle {
                    required property var modelData
                    required property int index
                    width: parent.width; height: 60; radius: 18
                    color: win.bgLight; border.color: win.selection; border.width: 1
                    Row {
                        anchors { left: parent.left; leftMargin: 18; verticalCenter: parent.verticalCenter }
                        spacing: 14
                        Glyph { text: modelData[0]; width: 26; color: index === 2 ? win.muted : win.accent }
                        Column {
                            spacing: 2
                            Txt { text: modelData[1]; font.pixelSize: 14 }
                            Txt { text: modelData[2]; color: win.muted; font.pixelSize: 12 }
                        }
                    }
                    Glyph { text: index === 2 ? "" : ""; anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 15; color: win.muted }
                }
            }
            Rectangle {
                width: parent.width; height: 44; radius: 22
                color: "transparent"; border.color: win.selection; border.width: 2
                Row { anchors.centerIn: parent; spacing: 10; Glyph { text: ""; font.pixelSize: 13 } Txt { text: "add source"; font.pixelSize: 14 } }
            }
            Txt { text: "layout"; color: win.muted; font.pixelSize: 12 }
            Rectangle {
                width: parent.width; height: 96; radius: 16; color: win.bgDarker; border.color: win.selection; border.width: 1
                Rectangle { anchors { fill: parent; margins: 12 } radius: 8; color: win.selection; opacity: 0.55 }
                Txt { anchors.centerIn: parent; text: "screen fills the frame"; color: win.fgDim; font.pixelSize: 12 }
            }
            Rectangle {
                width: parent.width; height: 48; radius: 24
                color: "transparent"; border.color: win.accent; border.width: 2
                Row { anchors.centerIn: parent; spacing: 10; Txt { text: "open full obs"; color: win.accent; font.pixelSize: 15; font.bold: true } Glyph { text: ""; font.pixelSize: 14 } }
            }
            Txt { text: "scenes, filters, plugins: everything else is there."; color: win.muted; font.pixelSize: 12; width: parent.width; wrapMode: Text.WordWrap }
        }
    }

    // ---- 3: recording ------------------------------------------------------
    Item {
        visible: win.screen === 3
        anchors { top: header.bottom; bottom: footer.top; left: parent.left; right: parent.right }
        Column {
            anchors.centerIn: parent
            spacing: 26
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12
                Rectangle {
                    width: 16; height: 16; radius: 8; color: win.red; anchors.verticalCenter: parent.verticalCenter
                    SequentialAnimation on opacity { loops: Animation.Infinite; NumberAnimation { to: 0.25; duration: 700 } NumberAnimation { to: 1; duration: 700 } }
                }
                Txt { text: "recording"; color: win.red; font.pixelSize: 20; font.bold: true }
            }
            Txt { text: "00:12:34"; font.pixelSize: 112; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
            Txt { text: "my screen, with mic  ·  saving to ~/Videos/mobs"; color: win.muted; font.pixelSize: 13; anchors.horizontalCenter: parent.horizontalCenter }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 26
                Rectangle { width: 88; height: 88; radius: 44; color: "transparent"; border.color: win.selection; border.width: 3
                    Glyph { anchors.centerIn: parent; text: ""; font.pixelSize: 30; color: win.fg } }
                Rectangle { width: 88; height: 88; radius: 44; color: win.accent
                    Glyph { anchors.centerIn: parent; text: ""; font.pixelSize: 30; color: win.bgDark } }
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 30
                Row { spacing: 8; Key { text: "Alt" } Key { text: "Shift" } Key { text: "V" } Txt { text: "stop"; color: win.muted; font.pixelSize: 13; leftPadding: 6; anchors.verticalCenter: parent.verticalCenter } }
                Row { spacing: 8; Key { text: "Ctrl" } Key { text: "Alt" } Key { text: "Shift" } Key { text: "V" } Txt { text: "pause"; color: win.muted; font.pixelSize: 13; leftPadding: 6; anchors.verticalCenter: parent.verticalCenter } }
            }
        }
    }

    // ---- 4: review (Monologue's real, unmodified EditBar.qml) ---------------
    Item {
        visible: win.screen === 4
        anchors { top: header.bottom; bottom: footer.top; left: parent.left; right: parent.right }
        Column {
            anchors.centerIn: parent
            width: 800
            spacing: 18
            Row {
                spacing: 12
                Txt { text: "today 15:42"; font.pixelSize: 14; font.bold: true }
                Txt { text: "2:14 recorded, keeping 1:56"; color: win.muted; font.pixelSize: 13 }
            }
            Rectangle {
                width: parent.width; height: 380; radius: 20; color: win.bgDarker; border.color: win.selection; border.width: 1
                Rectangle {
                    anchors { fill: parent; margins: 1 }
                    radius: 19
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "#1b2a38" }
                        GradientStop { position: 1; color: "#2b3a46" }
                    }
                }
                Glyph { anchors.centerIn: parent; text: ""; font.pixelSize: 64; color: win.fg; opacity: 0.8 }
                Txt { anchors { left: parent.left; bottom: parent.bottom; margins: 16 } text: "sample frame"; color: win.muted; font.pixelSize: 11 }
                Txt { anchors { right: parent.right; bottom: parent.bottom; margins: 16 } text: "00:48 / 02:14"; color: win.fgDim; font.pixelSize: 12 }
            }
            EditBar {
                width: parent.width
                height: 64
                accent: win.accent
                accentForeground: win.bgDark
                cornerRadius: 12
            }
            Row {
                spacing: 14
                Btn { text: "save"; primary: true }
                Btn { text: "save a copy" }
                Item { width: 20; height: 1 }
                Txt { text: "discard"; color: win.muted; anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }

    // ---- footer: key hints, always visible ---------------------------------
    Item {
        id: footer
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 46
        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: win.selection; opacity: 0.6 }
        Row {
            anchors { left: parent.left; leftMargin: 28; verticalCenter: parent.verticalCenter }
            spacing: 22
            Row { spacing: 8; visible: win.screen === 1; Key { text: "1" } Key { text: "2" } Key { text: "3" } Txt { text: "pick"; color: win.muted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: win.screen === 2 || win.screen === 5; Key { text: "Space" } Txt { text: "record"; color: win.muted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: win.screen === 2 || win.screen === 5; Key { text: "P" } Txt { text: "pro"; color: win.muted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: win.screen === 5; Key { text: "Tab" } Txt { text: "next tab"; color: win.muted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: win.screen === 3; Key { text: "Space" } Txt { text: "pause"; color: win.muted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: win.screen === 3; Key { text: "Enter" } Txt { text: "stop"; color: win.muted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: win.screen === 4; Key { text: "Space" } Txt { text: "play"; color: win.muted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: win.screen === 4; Key { text: "S" } Txt { text: "split"; color: win.muted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: win.screen === 4; Key { text: "X" } Txt { text: "cut"; color: win.muted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 8; visible: win.screen === 4; Key { text: "Ctrl" } Key { text: "S" } Txt { text: "save"; color: win.muted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } }
        }
        Txt { anchors { right: parent.right; rightMargin: 28; verticalCenter: parent.verticalCenter } text: "design prototype, sample data"; color: win.muted; font.pixelSize: 11; opacity: 0.7 }
    }

    // live navigation so the prototype can be clicked through by hand
    Item {
        focus: true
        Keys.onPressed: (e) => {
            if (win.screen === 1 && e.key >= Qt.Key_1 && e.key <= Qt.Key_3) win.screen = 2
            else if ((win.screen === 2 || win.screen === 5) && e.key === Qt.Key_Space) win.screen = 3
            else if (win.screen === 3 && (e.key === Qt.Key_Return || e.key === Qt.Key_Enter)) win.screen = 4
            else if ((win.screen === 2 || win.screen === 5) && e.key === Qt.Key_P) win.screen = win.pro ? 2 : 5
            else if (e.key === Qt.Key_Escape) win.screen = win.screen === 4 ? 2 : 1
        }
    }
}
