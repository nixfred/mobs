#pragma once
#include "edit.h"
#include "thumbnails.h"
#include <QObject>
#include <QProcess>
#include <QUrl>
#include <QVariantList>

// Reviewing one finished take: trim and cut it, then save the edited version. The original recording is never modified.
// Presents the interface Monologue's EditBar.qml expects of its "backend" (duration, clips, pauses, thumbnails and the
// edit calls) and follows Monologue's editing rules: a handle drag is ONE undo step, a clip may grow into a gap but never
// over its neighbours, and an export is encoded to a hidden partial file and renamed into place.
class Review : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QUrl clip READ clip NOTIFY openChanged)
    Q_PROPERTY(QString title READ title NOTIFY openChanged)
    Q_PROPERTY(double duration READ duration NOTIFY openChanged)
    Q_PROPERTY(QVariantList clips READ clips NOTIFY editChanged)
    Q_PROPERTY(QVariantList pauses READ pauses NOTIFY openChanged)
    Q_PROPERTY(QObject *thumbnails READ thumbnails CONSTANT)
    Q_PROPERTY(double keptDuration READ keptDuration NOTIFY editChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY editChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY editChanged)
    Q_PROPERTY(bool unsaved READ unsaved NOTIFY editChanged)        // edits that no saved file contains yet
    Q_PROPERTY(bool changed READ changed NOTIFY editChanged)        // differs from the untouched recording
    Q_PROPERTY(bool saving READ saving NOTIFY saveChanged)
    Q_PROPERTY(double saveProgress READ saveProgress NOTIFY saveChanged)
    Q_PROPERTY(QString message READ message NOTIFY saveChanged)
    Q_PROPERTY(QString savedPath READ savedPath NOTIFY saveChanged)
public:
    explicit Review(QObject *parent = nullptr);
    ~Review() override;
    bool isOpen() const { return m_open; }
    bool loading() const { return m_probe != nullptr; }
    QUrl clip() const { return QUrl::fromLocalFile(m_path); }
    QString title() const;
    double duration() const { return m_duration; }
    QVariantList clips() const;
    QVariantList pauses() const;
    QObject *thumbnails() { return &m_thumbs; }
    Thumbnails *thumbs() { return &m_thumbs; }
    double keptDuration() const { return edit::keptDuration(m_edit); }
    bool canUndo() const { return !m_undo.isEmpty() && !m_gesture; }
    bool canRedo() const { return !m_redo.isEmpty() && !m_gesture; }
    bool unsaved() const { return m_open && changed() && edit::kept(m_edit) != edit::kept(m_savedEdit); }
    bool changed() const { return m_open && !edit::untouched(m_edit, m_duration); }
    bool saving() const { return m_export != nullptr; }
    double saveProgress() const { return m_progress; }
    QString message() const { return m_message; }
    QString savedPath() const { return m_savedPath; }

    // The pause points the engine recorded next to a take: <take>.mobs.json
    static QString sidecarPath(const QString &take);
    static QList<double> readPauses(const QString &take);
    static bool writePauses(const QString &take, const QList<double> &pauses);
    // "<dir>/<name> (edited).mp4", or " 2", " 3"... if that already exists.
    static QString editedPathFor(const QString &take);

    // Starts asynchronous inspection; opened reports success once inspection finishes.
    Q_INVOKABLE bool open(const QString &path);
    Q_INVOKABLE void close();
    Q_INVOKABLE void split(double time);
    Q_INVOKABLE void setClip(int index, double start, double end);
    Q_INVOKABLE void removeClip(int index);
    Q_INVOKABLE void joinClips(int index);
    Q_INVOKABLE void beginGesture();
    Q_INVOKABLE void endGesture();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void save();           // write the edited version beside the original; the original stays
    Q_INVOKABLE bool deleteTake();     // move the original (and its sidecar) to the trash
signals:
    void loadingChanged();
    void opened(bool success);
    void openFailed(const QString &message);
    void openChanged();
    void editChanged();
    void saveChanged();
    void saved(const QString &path);
private:
    void applyEdit(edit::Clips next);
    void setMessage(const QString &m);
    void finishSave(const QString &destination, const QString &error);
    void finishOpen(QProcess *process, const QString &path, const QString &error = {});
    void cancelOpen();
    void releaseProbe(QProcess *process);
    QString m_path, m_message, m_savedPath, m_partial;
    bool m_open = false, m_audio = false, m_gesture = false, m_gestureRecorded = false;
    double m_duration = 0, m_progress = 0;
    QList<double> m_pauses;
    edit::Clips m_edit, m_savedEdit;
    QList<edit::Clips> m_undo, m_redo;
    Thumbnails m_thumbs;
    QProcess *m_export = nullptr;
    QProcess *m_probe = nullptr;
};
