#pragma once
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>

// The newest preview picture, for QML: `Image { source: "image://preview/" + Preview.serial }`.
// serial changes on every frame, which makes the Image ask the provider again.
class PreviewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int serial READ serial NOTIFY frameChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY frameChanged)
public:
    explicit PreviewModel(QObject *parent = nullptr);
    int serial() const { return m_serial; }
    bool hasFrame() const;
    QImage image() const;                       // thread-safe copy (what the provider hands to QML)
    QQuickImageProvider *provider() { return m_provider; }   // ownership passes to the QML engine
public slots:
    void setFrame(const QImage &image);
    void clear();                               // the preview was switched off: forget the last picture
signals:
    void frameChanged();
private:
    mutable QMutex m_mutex;
    QImage m_image;
    int m_serial = 0;
    QQuickImageProvider *m_provider;
};
