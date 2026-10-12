import QtQuick
import MobsCore

// Step 7: make a "source" on X, once. A source is the front door your stream goes through; it keeps its key, so this is never repeated.
Item {
    id: root
    objectName: "setupSource"
    readonly property bool canNext: true
    readonly property bool canSkip: true
    readonly property string skipText: "I already have a source"
    Column {
        anchors.centerIn: parent
        width: Math.min(780, root.width - 48)
        spacing: 12
        Txt { text: "Make a source called MOBS"; font.pixelSize: 26; font.bold: true }
        Txt {
            width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Omarchy.foreground; lineHeight: 1.25
            text: "1. On Manage Sources, press Create Source (the red ring).\n2. Name it MOBS.\n3. Region: US East (N. Virginia). MOBS is already set for it.\n4. Press Create. You do this once; the source stays."
        }
        GuideImage { name: "x-manage-sources.png"; maxWidth: parent.width; maxHeight: 70; rings: [{ x: 0.843, y: 0.22, w: 0.14, h: 0.53 }] }
        Txt { text: "A source is just the door your stream goes through."; font.pixelSize: 12; color: Omarchy.tertiaryText }
    }
}
