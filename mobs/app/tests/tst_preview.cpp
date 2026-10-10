#include "previewmodel.h"
#include <QSignalSpy>
#include <QtTest>

class PreviewTests : public QObject {
    Q_OBJECT
    static QImage solid(QColor c) { QImage i(640, 360, QImage::Format_RGB32); i.fill(c); return i; }
private slots:
    void startsEmpty() {
        PreviewModel m;
        QVERIFY(!m.hasFrame());
        QVERIFY(m.image().isNull());
    }
    void everyFrameChangesTheSerialSoQmlAsksAgain() {
        PreviewModel m;
        QSignalSpy sp(&m, &PreviewModel::frameChanged);
        m.setFrame(solid(Qt::red));
        const int a = m.serial();
        m.setFrame(solid(Qt::green));
        QVERIFY(m.serial() != a);
        QCOMPARE(sp.count(), 2);
        QVERIFY(m.hasFrame());
    }
    void theProviderServesTheNewestPicture() {
        PreviewModel m;
        m.setFrame(solid(QColor(10, 200, 30)));
        QSize size;
        QImage got = static_cast<QQuickImageProvider *>(m.provider())->requestImage("1", &size, QSize());
        QCOMPARE(size, QSize(640, 360));
        QCOMPARE(QColor(got.pixel(5, 5)), QColor(10, 200, 30));
        m.setFrame(solid(QColor(200, 10, 30)));
        got = static_cast<QQuickImageProvider *>(m.provider())->requestImage("2", &size, QSize());
        QCOMPARE(QColor(got.pixel(5, 5)), QColor(200, 10, 30));
    }
    void clearForgetsThePictureAndTellsQml() {
        PreviewModel m;
        m.setFrame(solid(Qt::red));
        QSignalSpy sp(&m, &PreviewModel::frameChanged);
        m.clear();
        QVERIFY(!m.hasFrame());
        QCOMPARE(sp.count(), 1);
        m.clear();                         // clearing an empty model is not a change
        QCOMPARE(sp.count(), 1);
    }
    void framesMayArriveFromAnotherThread() {
        PreviewModel m;
        QThread t;
        QObject worker; worker.moveToThread(&t);
        t.start();
        QMetaObject::invokeMethod(&worker, [&m] { for (int i = 0; i < 200; ++i) m.setFrame(solid(QColor(i % 255, 0, 0))); }, Qt::QueuedConnection);
        QTRY_VERIFY_WITH_TIMEOUT(m.hasFrame() && m.serial() >= 200, 5000);
        t.quit(); t.wait();
    }
};
QTEST_MAIN(PreviewTests)
#include "tst_preview.moc"
