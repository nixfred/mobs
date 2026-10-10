#pragma once
#include <QList>
#include <QString>

struct CameraInfo {
    QString path;           // stable: /dev/v4l/by-id/...-video-index0 (does not change when /dev/videoN renumbers)
    QString name;           // "Logitech BRIO"
    bool mjpg720 = false;   // can do 1280x720 MJPEG: ask for it instead of the camera's slow default format
    bool operator==(const CameraInfo &o) const { return path == o.path && name == o.name && mjpg720 == o.mjpg720; }
};

// Finds the real cameras (not the metadata or infrared nodes some webcams add).
class CameraScanner {
public:
    // byIdDir and sysDir are parameters so tests can point at a fake tree. probe=false skips the device ioctls.
    static QList<CameraInfo> scan(const QString &byIdDir = "/dev/v4l/by-id", const QString &sysDir = "/sys/class/video4linux", bool probe = true);
    static QString prettyName(const QString &sysName, const QString &byIdName);
};
