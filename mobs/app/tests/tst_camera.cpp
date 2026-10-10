#include "camera.h"
#include "placement.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

static void touch(const QString &path, const QByteArray &content = {}) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(content);
}

class CameraTests : public QObject {
    Q_OBJECT
private slots:
    void prettyNamePrefersTheSystemsOwnName() {
        QCOMPARE(CameraScanner::prettyName("Logitech BRIO\n", "usb-046d_Logitech_BRIO_ACB907D3-video-index0"), QString("Logitech BRIO"));
    }
    void prettyNameFallsBackToTheStableId() {
        QCOMPARE(CameraScanner::prettyName("", "usb-046d_Logitech_BRIO_ACB907D3-video-index0"), QString("Logitech BRIO"));
        QCOMPARE(CameraScanner::prettyName("", "usb-SunplusIT_Inc_HD_Webcam-video-index0"), QString("SunplusIT Inc HD Webcam"));
    }
    void scanFindsEachCameraOnceUsingIndexZeroOnly() {
        QTemporaryDir d; QDir().mkpath(d.path() + "/by-id");   // a fake /dev/v4l/by-id and /sys/class/video4linux, like gus: BRIO with 4 nodes, a laptop webcam with 2
        for (int i = 0; i < 4; ++i) {
            touch(d.path() + QString("/dev/video%1").arg(i));
            QVERIFY(QFile::link(d.path() + QString("/dev/video%1").arg(i), d.path() + QString("/by-id/usb-046d_Logitech_BRIO_ACB907D3-video-index%1").arg(i)));
        }
        touch(d.path() + "/dev/video4");
        QVERIFY(QFile::link(d.path() + "/dev/video4", d.path() + "/by-id/usb-SunplusIT_Inc_HD_Webcam-video-index0"));
        touch(d.path() + "/dev/video5");
        QVERIFY(QFile::link(d.path() + "/dev/video5", d.path() + "/by-id/usb-SunplusIT_Inc_HD_Webcam-video-index1"));
        touch(d.path() + "/sys/video0/name", "Logitech BRIO\n");
        touch(d.path() + "/sys/video4/name", "HD Webcam: HD Webcam\n");
        const auto cams = CameraScanner::scan(d.path() + "/by-id", d.path() + "/sys", false);
        QCOMPARE(cams.size(), 2);
        QCOMPARE(cams[0].name, QString("Logitech BRIO"));
        QVERIFY(cams[0].path.endsWith("usb-046d_Logitech_BRIO_ACB907D3-video-index0"));   // the STABLE id, not /dev/video0
        QCOMPARE(cams[1].name, QString("HD Webcam: HD Webcam"));
    }
    void scanOfAMissingFolderIsEmptyNotACrash() {
        QVERIFY(CameraScanner::scan("/nonexistent/by-id", "/nonexistent/sys", false).isEmpty());
    }

    // ---- placement: the recording and the on-screen diagram share this ---------------------------------------------
    void placementBottomRightMedium() {
        const QRect r = Placement::cameraRect(3, 1);                    // 24% of 1920 = 461 wide, 16:9
        QCOMPARE(r.width(), 461);
        QCOMPARE(r.height(), 461 * 9 / 16);
        QCOMPARE(r.right() + 1 + 32, 1920);                              // 32 px from the right edge
        QCOMPARE(r.bottom() + 1 + 32, 1080);                             // and the bottom
    }
    void placementCornersAreMirrorImages() {
        const QRect tl = Placement::cameraRect(0, 1), br = Placement::cameraRect(3, 1);
        QCOMPARE(tl.topLeft(), QPoint(32, 32));
        QCOMPARE(tl.width(), br.width());
        const QRect tr = Placement::cameraRect(1, 1), bl = Placement::cameraRect(2, 1);
        QCOMPARE(tr.y(), tl.y()); QCOMPARE(tr.right(), br.right());
        QCOMPARE(bl.x(), tl.x()); QCOMPARE(bl.bottom(), br.bottom());
    }
    void placementSizesGrowAndStayOnTheCanvas() {
        for (int corner = 0; corner < 4; ++corner) {
            int prev = 0;
            for (int size = 0; size < 3; ++size) {
                const QRect r = Placement::cameraRect(corner, size);
                QVERIFY(r.width() > prev); prev = r.width();
                QVERIFY(QRect(0, 0, 1920, 1080).contains(r));
            }
        }
    }
    void placementBadInputsFallBackToMedium() {
        QCOMPARE(Placement::cameraRect(3, 9), Placement::cameraRect(3, 1));
        QCOMPARE(Placement::cameraRect(3, -1), Placement::cameraRect(3, 1));
    }
};
QTEST_MAIN(CameraTests)
#include "tst_camera.moc"
