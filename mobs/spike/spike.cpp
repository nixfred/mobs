// MOBS dock spike. Loads Monologue's UNMODIFIED EditBar.qml in a QQuickWidget inside an
// OBS dock, over a stub "backend" object, and logs evidence to the OBS log.
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QDockWidget>
#include <QQmlContext>
#include <QVariantList>
#include <QVariantMap>
#include <QUrl>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mobs-spike", "en-US")

// Same shape as Monologue's Thumbnails: count/ready/revision + request().
class StubThumbs : public QObject {
	Q_OBJECT
	Q_PROPERTY(int count READ count CONSTANT)
	Q_PROPERTY(int ready READ ready CONSTANT)
	Q_PROPERTY(int revision READ revision CONSTANT)
public:
	using QObject::QObject;
	int count() const { return 12; }
	int ready() const { return 0; }
	int revision() const { return 0; }
	Q_INVOKABLE void request(double, double) {}
};

// Same property/method names EditBar.qml uses on Monologue's Backend.
class StubBackend : public QObject {
	Q_OBJECT
	Q_PROPERTY(double duration READ duration CONSTANT)
	Q_PROPERTY(QVariantList clips READ clips NOTIFY editChanged)
	Q_PROPERTY(QVariantList pauses READ pauses CONSTANT)
	Q_PROPERTY(QObject *thumbnails READ thumbnails CONSTANT)
public:
	explicit StubBackend(QObject *parent) : QObject(parent), m_thumbs(new StubThumbs(this))
	{
		m_clips = {QVariantMap{{"start", 0.0}, {"end", 60.0}}};
	}
	double duration() const { return 60.0; }
	QVariantList clips() const { return m_clips; }
	QVariantList pauses() const { return {12.0, 41.0}; }
	QObject *thumbnails() const { return m_thumbs; }

	Q_INVOKABLE void split(double t)
	{
		QVariantList out;
		for (const auto &c : m_clips) {
			auto m = c.toMap();
			if (t > m["start"].toDouble() && t < m["end"].toDouble()) {
				out.append(QVariantMap{{"start", m["start"]}, {"end", t}});
				out.append(QVariantMap{{"start", t}, {"end", m["end"]}});
			} else {
				out.append(c);
			}
		}
		m_clips = out;
		emit editChanged();
	}
	Q_INVOKABLE void setClip(int i, double s, double e)
	{
		if (i < 0 || i >= m_clips.size()) return;
		m_clips[i] = QVariantMap{{"start", s}, {"end", e}};
		emit editChanged();
	}
	Q_INVOKABLE void removeClip(int i)
	{
		if (i < 0 || i >= m_clips.size()) return;
		m_clips.removeAt(i);
		emit editChanged();
	}
	Q_INVOKABLE void joinClips(int) {}
	Q_INVOKABLE void beginGesture() {}
	Q_INVOKABLE void endGesture() {}
	Q_INVOKABLE void log(const QString &s) { blog(LOG_INFO, "[mobs-spike] %s", s.toUtf8().constData()); }
signals:
	void editChanged();

private:
	StubThumbs *m_thumbs;
	QVariantList m_clips;
};

static void make_dock()
{
	char *qml = obs_module_file("spike.qml");
	if (!qml) {
		blog(LOG_ERROR, "[mobs-spike] spike.qml not found in module data dir");
		return;
	}
	auto *view = new QQuickWidget;
	view->setResizeMode(QQuickWidget::SizeRootObjectToView);
	view->rootContext()->setContextProperty("backend", new StubBackend(view));
	view->setSource(QUrl::fromLocalFile(qml));
	blog(LOG_INFO, "[mobs-spike] QQuickWidget status=%d (1=Ready) from %s", (int)view->status(), qml);
	for (const auto &e : view->errors())
		blog(LOG_ERROR, "[mobs-spike] QML error: %s", e.toString().toUtf8().constData());
	bfree(qml);
	bool ok = obs_frontend_add_dock_by_id("mobs-spike", "MOBS spike", view);
	blog(LOG_INFO, "[mobs-spike] dock added=%d", (int)ok);
	for (QWidget *w = view->parentWidget(); w; w = w->parentWidget())
		blog(LOG_INFO, "[mobs-spike] parent chain: %s", w->metaObject()->className());
	if (auto *dock = qobject_cast<QDockWidget *>(view->parentWidget())) {
		dock->setFloating(true);
		dock->resize(760, 170);
		dock->show();
	}
}

static void on_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) make_dock();
}

bool obs_module_load(void)
{
	// Qt Quick's default OpenGL path is the suspect for the black OBS preview; software
	// rendering uses no GL context at all (fine for a flat 2D timeline).
	if (!qEnvironmentVariableIsSet("MOBS_SPIKE_GL"))
		QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
	obs_frontend_add_event_callback(on_event, nullptr);
	return true;
}

#include "spike.moc"
