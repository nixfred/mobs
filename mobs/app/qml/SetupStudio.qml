import QtQuick
import MobsCore

// Step 6: open X Live Studio, with the real page shown and the next button ringed. MOBS cannot do this part for the person (X needs their own login),
// so it opens the page and shows exactly where to click.
Item {
    id: root
    objectName: "setupStudio"
    readonly property bool canNext: true
    readonly property bool canSkip: false
    property bool opened: false
    Column {
        anchors.centerIn: parent
        width: Math.min(780, root.width - 48)
        spacing: 12
        Txt { text: "Open X Live Studio"; font.pixelSize: 26; font.bold: true }
        Txt {
            width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.secondaryText
            text: "Live Studio is the page on X where a live stream starts. It needs an X account with Premium. Press the button, sign in if X asks, then click the button the red ring points at."
        }
        Row {
            spacing: 14
            BigButton { objectName: "setupOpenStudio"; primary: false; text: "OPEN LIVE STUDIO"; implicitHeight: 46; implicitWidth: 260; onClicked: root.opened = App.openUrl("https://studio.x.com/live") }
            Txt { anchors.verticalCenter: parent.verticalCenter; visible: root.opened; text: "opened in your browser"; font.pixelSize: 13; color: Omarchy.green }
        }
        GuideImage { name: "x-live-studio.png"; maxWidth: parent.width; maxHeight: 120; rings: [{ x: 0.724, y: 0.054, w: 0.134, h: 0.2 }] }
        Txt { text: "This is the real page. Next you click Manage Sources."; font.pixelSize: 12; color: Omarchy.tertiaryText }
    }
}
