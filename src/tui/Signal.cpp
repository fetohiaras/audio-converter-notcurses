#include "tui/Signal.hpp"

#include <csignal>

std::atomic_bool g_sigint_received{false};

static void SigintHandler(int) {
    // Async-signal-safe handler: only flip the atomic flag.
    g_sigint_received.store(true, std::memory_order_relaxed);
}

void InitSigintHandler() {
    g_sigint_received.store(false, std::memory_order_relaxed);

    struct sigaction action {};
    action.sa_handler = SigintHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, nullptr);
}
