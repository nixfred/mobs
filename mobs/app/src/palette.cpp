#include "palette.h"
#include "theme.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <cmath>

namespace {
// Omarchy "Liftoff": used for any key a theme file does not set, and when no theme file exists.
const QHash<QString, QColor> &fallback() {
    static const QHash<QString, QColor> f{
        {"background", "#151E26"}, {"dark_background", "#10171E"}, {"darker_background", "#0B1117"},
        {"lighter_background", "#253440"}, {"selection", "#334D5D"}, {"foreground", "#ECE5D8"},
        {"dark_foreground", "#A2ACAE"}, {"muted", "#88979F"}, {"accent", "#F3B77B"}, {"red", "#EF8D80"},
        {"green", "#A9C59A"}, {"yellow", "#EAD093"}, {"cyan", "#8BC8D0"}};
    return f;
}
}

Palette::Palette(const QString &currentDirectory, QObject *parent) : QObject(parent),
    m_directory(currentDirectory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : currentDirectory),
    m_theme(new Theme(currentDirectory, this)) {
    m_file = m_directory + "/theme/colors.toml";
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Palette::reload);
    connect(m_theme, &Theme::changed, this, &Palette::changed);   // rounding or accent moved
    reload();
}
Palette::~Palette() = default;

QHash<QString, QColor> Palette::parse(const QString &toml) {
    QHash<QString, QColor> out;
    static const QRegularExpression line(R"re(^\s*([a-z_]+)\s*=\s*["'](#[0-9a-fA-F]{6})["']\s*(?:#.*)?$)re",
                                         QRegularExpression::MultilineOption);
    for (auto it = line.globalMatch(toml); it.hasNext();) {
        const auto m = it.next();
        out.insert(m.captured(1), QColor(m.captured(2)));
    }
    return out;
}

QColor Palette::contrasting(const QColor &c) {
    auto lin = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    const double lum = .2126 * lin(c.redF()) + .7152 * lin(c.greenF()) + .0722 * lin(c.blueF());
    return lum > .179 ? QColor("#000000") : QColor("#FFFFFF");
}

double Palette::contrastRatio(const QColor &a, const QColor &b) {
    auto lin = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    auto lum = [&](const QColor &c) { return .2126 * lin(c.redF()) + .7152 * lin(c.greenF()) + .0722 * lin(c.blueF()); };
    const double hi = std::max(lum(a), lum(b)), lo = std::min(lum(a), lum(b));
    return (hi + .05) / (lo + .05);
}

QColor Palette::readable(const QColor &c, double target) const {
    const QColor fg = foreground(), bg = background(), card = lighterBackground();
    auto worst = [&](const QColor &x) { return std::min(contrastRatio(x, bg), contrastRatio(x, card)); };
    if (worst(c) >= target) return c;           // already readable: the theme's own colour wins
    for (int i = 1; i <= 20; ++i) {
        const double t = i / 20.0;
        const QColor mix = QColor::fromRgbF(c.redF() + (fg.redF() - c.redF()) * t,
                                            c.greenF() + (fg.greenF() - c.greenF()) * t,
                                            c.blueF() + (fg.blueF() - c.blueF()) * t);
        if (worst(mix) >= target) return mix;
    }
    return fg;
}

QColor Palette::get(const QString &key) const {
    const auto it = m_colors.constFind(key);
    return it != m_colors.constEnd() ? it.value() : fallback().value(key, QColor("#FF00FF"));
}

int Palette::rounding() const { return m_theme->radius() > 0 ? m_theme->radius() : 12; }

void Palette::reload() {
    // The "current" symlink or the theme folder can be replaced by a theme switch: watch the whole chain,
    // and re-add every time (a replaced path drops its watch).
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty()) m_watcher.removePaths(paths);
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/") ancestor = QFileInfo(ancestor).absolutePath();
    QStringList watch{ancestor, QFileInfo(ancestor).absolutePath(), m_directory, m_directory + "/theme", m_file};
    watch.removeDuplicates();
    for (const auto &p : watch) if (QFileInfo::exists(p)) m_watcher.addPath(p);

    QHash<QString, QColor> next;
    if (QFile f(m_file); f.open(QIODevice::ReadOnly)) next = parse(QString::fromUtf8(f.readAll()));
    if (next != m_colors) { m_colors = next; emit changed(); }
}
