#ifndef TUI_SIGNAL_HPP
#define TUI_SIGNAL_HPP

#include <atomic>

// Global flag flipped by the SIGINT handler so the main loop can exit safely.
extern std::atomic_bool g_sigint_received;

// Installs a minimal SIGINT handler that only sets the flag (async-signal-safe).
void InitSigintHandler();

#endif // TUI_SIGNAL_HPP
