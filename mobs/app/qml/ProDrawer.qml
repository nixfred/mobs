import QtQuick
import MobsCore

// Focused expert pages use the whole content area, leaving the beginner screens roomy.
Rectangle {
    id: drawer
    objectName: "expertPanel"
    property int tab: 0
    visible: App.pro
    onVisibleChanged: if (visible) tab = App.expertTab
    Connections { target: App; function onProChanged() { if (App.pro && (App.expertTab === 2 || App.expertTab === 6)) drawer.tab = App.expertTab } }
    color: Omarchy.darkBackground
    Column {
        anchors { fill: parent; margins: 24 }
        spacing: 18
        Row {
            width: parent.width
            spacing: 12
            Txt { text: "EXPERT"; font.pixelSize: 20; font.bold: true; color: Omarchy.accent }
            Txt { text: Engine.active ? Engine.state + " " + Engine.elapsedText + " · next take settings" : "your choices are remembered"; color: Omarchy.secondaryText; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
            Button { visible: Engine.state === "recording" || Engine.state === "paused"; enabled: !Engine.broadcasting; text: Engine.state === "paused" ? "resume" : "pause"; onClicked: Engine.togglePause() }
            Button { visible: Engine.active; text: "stop & save"; primary: true; onClicked: Engine.stop() }
        }
        Row {
            spacing: 10
            Repeater {
                model: ["recording", "encoding", "audio", "keys", "about", "hardware", "live"]
                Button {
                    required property string modelData
                    required property int index
                    text: modelData
                    primary: drawer.tab === index
                    onClicked: drawer.tab = index
                }
            }
        }
        Item {
            width: parent.width
            height: parent.height - y
            Row {
                visible: drawer.tab === 0
                width: parent.width
                spacing: 28
                Column {
                    width: (parent.width - parent.spacing) / 2; spacing: 12
                    Txt { text: "picture quality"; font.pixelSize: 16; font.bold: true }
                    Flow {
                        width: parent.width; spacing: 8
                        Repeater {
                            model: [["small", 0], ["balanced", 1], ["high", 2]]
                            Button {
                                required property var modelData
                                text: modelData[0]; primary: !Prefs.customQuality && Prefs.quality === modelData[1]
                                onClicked: Prefs.setQuality(modelData[1])
                            }
                        }
                    }
                    Txt {
                        width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13; color: Omarchy.secondaryText
                        text: Prefs.customQuality ? (Prefs.hardware ? "custom CQP " : "custom CRF ") + Prefs.crf + " (encoding page). Choose a quality above to return to a preset." : ["smaller files for quick demos", "sharp text with a modest file size", "a sharper picture with larger files"][Prefs.quality]
                    }
                    Toggle { label: "open editor after recording"; on: Prefs.reviewAfter; onToggled: Prefs.setReviewAfter(!Prefs.reviewAfter) }
                    Toggle { label: "live preview"; on: App.preview; onToggled: App.setPreview(!App.preview) }
                    Txt { width: parent.width; wrapMode: Text.WordWrap; text: "Preview starts capture and turns on the camera in camera modes. Screen capture may open the system picker."; font.pixelSize: 12; color: Omarchy.tertiaryText }
                }
                Column {
                    width: (parent.width - parent.spacing) / 2; spacing: 12
                    Txt { text: "save recordings in"; font.pixelSize: 16; font.bold: true }
                    Rectangle {
                        width: parent.width; height: 44; radius: 10; color: Omarchy.lighterBackground
                        border.color: pathProblem.text !== "" ? Omarchy.red : Omarchy.selection
                        TextInput {
                            id: pathInput
                            objectName: "folderInput"
                            anchors { fill: parent; margins: 12 }
                            verticalAlignment: TextInput.AlignVCenter
                            font.family: "JetBrainsMono Nerd Font"; font.pixelSize: 13
                            color: Omarchy.foreground; selectionColor: Omarchy.accent; selectedTextColor: Omarchy.accentForeground
                            clip: true; text: Prefs.folder
                            onTextEdited: pathProblem.text = ""
                            Keys.onEscapePressed: { text = Prefs.folder; pathProblem.text = ""; App.setPro(false) }
                            Connections { target: Prefs; function onChanged() { if (!pathInput.activeFocus) pathInput.text = Prefs.folder } }
                        }
                    }
                    Row {
                        spacing: 8
                        Button {
                            objectName: "applyFolder"
                            text: "use folder"
                            onClicked: {
                                pathProblem.text = Prefs.problem(pathInput.text.trim())
                                if (pathProblem.text === "") { Prefs.setFolder(pathInput.text); pathInput.text = Prefs.folder }
                            }
                        }
                        Button { text: "reset"; enabled: !Prefs.folderIsDefault; onClicked: { Prefs.setFolder(""); pathInput.text = Prefs.folder; pathProblem.text = "" } }
                    }
                    Txt { id: pathProblem; width: parent.width; wrapMode: Text.WordWrap; text: ""; color: Omarchy.red; font.pixelSize: 13; visible: text !== "" }
                    Txt { width: parent.width; wrapMode: Text.WordWrap; text: "Use a full path starting with /. Apply with use folder; leaving this page keeps your saved folder."; color: Omarchy.tertiaryText; font.pixelSize: 12 }
                    Button { text: "open saved folder"; onClicked: Prefs.openFolder() }
                    Toggle { label: "play the opening animation"; on: Prefs.splash; onToggled: Prefs.setSplash(!Prefs.splash) }
                }
            }
            Row {
                visible: drawer.tab === 1
                width: parent.width; spacing: 28
                Column {
                    width: (parent.width - parent.spacing) / 2; spacing: 12
                    Txt { text: Prefs.hardware ? "NVIDIA quality (CQP)" : "H.264 quality (CRF)"; font.pixelSize: 16; font.bold: true }
                    Row {
                        spacing: 12
                        Button { text: "−"; enabled: Prefs.crf > 0; onClicked: Prefs.setCrf(Prefs.crf - 1) }
                        Txt { text: Prefs.crf; font.pixelSize: 24; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                        Button { text: "+"; enabled: Prefs.crf < 51; onClicked: Prefs.setCrf(Prefs.crf + 1) }
                    }
                    Txt { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13; color: Omarchy.secondaryText; text: "0–51. Lower means better quality and larger files. Balanced is 23; high is 18. CRF 0 can create very large files." }
                    Button { text: "reset encoding"; onClicked: Prefs.resetEncoding() }
                    Txt { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.tertiaryText; text: "Current output: 1920×1080, 30 fps, " + (Prefs.hardware ? "NVIDIA H.264" : "x264") + " video and AAC audio in MP4. Choose the encoder on the hardware page." }
                }
                Column {
                    width: (parent.width - parent.spacing) / 2; spacing: 12
                    Txt { text: Prefs.hardware ? "NVIDIA encoding" : "x264 CPU preset"; font.pixelSize: 16; font.bold: true }
                    Flow {
                        visible: !Prefs.hardware; width: parent.width; spacing: 8
                        Repeater {
                            model: ["ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"]
                            Button {
                                required property string modelData
                                width: 116; text: modelData; primary: Prefs.encoderPreset === modelData
                                onClicked: Prefs.setEncoderPreset(modelData)
                            }
                        }
                    }
                    Txt { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13; color: Omarchy.secondaryText; text: Prefs.hardware ? "Uses the GPU with quality-based CQP and preset P5. CPU presets apply only to software recording. If the GPU is unavailable, switch back to software on the hardware page." : "Faster presets use less CPU and make larger files. Slower presets compress better but can drop frames on a busy machine. Start with veryfast." }
                }
            }
            AudioPage { objectName: "audioPage"; visible: drawer.tab === 2; anchors.fill: parent }
            Row {
                visible: drawer.tab === 3; width: parent.width; spacing: 28
                Column {
                    width: (parent.width - parent.spacing) / 2; spacing: 14
                    Txt { text: "recording"; font.pixelSize: 16; font.bold: true }
                    Txt { width: parent.width; wrapMode: Text.WordWrap; lineHeight: 1.5; font.pixelSize: 13; text: "Alt + Shift + V: start / stop from any app\nCtrl + Alt + Shift + V: pause / resume\nSpace: record when ready; pause while recording\nEnter: stop and save while recording\nP: EXPERT\nEsc: close panel or go back"; color: Omarchy.secondaryText }
                }
                Column {
                    width: (parent.width - parent.spacing) / 2; spacing: 14
                    Txt { text: "editor"; font.pixelSize: 16; font.bold: true }
                    Txt { width: parent.width; wrapMode: Text.WordWrap; lineHeight: 1.5; font.pixelSize: 13; text: "Space: play / pause\nS: split; X: cut / restore\nLeft / Right: move by 1 second\nShift: 5 seconds; Alt: 0.2 seconds\n[ / ]: jump to a mark; Z: zoom\nCtrl + Z / Ctrl + Y: undo / redo\nCtrl + S: save; Esc: done"; color: Omarchy.secondaryText }
                }
            }
            Column {
                visible: drawer.tab === 5; width: Math.min(700, parent.width); spacing: 16
                Txt { text: "recording encoder"; font.pixelSize: 16; font.bold: true }
                Row {
                    spacing: 12
                    Button { text: "software (x264)"; primary: !Prefs.hardware; onClicked: Prefs.setHardware(false) }
                    Button { text: "NVIDIA H.264"; primary: Prefs.hardware; enabled: Engine.nvencAvailable; onClicked: Prefs.setHardware(true) }
                }
                Button { text: "check hardware"; onClicked: Engine.checkHardware() }
                Txt { width: parent.width; wrapMode: Text.WordWrap; text: Engine.hardwareKnown ? Engine.hardwareMessage : "Check hardware to detect NVIDIA support. This initializes the recording engine without opening the screen picker, camera or microphone."; font.pixelSize: 13; color: Omarchy.secondaryText }
                Txt { width: parent.width; wrapMode: Text.WordWrap; text: "Software is the reliable default. NVIDIA uses less CPU and needs a supported GPU and driver. Your choice applies to the next recording."; font.pixelSize: 13; color: Omarchy.tertiaryText }
            }
            Row {
                visible: drawer.tab === 6; width: parent.width; spacing: 28
                Column {
                    width: (parent.width - parent.spacing) / 2; spacing: 10
                    Txt { text: "go live to X"; font.pixelSize: 16; font.bold: true }
                    Txt { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.tertiaryText; text: "X US East is already filled in. Paste just the stream key from studio.x.com/live (New livestream, RTMP), then save. X may need Premium; Live Studio is in beta." }
                    Rectangle {
                        width: parent.width; height: 40; radius: 10; color: Omarchy.lighterBackground
                        border.color: liveProblem.text !== "" ? Omarchy.red : Omarchy.selection
                        TextInput {
                            id: urlInput
                            objectName: "streamUrlInput"
                            anchors { fill: parent; margins: 12 }
                            verticalAlignment: TextInput.AlignVCenter
                            font.family: "JetBrainsMono Nerd Font"; font.pixelSize: 13
                            color: Omarchy.foreground; selectionColor: Omarchy.accent; selectedTextColor: Omarchy.accentForeground
                            clip: true; text: Prefs.streamUrl
                            onTextEdited: liveProblem.text = ""
                            onAccepted: saveLive.clicked()
                            Keys.onEscapePressed: { text = Prefs.streamUrl; liveProblem.text = ""; App.setPro(false) }
                            Connections { target: Prefs; function onChanged() { if (!urlInput.activeFocus) urlInput.text = Prefs.streamUrl } }
                        }
                        Txt {
                            anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                            visible: urlInput.text === ""
                            text: "server address: rtmp://... or rtmps://..."
                            color: Omarchy.tertiaryText; font.pixelSize: 13
                        }
                    }
                    Rectangle {
                        width: parent.width; height: 40; radius: 10; color: Omarchy.lighterBackground
                        border.color: Omarchy.selection
                        TextInput {
                            id: keyInput
                            objectName: "streamKeyInput"
                            anchors { fill: parent; margins: 12 }
                            verticalAlignment: TextInput.AlignVCenter
                            echoMode: TextInput.Password
                            font.family: "JetBrainsMono Nerd Font"; font.pixelSize: 13
                            color: Omarchy.foreground; selectionColor: Omarchy.accent; selectedTextColor: Omarchy.accentForeground
                            clip: true
                            onTextEdited: liveProblem.text = ""
                            onAccepted: saveLive.clicked()
                            Keys.onEscapePressed: { text = ""; liveProblem.text = ""; App.setPro(false) }
                        }
                        Txt {
                            anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                            visible: keyInput.text === ""
                            text: Prefs.streamKeySaved ? "stream key saved and hidden: paste a new one to replace it" : "stream key: paste it here (kept in a private file)"
                            color: Omarchy.tertiaryText; font.pixelSize: 13
                        }
                    }
                    Row {
                        spacing: 8
                        Button {
                            id: saveLive
                            objectName: "saveLive"
                            text: "save"
                            onClicked: {
                                liveProblem.text = Prefs.setStreamUrl(urlInput.text)
                                if (liveProblem.text === "" && keyInput.text !== "") { liveProblem.text = Prefs.setStreamKey(keyInput.text); if (liveProblem.text === "") keyInput.text = "" }
                                if (liveProblem.text === "" && !Prefs.streamKeySaved) liveProblem.text = "paste the stream key X gives you"
                            }
                        }
                        Button { text: "forget key"; enabled: Prefs.streamKeySaved; onClicked: { Prefs.clearStreamKey(); keyInput.text = "" } }
                        Button { objectName: "closeLive"; text: "close EXPERT"; primary: Prefs.streamReady; onClicked: App.setPro(false) }
                    }
                    Txt { id: liveProblem; width: parent.width; wrapMode: Text.WordWrap; text: ""; color: Omarchy.red; font.pixelSize: 13; visible: text !== "" }
                    Txt {
                        width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13
                        color: Prefs.streamReady ? Omarchy.green : Omarchy.secondaryText
                        text: Prefs.streamReady ? "saved. Next: close EXPERT and press the red GO LIVE button beside Record." : "not set up yet: both boxes are needed, then save."
                    }
                }
                Column {
                    width: (parent.width - parent.spacing) / 2; spacing: 10
                    Txt { text: "stream quality"; font.pixelSize: 16; font.bold: true }
                    Flow {
                        width: parent.width; spacing: 8
                        Repeater {
                            model: [["light", 0], ["standard", 1], ["X recommended", 2]]
                            Button {
                                required property var modelData
                                text: modelData[0]; primary: Prefs.streamQuality === modelData[1]
                                onClicked: Prefs.setStreamQuality(modelData[1])
                            }
                        }
                    }
                    Txt {
                        width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 13; color: Omarchy.secondaryText
                        text: ["720p, 3 Mbps: for a slow connection.", "1080p, 6 Mbps: needs about 8 Mbps of upload.", "1080p, 30 fps, 9 Mbps, AAC 128 kbps, keyframe every 3 s: exactly what X recommends. Needs about 12 Mbps of upload."][Prefs.streamQuality]
                    }
                    Txt { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.tertiaryText; text: "You click go live before each broadcast; it never stays on. A full-quality copy is always saved too, and if the connection drops it keeps going. Pause is off while live." }
                }
            }
            Txt { visible: drawer.tab === 4; width: parent.width; text: Prefs.aboutText; font.pixelSize: 14; color: Omarchy.secondaryText; wrapMode: Text.WordWrap; lineHeight: 1.6 }
        }
    }
}
