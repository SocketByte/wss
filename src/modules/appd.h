#ifndef APPD_H
#define APPD_H

#include <pch.h>

namespace WSS {
class Shell;
}

namespace WSS {
class Appd {
    Shell* m_Shell = nullptr;
    std::thread m_Thread;
    std::atomic_bool m_Running{false};

    void WatchApplicationDirectory();

  public:
    explicit Appd(Shell* shell) : m_Shell(shell) {
        WSS_ASSERT(m_Shell != nullptr, "Shell instance must not be null.");
        WSS_DEBUG("Appd initialized with Shell instance.");
    }
    Appd() {
        m_Running = false;
        if (m_Thread.joinable()) {
            m_Thread.join();
        }
        WSS_DEBUG("Appd destroyed.");
    }

    Appd(const Appd&) = delete;
    Appd(Appd&&) = delete;
    Appd& operator=(Appd&&) = delete;

    void Start();
};
} // namespace WSS

#endif // APPD_H
