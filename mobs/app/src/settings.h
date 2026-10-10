#pragma once
#include <QObject>
#include <QSettings>
#include <QString>

// The Pro drawer's choices, remembered between runs. Everything here is safe to change at any time: it takes effect on the next recording.
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(int quality READ quality NOTIFY changed)               // 0 small file, 1 balanced, 2 high quality
    Q_PROPERTY(QString folder READ folder NOTIFY changed)             // where recordings go
    Q_PROPERTY(bool folderIsDefault READ folderIsDefault NOTIFY changed)
    Q_PROPERTY(bool micOn READ micOn NOTIFY changed)
    Q_PROPERTY(bool reviewAfter READ reviewAfter NOTIFY changed)      // open the editor after each recording
    Q_PROPERTY(QString aboutText READ aboutText CONSTANT)
public:
    explicit Settings(QSettings *settings, QObject *parent = nullptr);
    int quality() const { return m_quality; }
    QString folder() const;
    bool folderIsDefault() const { return m_folder.isEmpty(); }
    bool micOn() const { return m_mic; }
    bool reviewAfter() const { return m_review; }
    QString aboutText() const;
    static QString defaultFolder();
    static int crfFor(int quality);       // x264 constant rate factor: lower = bigger and better
    Q_INVOKABLE void setQuality(int quality);
    Q_INVOKABLE void setFolder(const QString &folder);   // "" = back to the default
    Q_INVOKABLE void setMicOn(bool on);
    Q_INVOKABLE void setReviewAfter(bool on);
    Q_INVOKABLE QString problem(const QString &folder) const { return problemWith(folder); }   // for the folder field in QML
    Q_INVOKABLE void openFolder();        // show the recordings folder in the file manager
    // Empty string if the folder can be used, otherwise a sentence saying why not (created if missing).
    static QString problemWith(const QString &folder);
signals:
    void changed();
private:
    QSettings *m_settings;
    int m_quality = 1;
    QString m_folder;
    bool m_mic = true, m_review = true;
};
