import QtQuick
import MobsCore

// Live Studio's page in a box of the (scaled) interface. A Loader sizes whatever it loads to its own box, so the browser view sits inside this frame, which it may size itself: the page is
// drawn at the real size of the box in screen pixels and shrunk back (XStudioView), sharp at any interface scale.
Item {
    id: frame
    property alias view: studio            // the browser view: what the helper asks questions of
    property alias url: studio.url
    property bool full: false              // a sign-in form: used at the size of the rest of the interface instead of showing a whole page
    clip: true
    XStudioView {
        id: studio
        objectName: "studioView"
        box: frame
        full: frame.full
    }
}
