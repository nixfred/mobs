#include "camera.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

QString CameraScanner::prettyName(const QString &sysName, const QString &byIdName) {
    if (!sysName.trimmed().isEmpty()) return sysName.trimmed();
    // usb-046d_Logitech_BRIO_ACB907D3-video-index0  ->  Logitech BRIO
    QString n = byIdName;
    n.remove(QRegularExpression("-video-index\\d+$"));
    n.remove(QRegularExpression("^(usb|pci|platform)-"));
    n.remove(QRegularExpression("^[0-9a-fA-F]{4}_"));       // vendor id
    n.remove(QRegularExpression("_(?=[0-9A-Za-z]*\\d)[0-9A-Za-z]{6,}$"));   // serial: 6+ chars WITH a digit, so "_Webcam" stays
    return n.replace('_', ' ').trimmed();
}

static void probeDevice(const QString &path, bool *isCapture, bool *mjpg720) {
    *isCapture = true; *mjpg720 = false;
    const int fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0) return;   // cannot open: keep it listed, and let the recorder report the real error
    v4l2_capability cap{};
    if (::ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
        const __u32 caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
        *isCapture = (caps & V4L2_CAP_VIDEO_CAPTURE) != 0;
    }
    if (*isCapture) {
        for (v4l2_fmtdesc f{}; f.index < 32; ++f.index) {
            f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (::ioctl(fd, VIDIOC_ENUM_FMT, &f) != 0) break;
            if (f.pixelformat != V4L2_PIX_FMT_MJPEG) continue;
            for (v4l2_frmsizeenum s{}; s.index < 64; ++s.index) {
                s.pixel_format = f.pixelformat;
                if (::ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &s) != 0) break;
                if (s.type == V4L2_FRMSIZE_TYPE_DISCRETE && s.discrete.width == 1280 && s.discrete.height == 720) *mjpg720 = true;
            }
        }
    }
    ::close(fd);
}

QList<CameraInfo> CameraScanner::scan(const QString &byIdDir, const QString &sysDir, bool probe) {
    QList<CameraInfo> out;
    const QDir dir(byIdDir);
    for (const QFileInfo &e : dir.entryInfoList({"*-video-index0"}, QDir::Files | QDir::System | QDir::NoDotAndDotDot, QDir::Name)) {
        const QString target = QFileInfo(e.symLinkTarget()).fileName();   // video0
        QString sysName;
        if (QFile f(sysDir + "/" + target + "/name"); f.open(QIODevice::ReadOnly)) sysName = QString::fromUtf8(f.readLine());
        bool capture = true, mjpg = false;
        if (probe) probeDevice(e.absoluteFilePath(), &capture, &mjpg);
        if (!capture) continue;
        CameraInfo c;
        c.path = e.absoluteFilePath();
        c.name = prettyName(sysName, e.fileName());
        c.mjpg720 = mjpg;
        out.append(c);
    }
    return out;
}
