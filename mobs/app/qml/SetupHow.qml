import QtQuick
import MobsCore

// Step 9: what going live looks like, every time, with the real Live Studio dialog. X makes a new livestream for each broadcast; the source
// (the front door) stays. This is the one thing MOBS cannot do for the person, so the page says it plainly and shows it.
Item {
    id: root
    objectName: "setupHow"
    readonly property bool canNext: true
    readonly property bool canSkip: false
    Row {
        anchors.centerIn: parent
        spacing: 26
        GuideImage {
            name: "x-create-livestream.png"; maxWidth: 440; maxHeight: 250
            rings: [{ x: 0.26, y: 0.399, w: 0.575, h: 0.073 }, { x: 0.854, y: 0.901, w: 0.086, h: 0.06 }]
        }
        Column {
            width: Math.min(380, root.width - 48 - 440 - 26); spacing: 9
            Txt { text: "Going live, every time"; font.pixelSize: 24; font.bold: true }
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.foreground; lineHeight: 1.2
                text: "1. In MOBS press GO LIVE. Your picture goes to X. Nobody sees it yet.\n2. In Live Studio press New Livestream and pick your source" + (Prefs.sourceName !== "" ? ", " + Prefs.sourceName : "") + ". A green dot means MOBS is connected to it.\n3. Choose Public (or Private to try it out), then press Go Live.\n4. Press Post livestream on X so your followers know."
            }
            Txt { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.tertiaryText; text: "MOBS opens Live Studio for you and shows this again right before you go live." }
        }
    }
}
