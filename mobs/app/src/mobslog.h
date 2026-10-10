#pragma once
#include <QString>

// MOBS's own log: a plain text file that is ALWAYS written (Fred, 2026-10-10: "check the logs, and if you aren't keeping logs turn it on").
// ~/.local/state/mobs/mobs.log (MOBS_LOG_FILE overrides). It keeps what the journal throws away: libobs's informational lines, the size of
// every picture it captures, each state the recorder goes through, every stream state, and Qt's own warnings. Two older logs are kept (4 MB each).
// The stream key is NEVER written: whatever was registered with secret() is replaced by *** in every line.
namespace MobsLog {
void init();                                          // opens the file (rotating a big one) and hooks Qt's messages; call once, early
QString path();                                       // "" when no file could be opened
void write(const char *category, const QString &text);  // "2026-10-10 16:31:02.123 [engine] text". Safe from any thread; a no-op before init()
void secret(const QString &text);                     // scrub this from every later line (the stream key); empty clears it
}
