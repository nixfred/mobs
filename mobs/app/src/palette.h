#pragma once
#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>

class Theme;

// The live Omarchy colour theme (colors.toml) plus Hyprland's corner rounding, for QML.
// Follows theme switches: the file, its folders and the "current" symlink are watched.
class Palette : public QObject {
    Q_OBJECT
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor darkBackground READ darkBackground NOTIFY changed)
    Q_PROPERTY(QColor darkerBackground READ darkerBackground NOTIFY changed)
    Q_PROPERTY(QColor lighterBackground READ lighterBackground NOTIFY changed)
    Q_PROPERTY(QColor selection READ selection NOTIFY changed)
    Q_PROPERTY(QColor foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QColor darkForeground READ darkForeground NOTIFY changed)
    Q_PROPERTY(QColor muted READ muted NOTIFY changed)
    Q_PROPERTY(QColor secondaryText READ secondaryText NOTIFY changed)
    Q_PROPERTY(QColor tertiaryText READ tertiaryText NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QColor red READ red NOTIFY changed)
    Q_PROPERTY(QColor green READ green NOTIFY changed)
    Q_PROPERTY(QColor yellow READ yellow NOTIFY changed)
    Q_PROPERTY(QColor cyan READ cyan NOTIFY changed)
    Q_PROPERTY(int rounding READ rounding NOTIFY changed)
    Q_PROPERTY(QString themeFile READ themeFile NOTIFY changed)
public:
    // currentDirectory is Omarchy's "current" folder; the file read is <dir>/theme/colors.toml.
    // Empty means ~/.local/state/omarchy/current. A non-empty folder also turns off the hyprctl rounding poll.
    explicit Palette(const QString &currentDirectory = {}, QObject *parent = nullptr);
    ~Palette() override;

    // key = "#RRGGBB" lines; anything else (comments, mode = "dark") is ignored.
    static QHash<QString, QColor> parse(const QString &toml);
    static QColor contrasting(const QColor &c);
    static double contrastRatio(const QColor &a, const QColor &b);   // WCAG: 1 (none) to 21
    // c nudged toward the main text colour until it reads at `target`:1 on the window AND card backgrounds.
    QColor readable(const QColor &c, double target = 4.5) const;

    QColor background() const { return get("background"); }
    QColor darkBackground() const { return get("dark_background"); }
    QColor darkerBackground() const { return get("darker_background"); }
    QColor lighterBackground() const { return get("lighter_background"); }
    QColor selection() const { return get("selection"); }
    QColor foreground() const { return get("foreground"); }
    QColor darkForeground() const { return get("dark_foreground"); }
    QColor muted() const { return get("muted"); }
    // Dim text kept readable on any theme (some themes' own dim colours are well under 3:1).
    QColor secondaryText() const { return readable(darkForeground()); }
    QColor tertiaryText() const { return readable(muted()); }
    QColor accent() const { return get("accent"); }
    QColor accentForeground() const { return contrasting(accent()); }
    QColor red() const { return get("red"); }
    QColor green() const { return get("green"); }
    QColor yellow() const { return get("yellow"); }
    QColor cyan() const { return get("cyan"); }
    int rounding() const;
    QString themeFile() const { return m_file; }
    QColor get(const QString &key) const;
signals:
    void changed();
private:
    void reload();
    QString m_directory, m_file;
    QHash<QString, QColor> m_colors;
    Theme *m_theme;   // Monologue's theme.cpp: live Hyprland rounding, reused by path
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
