#include "review.h"
#include "mediautils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <algorithm>
#include <cstdio>

Review::Review(QObject *parent) : QObject(parent) {
    connect(&m_thumbs, &Thumbnails::changed, this, &Review::openChanged);   // a new filmstrip image: let QML look again
}

Review::~Review() {
    if (m_export) { m_export->kill(); m_export->waitForFinished(2000); }
    if (!m_partial.isEmpty()) QFile::remove(m_partial);
}

QString Review::title() const { return m_open ? QFileInfo(m_path).completeBaseName() : QString(); }

QVariantList Review::clips() const {
    QVariantList out;
    for (const auto &c : m_edit) out.append(QVariantMap{{"start", c.start}, {"end", c.end}});
    return out;
}

QVariantList Review::pauses() const {
    QVariantList out;
    for (double p : m_pauses) if (p > 0 && p < m_duration) out.append(p);
    return out;
}

QString Review::sidecarPath(const QString &take) { return take + ".mobs.json"; }

QList<double> Review::readPauses(const QString &take) {
    QList<double> out;
    QFile f(sidecarPath(take));
    if (!f.open(QIODevice::ReadOnly)) return out;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    for (const auto &v : doc.object().value("pauses").toArray()) if (v.isDouble()) out.append(v.toDouble());
    return out;
}

bool Review::writePauses(const QString &take, const QList<double> &pauses) {
    QJsonArray a;
    for (double p : pauses) a.append(p);
    QFile f(sidecarPath(take));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(QJsonObject{{"version", 1}, {"pauses", a}}).toJson(QJsonDocument::Compact));
    return true;
}

QString Review::editedPathFor(const QString &take) {
    const QFileInfo fi(take);
    const QString base = fi.absolutePath() + "/" + fi.completeBaseName() + " (edited)";
    QString path = base + ".mp4";
    for (int n = 2; QFileInfo::exists(path); ++n) path = base + " " + QString::number(n) + ".mp4";
    return path;
}

void Review::setMessage(const QString &m) { m_message = m; emit saveChanged(); }

bool Review::open(const QString &path) {
    if (m_export) return false;                                  // never swap the take under a running save
    const auto probe = media::probe(path);
    if (!probe.ok || probe.duration <= 0) {
        setMessage("could not open this recording" + (probe.error.isEmpty() ? QString() : ": " + probe.error));
        return false;
    }
    m_path = QFileInfo(path).absoluteFilePath();
    m_duration = probe.duration;
    m_audio = probe.audio;
    m_pauses = readPauses(m_path);
    m_edit = edit::whole(m_duration);
    m_savedEdit.clear();
    m_undo.clear(); m_redo.clear();
    m_gesture = false; m_progress = 0; m_savedPath.clear(); m_message.clear();
    m_open = true;
    m_thumbs.load(m_path, m_duration);
    emit openChanged(); emit editChanged(); emit saveChanged();
    return true;
}

void Review::close() {
    if (!m_open || m_export) return;
    m_open = false;
    m_thumbs.clear();
    m_edit.clear(); m_undo.clear(); m_redo.clear(); m_pauses.clear();
    emit openChanged(); emit editChanged();
}

void Review::applyEdit(edit::Clips next) {
    if (!m_open || m_export) return;
    if (!m_gesture) next = edit::normalized(next, m_duration);    // mid-drag clips keep their indices; the gesture's end tidies up
    if (next == m_edit || next.isEmpty()) return;
    if (!m_gesture || !m_gestureRecorded) { m_undo.append(m_edit); m_redo.clear(); m_gestureRecorded = m_gesture; }
    m_edit = next;
    emit editChanged();
}

void Review::split(double time) {
    for (int i = 0; i < m_edit.size(); ++i) {
        const auto clip = m_edit[i];
        if (time - clip.start < edit::minimumClip || clip.end - time < edit::minimumClip) continue;
        auto next = m_edit;
        next[i].end = time;
        next.insert(i + 1, {time, clip.end});
        applyEdit(next);
        return;
    }
}

void Review::setClip(int index, double start, double end) {
    if (index < 0 || index >= m_edit.size()) return;
    const double low = index > 0 ? m_edit[index - 1].end : 0;                                  // may grow into a gap...
    const double high = index + 1 < m_edit.size() ? m_edit[index + 1].start : m_duration;      // ...never over a neighbour
    start = std::clamp(start, low, high);
    end = std::clamp(end, low, high);
    if (end - start < edit::minimumClip) return;
    auto next = m_edit;
    next[index] = {start, end};
    applyEdit(next);
}

void Review::removeClip(int index) {
    if (index < 0 || index >= m_edit.size() || m_edit.size() < 2) return;   // the last clip cannot be removed
    auto next = m_edit;
    next.removeAt(index);
    applyEdit(next);
}

void Review::joinClips(int index) {
    if (index < 0 || index + 1 >= m_edit.size()) return;
    auto next = m_edit;
    next[index].end = next[index + 1].end;
    next.removeAt(index + 1);
    applyEdit(next);
}

void Review::beginGesture() { if (m_open) { m_gesture = true; m_gestureRecorded = false; } }

void Review::endGesture() {
    if (!m_gesture) return;
    m_gesture = false;
    const auto next = edit::normalized(m_edit, m_duration);
    if (!next.isEmpty() && next != m_edit) { m_edit = next; emit editChanged(); }   // never let tidying empty the take
}

void Review::undo() {
    if (!m_open || m_gesture || m_undo.isEmpty() || m_export) return;
    m_redo.append(m_edit);
    m_edit = m_undo.takeLast();
    emit editChanged();
}

void Review::redo() {
    if (!m_open || m_gesture || m_redo.isEmpty() || m_export) return;
    m_undo.append(m_edit);
    m_edit = m_redo.takeLast();
    emit editChanged();
}

void Review::save() {
    if (!m_open || m_export) return;
    if (!changed()) { setMessage("nothing to save: the recording is untouched"); return; }
    const QString destination = editedPathFor(m_path);
    // Encode into a hidden partial file beside the destination, then rename it into place: no half-written file ever has the real name.
    const QFileInfo target(destination);
    QTemporaryFile partial(target.absolutePath() + "/." + target.completeBaseName() + "-XXXXXX.mp4");
    partial.setAutoRemove(false);
    if (!partial.open()) { setMessage("could not save: " + partial.errorString()); return; }
    m_partial = partial.fileName();
    partial.close();
    const double total = keptDuration();
    m_progress = 0;
    setMessage("saving your edit...");
    m_export = new QProcess(this);
    connect(m_export, &QProcess::readyReadStandardOutput, this, [this, total] {
        while (m_export && m_export->canReadLine()) {
            const auto line = m_export->readLine().trimmed();
            if (line.startsWith("out_time_us=") && total > 0) {
                m_progress = std::clamp(line.mid(12).toDouble() / 1000000.0 / total, 0.0, 1.0);
                emit saveChanged();
            }
        }
    });
    connect(m_export, &QProcess::finished, this, [this, destination](int code, QProcess::ExitStatus status) {
        auto *p = std::exchange(m_export, nullptr);
        p->deleteLater();
        QString error;
        if (status != QProcess::NormalExit || code != 0) {
            error = QString::fromUtf8(p->readAllStandardError()).trimmed().left(300);
            if (error.isEmpty()) error = "ffmpeg could not encode the clip";
        } else if (std::rename(QFile::encodeName(m_partial).constData(), QFile::encodeName(destination).constData()) != 0) {
            error = "could not move the finished file into place";
        }
        finishSave(destination, error);
    });
    connect(m_export, &QProcess::errorOccurred, this, [this, destination](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        m_export->deleteLater();
        m_export = nullptr;
        finishSave(destination, "ffmpeg could not be started");
    });
    m_export->start("ffmpeg", media::exportArgs(m_path, m_partial, edit::kept(m_edit), m_audio));
    emit saveChanged();
}

void Review::finishSave(const QString &destination, const QString &error) {
    if (!error.isEmpty()) {
        QFile::remove(m_partial);
        m_partial.clear();
        setMessage("could not save: " + error);
        return;
    }
    m_partial.clear();
    m_progress = 1;
    m_savedPath = destination;
    m_savedEdit = m_edit;                 // what is on disk now: leaving is safe
    setMessage("saved " + destination);
    emit editChanged();
    emit saved(destination);
}

bool Review::deleteTake() {
    if (!m_open || m_export) return false;
    const QString path = m_path;
    const bool ok = QFile::moveToTrash(path);              // the trash, not rm: this is the user's recording
    if (ok) QFile::moveToTrash(sidecarPath(path));
    if (!ok) { setMessage("could not move the recording to the trash"); return false; }
    m_savedEdit = m_edit;
    close();
    return true;
}
