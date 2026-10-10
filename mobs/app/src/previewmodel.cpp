#include "previewmodel.h"

namespace {
class Provider : public QQuickImageProvider {
public:
    explicit Provider(const PreviewModel *model) : QQuickImageProvider(QQuickImageProvider::Image), m_model(model) {}
    QImage requestImage(const QString &, QSize *size, const QSize &) override {
        const QImage img = m_model->image();
        if (size) *size = img.size();
        return img;
    }
private:
    const PreviewModel *m_model;
};
}

PreviewModel::PreviewModel(QObject *parent) : QObject(parent), m_provider(new Provider(this)) {}

bool PreviewModel::hasFrame() const { QMutexLocker l(&m_mutex); return !m_image.isNull(); }
QImage PreviewModel::image() const { QMutexLocker l(&m_mutex); return m_image; }

void PreviewModel::setFrame(const QImage &image) {
    { QMutexLocker l(&m_mutex); m_image = image; ++m_serial; }
    emit frameChanged();
}

void PreviewModel::clear() {
    { QMutexLocker l(&m_mutex); if (m_image.isNull()) return; m_image = QImage(); ++m_serial; }
    emit frameChanged();
}
