#ifndef APPD_H
#define APPD_H

#include <pch.h>

namespace WSS {
class Shell;
}

namespace WSS {
struct Application {
    std::string Id;
    std::string Name;
    std::string Comment;
    std::string Exec;
    std::string IconBase64Large;
    std::string IconBase64Small;
};

class Appd {
    Shell* m_Shell = nullptr;
    std::thread m_Thread;
    std::atomic_bool m_Running{false};

    std::unordered_map<std::string, Application> m_Applications;
    std::mutex m_ApplicationsMutex;

    Application ReadDesktopFile(const std::string& filePath);
    void WatchApplicationDirectory();
    void LoadApplication(const std::string& filePath);

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
        m_Applications.clear();
        WSS_DEBUG("Appd destroyed.");
    }

    Appd(const Appd&) = delete;
    Appd(Appd&&) = delete;
    Appd& operator=(Appd&&) = delete;

    void AddApplication(const std::string& name, const Application& app);
    void RunApplication(const std::string& prefix, const std::string& appId);

    void SendAppIPC(const Application& app);

    [[nodiscard]] const std::unordered_map<std::string, Application>& GetApplications() const { return m_Applications; }

    void Start();
};
} // namespace WSS

#endif // APPD_H
