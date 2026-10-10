#pragma once
#include <QRect>

// Where the webcam sits on the recorded canvas for "screen + me". ONE definition, used by the recording (ObsBackend)
// and by the diagram in the camera panel, so what you see in the panel is what ends up in the file.
// corner: 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right.  size: 0 small, 1 medium, 2 large.
namespace Placement {
constexpr int kCanvasW = 1920, kCanvasH = 1080;
inline QRect cameraRect(int corner, int size, int canvasW = kCanvasW, int canvasH = kCanvasH) {
    static const double kWidthFraction[] = {0.16, 0.24, 0.34};
    const int s = (size < 0 || size > 2) ? 1 : size;
    const int w = int(canvasW * kWidthFraction[s] + 0.5);
    const int h = w * 9 / 16;
    const int margin = canvasW / 60;   // 32 px on 1920
    const bool right = (corner == 1 || corner == 3), bottom = (corner == 2 || corner == 3);
    return QRect(right ? canvasW - w - margin : margin, bottom ? canvasH - h - margin : margin, w, h);
}

// A webcam dragged to a place of the user's choosing: x, y is its top-left corner on the canvas, kept fully inside it.
inline QRect cameraRectAt(int x, int y, int size, int canvasW = kCanvasW, int canvasH = kCanvasH) {
    const QRect corner = cameraRect(0, size, canvasW, canvasH);          // for the size only
    return QRect(qBound(0, x, canvasW - corner.width()), qBound(0, y, canvasH - corner.height()), corner.width(), corner.height());
}

// "The show": the shared window big on the left, two tiles stacked on the right (your guest above, you below), the way a video
// call's gallery looks. ONE definition for the recording and for the diagram in the camera panel, like cameraRect above.
struct ShowLayout { QRect main, guest, me; };
inline ShowLayout showLayout(int canvasW = kCanvasW, int canvasH = kCanvasH) {
    const int pad = canvasW / 160;                    // 12 px on 1920
    const int columnW = canvasW / 4;                  // the right-hand column of tiles
    const int tileW = columnW - 2 * pad;
    const int tileH = tileW * 9 / 16;
    const int top = (canvasH - (2 * tileH + pad)) / 2;
    ShowLayout l;
    l.main = QRect(pad, pad, canvasW - columnW - pad, canvasH - 2 * pad);
    l.guest = QRect(canvasW - columnW + pad, top, tileW, tileH);
    l.me = QRect(canvasW - columnW + pad, top + tileH + pad, tileW, tileH);
    return l;
}
}
