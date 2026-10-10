import QtQuick
import QtQuick.Shapes
import MobsCore

// The opening screen. ONE clock (t, in ms) drives every element, so a frame is a pure function of time: the MOBS mark
// builds itself (viewfinder corners snap in, the record ring draws round, the dot goes live, the webcam lens opens), the
// name decodes letter by letter, and a short readout reports what this machine actually has. Any key or click skips it;
// a recording that starts (hotkey) dismisses it at once. Off switch: EXPERT > recording > opening animation.
// Every graphic carries a fact; the live Omarchy theme supplies every colour.
Item {
    id: splash
    objectName: "splash"
    anchors.fill: parent
    z: 200
    visible: !done
    opacity: 1 - fade

    property real t: 0          // ms since the show began (frozen by MOBS_SPLASH_AT for design checks)
    property real fade: 0       // 0 showing .. 1 gone
    property bool done: false
    readonly property real total: 2800
    readonly property real size: Math.max(150, Math.min(250, height * 0.36))   // the emblem's edge
    readonly property real unit: size / 256                                    // the emblem is drawn on a 256 grid

    function seg(a, b) { return Math.max(0, Math.min(1, (t - a) / (b - a))) }
    function outCubic(x) { return 1 - Math.pow(1 - x, 3) }
    function outBack(x) { const c = 1.70158; return 1 + (c + 1) * Math.pow(x - 1, 3) + c * Math.pow(x - 1, 2) }
    function outElastic(x) { return x <= 0 ? 0 : x >= 1 ? 1 : Math.pow(2, -10 * x) * Math.sin((x * 10 - 0.75) * (2 * Math.PI / 3)) + 1 }
    function skip(quick) {
        if (done || out.running) return
        out.duration = quick ? 140 : 280
        out.start()
    }

    NumberAnimation { id: out; target: splash; property: "fade"; from: 0; to: 1; easing.type: Easing.InOutQuad; onFinished: splash.done = true }
    NumberAnimation { id: clock; target: splash; property: "t"; from: 0; to: splash.total; duration: splash.total; onFinished: splash.skip() }
    Component.onCompleted: {
        // Decided once, at launch: a switch flipped later never makes it appear mid-session.
        if (!splashAllowed || !Prefs.splash || Engine.active) { done = true; return }
        if (splashAt >= 0) t = splashAt
        else clock.start()
    }
    Connections { target: Engine; function onStateChanged() { if (Engine.active) splash.skip(true) } }
    MouseArea { anchors.fill: parent; onClicked: splash.skip() }

    // ---- what the readout says (all of it real) ----
    readonly property string engineText: {
        const m = /libobs (\S+)/.exec(Prefs.aboutText)
        return (m ? "libobs " + m[1] : "libobs") + ", built in"
    }
    function tilde(path) { return path.replace(/^\/home\/[^/]+/, "~") }
    readonly property var rows: {
        const cams = App.cameras, mics = Audio.microphones
        let camText = "none found, screen only"
        if (cams.length === 1) camText = cams[0].name
        else if (cams.length > 1) camText = cams[0].name + " + " + (cams.length - 1) + " more"
        let micText = "off"
        let micLevel = "ok"
        if (Prefs.micOn) {
            micText = mics.length > 0 ? Audio.name(Prefs.micDevice) : "no input found"
            micLevel = mics.length > 0 ? "ok" : "warn"
        }
        return [
            { label: "engine", value: engineText, level: "ok" },
            { label: "display", value: App.displayText.trim(), level: App.displayText === "no display" ? "warn" : "ok" },
            { label: "camera", value: camText, level: cams.length > 0 ? "ok" : "none" },
            { label: "mic", value: micText, level: micLevel },
            { label: "saves to", value: tilde(Prefs.folder), level: "ok" }
        ]
    }

    // ---- the stage ----
    Rectangle { anchors.fill: parent; color: Omarchy.darkerBackground }
    Shape {
        anchors.fill: parent
        opacity: splash.seg(60, 900)
        ShapePath {
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: splash.width / 2; centerY: stack.y + splash.size / 2; centerRadius: Math.max(splash.width, splash.height) * 0.6
                focalX: centerX; focalY: centerY
                GradientStop { position: 0; color: Qt.alpha(Omarchy.accent, 0.20) }
                GradientStop { position: 0.4; color: Qt.alpha(Omarchy.accent, 0.06) }
                GradientStop { position: 1; color: "transparent" }
            }
            PathSvg { path: "M0 0 H" + splash.width + " V" + splash.height + " H0 Z" }
        }
    }
    Repeater {   // scanlines
        model: Math.ceil(splash.height / 4)
        Rectangle { y: index * 4; width: splash.width; height: 1; color: Omarchy.foreground; opacity: 0.035 }
    }
    Item {   // one bright sweep, top to bottom, while the mark builds
        id: sweep
        readonly property real p: splash.seg(120, 1350)
        visible: p > 0 && p < 1
        y: -height + (splash.height + height) * (p < 0.5 ? 2 * p * p : 1 - Math.pow(-2 * p + 2, 2) / 2)
        width: splash.width; height: 120
        Rectangle { anchors.fill: parent; gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: 1; color: Qt.alpha(Omarchy.accent, 0.13) } } }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: Qt.alpha(Omarchy.accent, 0.55) }
    }

    Column {
        id: stack
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.max(16, (splash.height - height) / 2 - 14)
        spacing: Math.round(splash.size * 0.10)
        scale: 1 + 0.05 * splash.fade

        // ---- the mark, drawn on a 256 grid ----
        Item {
            id: emblem
            width: splash.size; height: splash.size
            anchors.horizontalCenter: parent.horizontalCenter
            Item {
                id: art
                width: 256; height: 256
                scale: splash.unit; transformOrigin: Item.TopLeft

                // viewfinder corners (the fourth belongs to the webcam lens)
                Repeater {
                    model: [
                        { path: "M44 80 V60 Q44 44 60 44 H80", dx: -34, dy: -34, from: 120, to: 580 },
                        { path: "M176 44 H196 Q212 44 212 60 V80", dx: 34, dy: -34, from: 200, to: 660 },
                        { path: "M44 176 V196 Q44 212 60 212 H80", dx: -34, dy: 34, from: 280, to: 740 }
                    ]
                    Shape {
                        id: corner
                        required property var modelData
                        width: 256; height: 256
                        preferredRendererType: Shape.CurveRenderer
                        readonly property real p: splash.outBack(splash.seg(modelData.from, modelData.to))
                        opacity: splash.seg(modelData.from, modelData.from + 160)
                        transform: Translate { x: (1 - corner.p) * corner.modelData.dx; y: (1 - corner.p) * corner.modelData.dy }
                        ShapePath {
                            strokeColor: Omarchy.accent; strokeWidth: 8; fillColor: "transparent"
                            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
                            PathSvg { path: modelData.path }
                        }
                    }
                }

                // the record ring draws itself round, with a bright head and a soft glow
                Shape {
                    id: ring
                    width: 256; height: 256
                    preferredRendererType: Shape.CurveRenderer
                    readonly property real sweep: Math.min(359.9, 360 * splash.outCubic(splash.seg(300, 1250)))
                    ShapePath { strokeColor: Qt.alpha(Omarchy.accent, 0.07); strokeWidth: 45; fillColor: "transparent"; capStyle: ShapePath.RoundCap
                        PathAngleArc { centerX: 126; centerY: 122; radiusX: 54; radiusY: 54; startAngle: -90; sweepAngle: ring.sweep } }
                    ShapePath { strokeColor: Qt.alpha(Omarchy.accent, 0.12); strokeWidth: 33; fillColor: "transparent"; capStyle: ShapePath.RoundCap
                        PathAngleArc { centerX: 126; centerY: 122; radiusX: 54; radiusY: 54; startAngle: -90; sweepAngle: ring.sweep } }
                    ShapePath { strokeColor: Qt.alpha(Omarchy.accent, 0.22); strokeWidth: 23; fillColor: "transparent"; capStyle: ShapePath.RoundCap
                        PathAngleArc { centerX: 126; centerY: 122; radiusX: 54; radiusY: 54; startAngle: -90; sweepAngle: ring.sweep } }
                    ShapePath { strokeColor: Omarchy.accent; strokeWidth: 15; fillColor: "transparent"; capStyle: ShapePath.RoundCap
                        PathAngleArc { centerX: 126; centerY: 122; radiusX: 54; radiusY: 54; startAngle: -90; sweepAngle: ring.sweep } }
                }
                Item {   // the comet head at the end of the stroke
                    readonly property real a: (-90 + ring.sweep) * Math.PI / 180
                    x: 126 + 54 * Math.cos(a); y: 122 + 54 * Math.sin(a)
                    visible: ring.sweep > 0 && splash.t < 1450
                    opacity: 1 - splash.seg(1250, 1450)
                    Rectangle { x: -22; y: -22; width: 44; height: 44; radius: 22; color: Qt.alpha(Omarchy.accent, 0.28) }
                    Rectangle { x: -10; y: -10; width: 20; height: 20; radius: 10; color: Qt.lighter(Omarchy.accent, 1.7) }
                }

                // the dot goes live: a pop, a shockwave, then it breathes
                Shape {
                    id: shock
                    readonly property real r: 31 + 78 * splash.outCubic(splash.seg(1000, 1700))
                    width: 256; height: 256
                    visible: splash.t > 1000 && splash.t < 1700
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeColor: Qt.alpha(Omarchy.red, 0.6 * (1 - splash.seg(1000, 1700))); strokeWidth: 2.5; fillColor: "transparent"
                        PathAngleArc { centerX: 126; centerY: 122; radiusX: shock.r; radiusY: shock.r; startAngle: 0; sweepAngle: 359.9 }
                    }
                }
                Item {
                    id: dot
                    readonly property real pop: splash.outElastic(splash.seg(1000, 1500))
                    readonly property real breath: splash.t > 1500 ? 1 + 0.05 * Math.sin((splash.t - 1500) / 1100 * 2 * Math.PI) : 1
                    x: 126; y: 122
                    scale: pop * breath
                    visible: pop > 0
                    Rectangle { x: -52; y: -52; width: 104; height: 104; radius: 52; color: Qt.alpha(Omarchy.red, 0.20 * dot.breath) }
                    Rectangle {
                        x: -31; y: -31; width: 62; height: 62; radius: 31
                        gradient: Gradient { GradientStop { position: 0; color: Qt.lighter(Omarchy.red, 1.18) } GradientStop { position: 1; color: Qt.darker(Omarchy.red, 1.12) } }
                    }
                    Rectangle { x: -20; y: -21; width: 20; height: 13; radius: 7; color: "white"; opacity: 0.26 }
                }

                // the webcam lens opens in the corner where MOBS puts your camera, then blinks awake
                Item {
                    id: lens
                    readonly property real pop: splash.outBack(splash.seg(1150, 1500))
                    readonly property real blink: { const b = Math.abs((splash.t - 1900) / 70); return b < 1 ? 1 - b : 0 }
                    x: 192; y: 190
                    scale: pop; opacity: splash.seg(1150, 1300); visible: opacity > 0
                    Shape {
                        preferredRendererType: Shape.CurveRenderer
                        ShapePath {
                            strokeColor: Omarchy.accent; strokeWidth: 8; fillColor: Omarchy.darkerBackground
                            PathAngleArc { centerX: 0; centerY: 0; radiusX: 33; radiusY: 33; startAngle: -90; sweepAngle: 360 }
                        }
                        ShapePath {
                            strokeColor: Qt.alpha(Omarchy.cyan, 0.85); strokeWidth: 3.5; fillColor: Omarchy.background
                            PathAngleArc { centerX: 0; centerY: 0; radiusX: 21.25; radiusY: 21.25; startAngle: -90; sweepAngle: 360 }
                        }
                    }
                    Rectangle {
                        x: -11; y: -11; width: 22; height: 22; radius: 11; color: Omarchy.cyan
                        transform: Scale { origin.x: 11; origin.y: 11; yScale: 1 - 0.9 * lens.blink }
                    }
                    Rectangle { x: -5.5; y: -5.5; width: 8; height: 8; radius: 4; color: "white"; opacity: 0.85 * (1 - 0.9 * lens.blink) }
                }
            }
        }

        // ---- the name decodes ----
        Item {
            id: word
            anchors.horizontalCenter: parent.horizontalCenter
            readonly property real fs: Math.round(splash.size * 0.27)
            readonly property real cell: fs * 0.6 + fs * 0.2
            width: 4 * cell; height: Math.round(fs * 1.25)
            Repeater {
                model: ["M", "O", "B", "S"]
                Txt {
                    required property var modelData
                    required property int index
                    readonly property real begins: 1100 + index * 90
                    readonly property real lands: 1280 + index * 110
                    readonly property string noise: "01<>/\\#%&@$=+"
                    x: index * word.cell; width: word.cell; height: word.height
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    font.pixelSize: word.fs; font.bold: true
                    text: splash.t >= lands ? modelData : splash.t >= begins ? noise.charAt((Math.floor(splash.t / 38) + index * 5) % noise.length) : ""
                    color: splash.t >= lands ? Qt.tint(Omarchy.foreground, Qt.alpha(Omarchy.accent, 1 - splash.seg(lands, lands + 220))) : Qt.alpha(Omarchy.accent, 0.85)
                    Repeater {   // the hit: a cyan and a red ghost slide back into the letter
                        model: [[-1, "cyan"], [1, "red"]]
                        Txt {
                            required property var modelData
                            readonly property real k: 1 - splash.seg(parent.lands, parent.lands + 240)
                            visible: splash.t >= parent.lands && k > 0
                            x: modelData[0] * 7 * k; width: parent.width; height: parent.height
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            font.pixelSize: parent.font.pixelSize; font.bold: true
                            text: parent.modelData; color: Qt.alpha(modelData[1] === "cyan" ? Omarchy.cyan : Omarchy.red, 0.55 * k)
                            z: -1
                        }
                    }
                }
            }
            Rectangle {   // an underline that wipes in under the name
                anchors.bottom: parent.bottom; anchors.bottomMargin: -4
                x: word.cell * 0.1; height: 2; color: Qt.alpha(Omarchy.accent, 0.7)
                width: (word.width - word.cell * 0.2) * splash.outCubic(splash.seg(1500, 1900))
            }
        }

        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: tagline.implicitWidth; height: tagline.implicitHeight
            Txt { id: tagline; text: "SCREEN  ·  CAMERA  ·  ONE APP"; font.pixelSize: 12; font.letterSpacing: 4; opacity: 0 }
            Txt {
                text: tagline.text.substring(0, Math.floor(tagline.text.length * splash.seg(1550, 2000)))
                font.pixelSize: 12; font.letterSpacing: 4; color: Omarchy.secondaryText
            }
        }

        // ---- the readout: what this machine has right now ----
        Item {
            id: readout
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(520, splash.width - 96)
            height: column.height
            Rectangle {   // the rule grows with the rows
                width: 2; color: Qt.alpha(Omarchy.accent, 0.6)
                height: column.height * splash.seg(1650, 1650 + 130 * 5)
            }
            Column {
                id: column
                x: 16; spacing: 4
                Repeater {
                    model: splash.rows
                    Row {
                        required property var modelData
                        required property int index
                        readonly property real p: splash.outCubic(splash.seg(1650 + index * 130, 1650 + index * 130 + 240))
                        spacing: 12; opacity: p; x: (1 - p) * -14
                        Txt { width: 76; text: modelData.label.toUpperCase(); color: Omarchy.tertiaryText; font.pixelSize: 11; font.letterSpacing: 1.5; anchors.verticalCenter: parent.verticalCenter }
                        Txt {
                            width: readout.width - 16 - 76 - 24 - 24; text: modelData.value; elide: Text.ElideRight; font.pixelSize: 13
                            color: modelData.level === "warn" ? Omarchy.yellow : modelData.level === "none" ? Omarchy.secondaryText : Omarchy.foreground
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Txt {
                            width: 24; text: modelData.level === "ok" ? "✓" : modelData.level === "warn" ? "!" : "–"; font.pixelSize: 13; font.bold: true
                            color: modelData.level === "ok" ? Omarchy.green : modelData.level === "warn" ? Omarchy.yellow : Omarchy.tertiaryText
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }

    Row {   // the way out
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 22 }
        spacing: 8; opacity: splash.seg(600, 1000) * 0.9
        Key { text: "any key" }
        Txt { text: "skip"; color: Omarchy.tertiaryText; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
    }
}
