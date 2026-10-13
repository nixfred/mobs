import QtQuick
import MobsCore

// Step 7: the person's SOURCE on X. A source is a named door the stream goes through; it has its own stream key and X keeps it, so it is made once.
// The page asks first whether one already exists (Manage Sources lists them) instead of telling everyone to make a new one: a second source has a second
// key, and going live with a source whose key MOBS does not have shows nothing. Either answer ends with the same instruction: remember the NAME.
Item {
    id: root
    objectName: "setupSource"
    property string mode: ""          // "" not answered yet, "have" a source already exists, "make" it has to be made
    readonly property bool canNext: mode !== ""
    readonly property bool canSkip: false
    Column {
        anchors.centerIn: parent
        width: Math.min(780, root.width - 48)
        spacing: 10
        Txt { text: "Your source on X"; font.pixelSize: 26; font.bold: true }
        Txt {
            width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.foreground; lineHeight: 1.2
            text: "A source is a named door your video goes through to reach X. It has its own key. You make it once and X keeps it. Does Manage Sources already list one?"
        }
        Row {
            spacing: 12
            BigButton { objectName: "setupHaveSource"; primary: root.mode === "have"; text: "YES, I HAVE ONE"; implicitHeight: 44; implicitWidth: 250; onClicked: root.mode = "have" }
            BigButton { objectName: "setupMakeSource"; primary: root.mode === "make"; text: "NO, MAKE ONE"; implicitHeight: 44; implicitWidth: 250; onClicked: root.mode = "make" }
        }
        Txt { visible: root.mode === ""; text: "Press one of the two buttons."; font.pixelSize: 13; color: Omarchy.tertiaryText }
        Txt {
            visible: root.mode === "have"; width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.foreground; lineHeight: 1.2
            text: "Good. Remember its NAME, for example Gus. The next page puts its key into MOBS, and every time you go live you pick this same source."
        }
        GuideImage { visible: root.mode === "have"; name: "x-source-card.png"; maxWidth: 440; maxHeight: 76; rings: [{ x: 0.0, y: 0.08, w: 0.11, h: 0.32 }] }
        Txt {
            visible: root.mode === "make"; width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.foreground; lineHeight: 1.2
            text: "1. On Manage Sources, press Create Source (the red ring).\n2. Name it, for example MOBS.\n3. Region: US East (N. Virginia). MOBS is already set for it.\n4. Press Create. Remember the name."
        }
        GuideImage { visible: root.mode === "make"; name: "x-manage-sources.png"; maxWidth: parent.width; maxHeight: 62; rings: [{ x: 0.843, y: 0.22, w: 0.14, h: 0.53 }] }
    }
}
