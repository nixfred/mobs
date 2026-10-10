#pragma once
#include <QString>

// A desktop notification, for the moments a person who is looking at another window needs to know: connected, dropped, not live, saved.
// It is written to the log too, is silent when MOBS is not on Wayland (tests), and never carries a secret.
namespace Notifier {
void send(const QString &title, const QString &body, bool important = false);   // important = stays up longer
}
