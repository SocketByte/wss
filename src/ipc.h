#ifndef IPC_H
#define IPC_H

#include <pch.h>

namespace WSS {
class Shell;
}
namespace WSS {
class IPC {
    Shell* m_Shell = nullptr;
    lws_context* m_Context = nullptr;
    std::thread m_Thread;
    std::atomic_bool m_Running{false};

  public:
    explicit IPC(Shell* shell) : m_Shell(shell) {
        WSS_ASSERT(m_Shell != nullptr, "Shell instance must not be null.");
        WSS_DEBUG("Initializing IPC with Shell instance.");
    }

    ~IPC();
    IPC(const IPC&) = delete;
    IPC(IPC&&) = delete;
    IPC& operator=(IPC&&) = delete;

    void Start();
};
} // namespace WSS

#endif // IPC_H
