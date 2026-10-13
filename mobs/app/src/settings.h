#pragma once
#include "recorderbackend.h"
#include <QObject>
#include <QSettings>
#include <QString>

// The Pro drawer's choices, remembered between runs. Everything here is safe to change at any time: it takes effect on the next recording.
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(int quality READ quality NOTIFY changed)               // 0 small file, 1 balanced, 2 high quality
    Q_PROPERTY(int crf READ crf NOTIFY changed)
    Q_PROPERTY(bool customQuality READ customQuality NOTIFY changed)
    Q_PROPERTY(QString encoderPreset READ encoderPreset NOTIFY changed)
    Q_PROPERTY(QString folder READ folder NOTIFY changed)             // where recordings go
    Q_PROPERTY(bool folderIsDefault READ folderIsDefault NOTIFY changed)
    Q_PROPERTY(bool micOn READ micOn NOTIFY changed)
    Q_PROPERTY(QString micDevice READ micDevice NOTIFY changed)
    Q_PROPERTY(int micGainDb READ micGainDb NOTIFY changed)
    Q_PROPERTY(bool desktopOn READ desktopOn NOTIFY changed)
    Q_PROPERTY(QString desktopDevice READ desktopDevice NOTIFY changed)
    Q_PROPERTY(int desktopGainDb READ desktopGainDb NOTIFY changed)
    Q_PROPERTY(bool hardware READ hardware NOTIFY changed)
    Q_PROPERTY(bool reviewAfter READ reviewAfter NOTIFY changed)      // open the editor after each recording
    Q_PROPERTY(bool splash READ splash NOTIFY changed)                // play the opening screen when MOBS starts
    Q_PROPERTY(bool stayOut READ stayOut NOTIFY changed)              // MOBS's own window hides itself from the picture while a take records
    Q_PROPERTY(bool pip READ pip NOTIFY changed)                      // "screen + me": the webcam is a window ON the screen (move it with SUPER + drag)
    Q_PROPERTY(bool chat READ chat NOTIFY changed)                    // the chat card under the webcam window, when the broadcast has a chat feed
    Q_PROPERTY(QString streamUrl READ streamUrl NOTIFY changed)       // where a live take is also sent (rtmp:// or rtmps://)
    Q_PROPERTY(bool streamKeySaved READ streamKeySaved NOTIFY changed)   // a stream key is on disk; QML can never read it back
    Q_PROPERTY(bool streamReady READ streamReady NOTIFY changed)       // both the address and the key are set
    Q_PROPERTY(int streamQuality READ streamQuality NOTIFY changed)    // 0 light 720p 3 Mbps, 1 standard 1080p 6 Mbps, 2 X recommended 1080p30 9 Mbps (the default)
    Q_PROPERTY(bool setupDone READ setupDone NOTIFY changed)           // the guided setup was finished once (it can be opened again any time)
    Q_PROPERTY(int setupStep READ setupStep NOTIFY changed)            // where the guided setup stopped, so it picks up there next time
    Q_PROPERTY(bool xSignedIn READ xSignedIn NOTIFY changed)           // the person signed in to X inside MOBS, so the chat can show under the webcam
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY changed)      // what the person calls their source on X: shown again when they go live, so they pick the right one
    Q_PROPERTY(bool micLocked READ micLocked NOTIFY changed)           // the person pressed "this is my microphone": it stays until they change it
    Q_PROPERTY(QString aboutText READ aboutText CONSTANT)
public:
    // streamKeyFile is where the stream key is kept (owner-only). Empty = nowhere, so a test can never write into a real config.
    explicit Settings(QSettings *settings, QObject *parent = nullptr, QString streamKeyFile = QString());
    int quality() const { return m_quality; }
    int crf() const { return m_customCrf < 0 ? crfFor(m_quality) : m_customCrf; }
    bool customQuality() const { return m_customCrf >= 0; }
    QString encoderPreset() const { return m_preset; }
    QString folder() const;
    bool folderIsDefault() const { return m_folder.isEmpty(); }
    bool micOn() const { return m_mic; }
    QString micDevice() const { return m_audio.micDevice; }
    int micGainDb() const { return m_audio.micGainDb; }
    bool desktopOn() const { return m_audio.desktop; }
    QString desktopDevice() const { return m_audio.desktopDevice; }
    int desktopGainDb() const { return m_audio.desktopGainDb; }
    bool hardware() const { return m_hardware; }
    AudioSpec audioSpec() const { return m_audio; }
    Q_INVOKABLE void setMicDevice(const QString &id);
    Q_INVOKABLE void setMicGainDb(int db);
    Q_INVOKABLE void setDesktopOn(bool on);
    Q_INVOKABLE void setDesktopDevice(const QString &id);
    Q_INVOKABLE void setDesktopGainDb(int db);
    Q_INVOKABLE void setHardware(bool on);
    bool reviewAfter() const { return m_review; }
    bool splash() const { return m_splash; }
    bool pip() const { return m_pip; }
    bool stayOut() const { return m_stayOut; }
    bool chat() const { return m_chat; }
    bool chatRelay() const { return m_chatRelay; }           // listen for the chat that another computer sends (INI key chatRelay, off unless asked for)
    int chatRelayPort() const { return m_chatRelayPort; }
    bool setupDone() const { return m_setupDone; }
    int setupStep() const { return m_setupStep; }
    bool micLocked() const { return m_micLocked; }
    QString sourceName() const { return m_sourceName; }
    Q_INVOKABLE void setSourceName(const QString &name);
    static QString cleanSourceName(const QString &name);   // plain words only, at most 40 characters: it is shown as text
    bool xSignedIn() const { return m_xSignedIn; }
    Q_INVOKABLE void setXSignedIn(bool in);
    Q_INVOKABLE void noteXUrl(const QString &url);      // the sign-in browser shows a page: X's own home page means the person is signed in
    static bool xHomeUrl(const QString &url);
    Q_INVOKABLE void setSetupDone(bool done);
    Q_INVOKABLE void setSetupStep(int step);
    Q_INVOKABLE void lockMic(const QString &id);        // "this is my microphone": kept until the person changes it, and the next launch asks nothing
    Q_INVOKABLE void unlockMic();
    Q_INVOKABLE QString pasteStreamKeyFromClipboard();  // the key the person just copied in Live Studio: "" when it was saved, else a sentence saying why not    // INI key chatRelayPort, 8799 by default
    QString aboutText() const;
    QString streamUrl() const { return m_streamUrl; }
    bool streamKeySaved() const { return m_keySaved; }
    bool streamReady() const { return m_keySaved && !m_streamUrl.isEmpty(); }
    int streamQuality() const { return m_streamQuality; }
    // What a live take needs, key included. C++ only, on purpose: the key is never a property QML (or a screenshot) can reach.
    StreamSpec streamSpec() const;
    // Both return a sentence saying what is wrong, or "" when the value was saved.
    Q_INVOKABLE QString setStreamUrl(const QString &url);
    Q_INVOKABLE QString setStreamKey(const QString &key);
    Q_INVOKABLE void clearStreamKey();
    Q_INVOKABLE void setStreamQuality(int quality);
    static QString streamUrlProblem(const QString &url);
    // X's Live Studio, US East (N. Virginia): what it shows for a new RTMP livestream. The default, so only the stream key has to be pasted.
    static inline const QString kXUsEast = QStringLiteral("rtmp://va.pscp.tv:80/x");
    static QString defaultFolder();
    static int crfFor(int quality);       // x264 constant rate factor: lower = bigger and better
    Q_INVOKABLE void setQuality(int quality);
    Q_INVOKABLE void setCrf(int crf);
    Q_INVOKABLE void setEncoderPreset(const QString &preset);
    Q_INVOKABLE void resetEncoding();
    Q_INVOKABLE void setFolder(const QString &folder);   // "" = back to the default
    Q_INVOKABLE void setMicOn(bool on);
    Q_INVOKABLE void setReviewAfter(bool on);
    Q_INVOKABLE void setSplash(bool on);
    Q_INVOKABLE void setPip(bool on);
    Q_INVOKABLE void setStayOut(bool on);
    Q_INVOKABLE void setChat(bool on);
    Q_INVOKABLE QString problem(const QString &folder) const { return problemWith(folder); }   // for the folder field in QML
    Q_INVOKABLE void openFolder();        // show the recordings folder in the file manager
    // Empty string if the folder can be used, otherwise a sentence saying why not (created if missing).
    static QString problemWith(const QString &folder);
signals:
    void changed();
private:
    QSettings *m_settings;
    int m_quality = 1, m_customCrf = -1;
    QString m_preset = "veryfast";
    QString m_folder;
    AudioSpec m_audio;
    bool m_hardware = false;
    bool m_mic = true, m_review = true, m_splash = true, m_pip = true, m_stayOut = true, m_chat = true;
    bool m_chatRelay = false;
    int m_chatRelayPort = 8799;
    bool m_setupDone = false, m_micLocked = false, m_xSignedIn = false;
    int m_setupStep = 0;
    QString m_sourceName;
    QString m_streamUrl = kXUsEast, m_streamKeyFile;
    bool m_keySaved = false;
    int m_streamQuality = 2;
    QString readStreamKey() const;
};
