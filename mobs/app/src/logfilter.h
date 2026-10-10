#pragma once

// libobs logs at debug level for EVERY camera frame ("ts: ... buf id ..."); written to stderr under a launcher or a
// systemd user session that is ~60 journal lines a second. MOBS keeps warnings and errors, and everything with MOBS_VERBOSE=1.
// The numbers are libobs's LOG_* values; obsbackend.cpp static_asserts they still match (this header stays free of obs.h,
// which cannot share a translation unit with QtTest).
namespace obslog { constexpr int Error = 100, Warning = 200, Info = 300, Debug = 400; }
inline bool obsLogWanted(int level, bool verbose) { return verbose || level <= obslog::Warning; }
