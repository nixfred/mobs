#include "palette.h"
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

static void writeTheme(const QString &current, const QString &toml) {
    QDir().mkpath(current + "/theme");
    QFile f(current + "/theme/colors.toml");
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(toml.toUtf8());
}

class PaletteTests : public QObject {
    Q_OBJECT
private slots:
    void parsesColoursAndIgnoresTheRest() {
        const auto c = Palette::parse("# Liftoff\nmode = \"dark\"\naccent = \"#F3B77B\"  # warm\nred = '#EF8D80'\nbad = \"#12\"\n");
        QCOMPARE(c.value("accent"), QColor("#F3B77B"));
        QCOMPARE(c.value("red"), QColor("#EF8D80"));
        QVERIFY(!c.contains("mode"));
        QVERIFY(!c.contains("bad"));
    }
    void missingFileFallsBackToLiftoff() {
        QTemporaryDir dir;
        Palette p(dir.path());
        QCOMPARE(p.accent(), QColor("#F3B77B"));
        QCOMPARE(p.background(), QColor("#151E26"));
    }
    void readsTheThemeFile() {
        QTemporaryDir dir;
        writeTheme(dir.path(), "accent = \"#112233\"\nbackground = \"#000000\"\n");
        Palette p(dir.path());
        QCOMPARE(p.accent(), QColor("#112233"));
        QCOMPARE(p.background(), QColor("#000000"));
        QCOMPARE(p.foreground(), QColor("#ECE5D8"));   // not in the file: falls back per key
    }
    void followsAThemeSwitch() {
        QTemporaryDir dir;
        writeTheme(dir.path(), "accent = \"#112233\"\n");
        Palette p(dir.path());
        QSignalSpy spy(&p, &Palette::changed);
        writeTheme(dir.path(), "accent = \"#445566\"\nbackground = \"#101010\"\n");
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);
        QCOMPARE(p.accent(), QColor("#445566"));
        QCOMPARE(p.background(), QColor("#101010"));
        // and again: the watcher must have re-armed itself after the first change
        spy.clear();
        writeTheme(dir.path(), "accent = \"#778899\"\n");
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);
        QCOMPARE(p.accent(), QColor("#778899"));
    }
    void followsAThemeFolderBeingReplaced() {
        // Omarchy switches themes by replacing the whole theme folder, not by editing the file.
        QTemporaryDir dir;
        writeTheme(dir.path(), "accent = \"#112233\"\n");
        Palette p(dir.path());
        QSignalSpy spy(&p, &Palette::changed);
        QVERIFY(QDir(dir.path() + "/theme").removeRecursively());
        writeTheme(dir.path(), "accent = \"#AABBCC\"\n");
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);
        QCOMPARE(p.accent(), QColor("#AABBCC"));
    }
    void pickReadableTextOnTheAccent() {
        QCOMPARE(Palette::contrasting(QColor("#FFD60A")), QColor("#000000"));   // bright accent: dark text
        QCOMPARE(Palette::contrasting(QColor("#1E3A8A")), QColor("#FFFFFF"));   // dark accent: light text
    }
    void dimTextIsReadableOnAnyTheme() {
        // Tokyo Night's own dim colours measure 2.35:1 and 1.9:1; the guard must lift them to 4.5:1.
        QTemporaryDir dir;
        writeTheme(dir.path(), "background = \"#1a1b26\"\nlighter_background = \"#24283b\"\nforeground = \"#a9b1d6\"\n"
                               "dark_foreground = \"#565f89\"\nmuted = \"#414868\"\n");
        Palette p(dir.path());
        for (const QColor &c : {p.secondaryText(), p.tertiaryText()}) {
            QVERIFY2(Palette::contrastRatio(c, p.background()) >= 4.5, qPrintable(c.name()));
            QVERIFY2(Palette::contrastRatio(c, p.lighterBackground()) >= 4.5, qPrintable(c.name()));
        }
        QVERIFY(Palette::contrastRatio(p.darkForeground(), p.lighterBackground()) < 4.5);   // the raw colour really was too dim
    }
    void readableColoursAreLeftAlone() {
        QTemporaryDir dir;   // Liftoff fallback: dark_foreground already passes, so it must not change
        Palette p(dir.path());
        QCOMPARE(p.secondaryText(), p.darkForeground());
    }
    void roundingHasASaneFloor() {
        QTemporaryDir dir;
        Palette p(dir.path());   // no hyprctl when a folder is given
        QVERIFY(p.rounding() >= 6);
    }
};
QTEST_MAIN(PaletteTests)
#include "tst_palette.moc"
