import QtQuick
import MobsCore

// One device per page keeps full readable descriptions and all actions on small windows.
Column {
    id: root
    property var devices: []
    property string selectedId: "default"
    property string defaultLabel: "system default"
    property bool showDefault: true
    property int page: 0
    readonly property var currentDevice: page >= 0 && page < devices.length ? devices[page] : null
    signal selected(string id)
    spacing: 6
    function syncPage() {
        let selected = devices.findIndex(device => device.id === selectedId)
        page = selected >= 0 ? selected : Math.max(0, Math.min(page, devices.length - 1))
    }
    onDevicesChanged: syncPage()
    onSelectedIdChanged: syncPage()
    Button { visible: root.showDefault; width: parent.width; text: root.defaultLabel; primary: root.selectedId === "default"; onClicked: root.selected("default") }
    Rectangle {
        id: deviceCard
        objectName: root.objectName + "Device"
        signal clicked()
        onClicked: if (root.currentDevice) root.selected(root.currentDevice.id)
        visible: root.devices.length > 0
        width: parent.width; height: Math.max(44, deviceText.implicitHeight + 16); radius: 10
        color: root.currentDevice && root.selectedId === root.currentDevice.id ? Omarchy.accent : Omarchy.lighterBackground
        border.color: Omarchy.selection
        Txt {
            id: deviceText
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 10 }
            text: root.currentDevice ? root.currentDevice.name : ""
            font.pixelSize: 13; wrapMode: Text.Wrap
            color: root.currentDevice && root.selectedId === root.currentDevice.id ? Omarchy.accentForeground : Omarchy.foreground
        }
        TapHandler { enabled: root.currentDevice !== null; onTapped: deviceCard.clicked() }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
    }
    Row {
        spacing: 8; visible: root.devices.length > 1
        Button { objectName: root.objectName + "Previous"; text: "‹"; height: 32; enabled: root.page > 0; onClicked: root.page-- }
        Txt { text: root.page + 1 + " / " + root.devices.length; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
        Button { objectName: root.objectName + "Next"; text: "›"; height: 32; enabled: root.page < root.devices.length - 1; onClicked: root.page++ }
    }
}
