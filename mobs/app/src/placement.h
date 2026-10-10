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
}
