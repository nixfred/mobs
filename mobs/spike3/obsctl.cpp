// Minimal obs-websocket v5 client: connect, authenticate, record with a pause, report the file.
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUuid>
#include <QWebSocket>
#include <cstdio>

static QWebSocket ws;
static QHash<QString, QJsonObject> responses;
static QJsonObject hello, identified, stoppedEvent;
static int failures = 0;

static void say(const QString &s) { std::printf("%s\n", s.toUtf8().constData()); std::fflush(stdout); }
static void check(bool ok, const QString &what) { say(QString(ok ? "PASS  " : "FAIL  ") + what); if (!ok) ++failures; }

static void sleepMs(int ms) { QEventLoop l; QTimer::singleShot(ms, &l, &QEventLoop::quit); l.exec(); }

static QJsonObject request(const QString &type, const QJsonObject &data = {}, int timeoutMs = 8000)
{
	const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
	QJsonObject d{{"requestType", type}, {"requestId", id}};
	if (!data.isEmpty()) d["requestData"] = data;
	ws.sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"op", 6}, {"d", d}}).toJson(QJsonDocument::Compact)));
	QElapsedTimer t;
	t.start();
	while (!responses.contains(id) && t.elapsed() < timeoutMs) sleepMs(10);
	return responses.value(id);
}
static bool ok(const QJsonObject &r) { return r["requestStatus"].toObject()["result"].toBool(); }
static QString why(const QJsonObject &r) { const auto s = r["requestStatus"].toObject(); return QString("code %1 %2").arg(s["code"].toInt()).arg(s["comment"].toString()); }

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	const QString pass = qEnvironmentVariable("MOBS_OBS_PASSWORD");
	const QString dir = qEnvironmentVariable("MOBS_REC_DIR", "/tmp");
	QObject::connect(&ws, &QWebSocket::textMessageReceived, [](const QString &m) {
		const auto o = QJsonDocument::fromJson(m.toUtf8()).object();
		const int op = o["op"].toInt();
		if (op == 0) hello = o["d"].toObject();
		else if (op == 2) identified = o["d"].toObject();
		else if (op == 5) {
			const auto d = o["d"].toObject();
			if (d["eventType"].toString() == "RecordStateChanged" && d["eventData"].toObject()["outputState"].toString() == "OBS_WEBSOCKET_OUTPUT_STOPPED")
				stoppedEvent = d["eventData"].toObject();
		}
		else if (op == 7) responses[o["d"].toObject()["requestId"].toString()] = o["d"].toObject();
	});

	// connect
	ws.open(QUrl("ws://127.0.0.1:4455"));
	for (QElapsedTimer t; (t.start(), true);) { while (hello.isEmpty() && t.elapsed() < 8000) sleepMs(20); break; }
	check(!hello.isEmpty(), "websocket connected, got Hello");
	if (hello.isEmpty()) return 2;

	// authenticate (v5: base64(sha256(base64(sha256(password+salt)) + challenge)))
	QJsonObject ident{{"rpcVersion", 1}, {"eventSubscriptions", 64}};
	if (hello.contains("authentication")) {
		const auto a = hello["authentication"].toObject();
		const QByteArray secret = QCryptographicHash::hash((pass + a["salt"].toString()).toUtf8(), QCryptographicHash::Sha256).toBase64();
		ident["authentication"] = QString::fromUtf8(QCryptographicHash::hash(secret + a["challenge"].toString().toUtf8(), QCryptographicHash::Sha256).toBase64());
	}
	ws.sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"op", 1}, {"d", ident}}).toJson(QJsonDocument::Compact)));
	for (QElapsedTimer t; (t.start(), true);) { while (identified.isEmpty() && t.elapsed() < 5000) sleepMs(20); break; }
	check(!identified.isEmpty(), "authenticated (Identified)");
	if (identified.isEmpty()) return 2;

	auto v = request("GetVersion");
	check(ok(v), QString("GetVersion: OBS %1, websocket %2").arg(v["responseData"].toObject()["obsVersion"].toString(), v["responseData"].toObject()["obsWebSocketVersion"].toString()));

	// 'my screen' preset stand-in: make sure a scene exists and is current (colour source = no portal needed)
	request("CreateScene", {{"sceneName", "MOBS"}});
	auto sc = request("SetCurrentProgramScene", {{"sceneName", "MOBS"}});
	check(ok(sc), "scene 'MOBS' created and made current: " + (ok(sc) ? QString("ok") : why(sc)));
	if (qEnvironmentVariableIsSet("MOBS_SCREEN")) {
		// Real "my screen": PipeWire screen capture, which goes through the xdg desktop portal picker.
		auto sr = request("CreateInput", {{"sceneName", "MOBS"}, {"inputName", "my screen"}, {"inputKind", "pipewire-screen-capture-source"},
		                                  {"inputSettings", QJsonObject{{"ShowCursor", true}}}});
		say("screen input: " + (ok(sr) ? QString("created, waiting for the portal picker to be answered...") : why(sr)));
		sleepMs(qEnvironmentVariableIntValue("MOBS_PICK_WAIT") ?: 25000);
	}
	auto in = request("CreateInput", {{"sceneName", "MOBS"}, {"inputName", "test colour"}, {"inputKind", "color_source_v3"},
	                                  {"inputSettings", QJsonObject{{"color", 4278190335.0}, {"width", 1920}, {"height", 1080}}}});
	check(ok(in) || in["requestStatus"].toObject()["code"].toInt() == 601, "colour input present: " + (ok(in) ? QString("created") : why(in)));

	auto rd = request("SetRecordDirectory", {{"recordDirectory", dir}});
	check(ok(rd), "SetRecordDirectory " + dir + (ok(rd) ? "" : ": " + why(rd)));

	// record, pause, resume, stop
	QElapsedTimer total;
	total.start();
	auto st = request("StartRecord");
	check(ok(st), "StartRecord" + (ok(st) ? QString("") : ": " + why(st)));
	sleepMs(qEnvironmentVariableIsSet("MOBS_SCREEN") ? 5000 : 3000);
	auto stat = request("GetRecordStatus");
	check(stat["responseData"].toObject()["outputActive"].toBool(), "GetRecordStatus: outputActive after 3 s");
	auto pa = request("PauseRecord");
	check(ok(pa), "PauseRecord" + (ok(pa) ? QString("") : ": " + why(pa)));
	sleepMs(2000);
	auto pst = request("GetRecordStatus");
	check(pst["responseData"].toObject()["outputPaused"].toBool(), "GetRecordStatus: outputPaused");
	auto re = request("ResumeRecord");
	check(ok(re), "ResumeRecord" + (ok(re) ? QString("") : ": " + why(re)));
	sleepMs(2000);
	auto sp = request("StopRecord", {}, 20000);
	const QString path = sp["responseData"].toObject()["outputPath"].toString();
	check(ok(sp) && !path.isEmpty(), "StopRecord returned a path: " + path);
	// StopRecord returns the path BEFORE a Hybrid MP4 is finalized: wait for OBS's "stopped" event.
	QElapsedTimer fin;
	fin.start();
	while (stoppedEvent.isEmpty() && fin.elapsed() < 15000) sleepMs(20);
	check(!stoppedEvent.isEmpty(), QString("RecordStateChanged STOPPED event arrived %1 ms after StopRecord replied").arg(fin.elapsed()));
	const QFileInfo fi(path);
	check(fi.exists() && fi.size() > 1000, QString("file exists, %1 bytes").arg(fi.size()));
	say(QString("wall clock start..stop: %1 ms (3 s + 2 s paused + 2 s)").arg(total.elapsed()));
	say(QString("OUTPUT %1").arg(path));
	say(failures ? QString("RESULT: %1 check(s) FAILED").arg(failures) : QString("RESULT: all checks passed"));
	ws.close();
	return failures ? 1 : 0;
}
