import QtQuick
import MobsCore

// Step 7: what going live looks like, every time. MOBS does the steps in X's Live Studio itself (LivePilot): it types the title, waits until X says it sees the video, presses Go Live and
// takes the livestream's address from the page. The picture is the REAL Create Livestream form it fills in, with a ring on the two things that matter.
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
            rings: [{ x: 0.26, y: 0.185, w: 0.575, h: 0.085 }, { x: 0.854, y: 0.901, w: 0.086, h: 0.06 }]
        }
        Column {
            width: Math.min(380, root.width - 48 - 440 - 26); spacing: 9
            Txt { text: "Going live, every time"; font.pixelSize: 24; font.bold: true }
            Txt {
                width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.foreground; lineHeight: 1.2
                text: "1. Type a title on the ready screen (MOBS remembers it) and press GO LIVE. Your picture goes to X.\n2. MOBS fills in this form for you in its own box: the title, who can watch. X will not let you go live without a title.\n3. MOBS waits until X says it sees your video (X looks every 20 seconds), then presses Go Live. Your chat shows under your webcam and this window steps out of the picture."
            }
            Txt { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Omarchy.tertiaryText; text: "There is nothing to click in X. If something goes wrong MOBS says what X said, and you can finish in the same box." }
        }
    }
}
