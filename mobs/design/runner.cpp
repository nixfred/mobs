// Loads the design prototype. Everything it shows is sample data.
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QPainter>
#include <QLinearGradient>
#include <QVariantMap>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>

// Same property/method names EditBar.qml uses on Monologue's Backend, with sample values.
class SampleThumbs : public QObject {
	Q_OBJECT
	Q_PROPERTY(int count READ count CONSTANT)
	Q_PROPERTY(int ready READ ready CONSTANT)
	Q_PROPERTY(int revision READ revision CONSTANT)
public:
	using QObject::QObject;
	int count() const { return 14; }
	int ready() const { return 14; }
	int revision() const { return 0; }
	Q_INVOKABLE void request(double, double) {}
};

class SampleBackend : public QObject {
	Q_OBJECT
	Q_PROPERTY(double duration READ duration CONSTANT)
	Q_PROPERTY(QVariantList clips READ clips NOTIFY editChanged)
	Q_PROPERTY(QVariantList pauses READ pauses CONSTANT)
	Q_PROPERTY(QObject *thumbnails READ thumbnails CONSTANT)
public:
	explicit SampleBackend(QObject *p) : QObject(p), m_thumbs(new SampleThumbs(this))
	{
		m_clips = {QVariantMap{{"start", 3.0}, {"end", 71.0}}, QVariantMap{{"start", 80.0}, {"end", 134.0}}};
	}
	double duration() const { return 134.0; }
	QVariantList clips() const { return m_clips; }
	QVariantList pauses() const { return {45.0, 98.0}; }
	QObject *thumbnails() const { return m_thumbs; }
	Q_INVOKABLE void split(double) {}
	Q_INVOKABLE void setClip(int, double, double) {}
	Q_INVOKABLE void removeClip(int) {}
	Q_INVOKABLE void joinClips(int) {}
	Q_INVOKABLE void beginGesture() {}
	Q_INVOKABLE void endGesture() {}
signals:
	void editChanged();

private:
	SampleThumbs *m_thumbs;
	QVariantList m_clips;
};

// Sample filmstrip frames: soft gradients, clearly not real footage.
class SampleFrames : public QQuickImageProvider {
public:
	SampleFrames() : QQuickImageProvider(QQuickImageProvider::Image) {}
	QImage requestImage(const QString &id, QSize *size, const QSize &) override
	{
		const int i = id.section('/', -1).toInt();
		QImage img(160, 90, QImage::Format_RGB32);
		QPainter p(&img);
		QLinearGradient g(0, 0, 160, 90);
		const int hue = (i * 23 + 200) % 360;
		g.setColorAt(0, QColor::fromHsv(hue, 90, 70));
		g.setColorAt(1, QColor::fromHsv((hue + 40) % 360, 120, 120));
		p.fillRect(img.rect(), g);
		if (size) *size = img.size();
		return img;
	}
};

int main(int argc, char *argv[])
{
	QGuiApplication app(argc, argv);
	QQmlApplicationEngine engine;
	engine.addImageProvider("thumbs", new SampleFrames);
	engine.rootContext()->setContextProperty("backend", new SampleBackend(&app));
	engine.rootContext()->setContextProperty("startScreen", qEnvironmentVariableIntValue("MOBS_SCREEN") ?: 1);
	engine.load(QUrl::fromLocalFile(argc > 1 ? argv[1] : "proto.qml"));
	if (engine.rootObjects().isEmpty()) return 1;
	// MOBS_GRAB=/path.png saves the app window's own pixels (no desktop, no toasts) and quits.
	if (const QString grab = qEnvironmentVariable("MOBS_GRAB"); !grab.isEmpty()) {
		auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
		QTimer::singleShot(2500, &app, [&app, w, grab] {
			const QImage img = w->grabWindow();
			qInfo() << "grab" << img.size() << img.save(grab);
			app.quit();
		});
	}
	return app.exec();
}

#include "runner.moc"
