#ifndef IPC_H
#define IPC_H

#include <pch.h>

namespace WSS {
class Shell;
}
namespace WSS {
struct IPCClientInfo {
    int monitorId;
    std::string widgetName;
};

class IPC {
    Shell* m_Shell = nullptr;
    lws_context* m_Context = nullptr;
    std::thread m_Thread;
    std::atomic_bool m_Running{false};

    std::mutex m_ClientInfoMutex;
    std::unordered_map<lws*, IPCClientInfo> m_ClientInfoMap;

    using ListenerCallback = std::function<void(Shell* shell, const IPCClientInfo* client, const json_object* payload)>;

    std::mutex m_ListenersMutex;
    std::unordered_map<std::string, std::vector<ListenerCallback>> m_Listeners;

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

    void Listen(const std::string& type, ListenerCallback callback) {
        std::lock_guard lock(m_ListenersMutex);
        m_Listeners[type].push_back(std::move(callback));
    }

    void Notify(const std::string& type, Shell* shell, const IPCClientInfo* client, const json_object* payload) {
        std::lock_guard lock(m_ListenersMutex);
        WSS_ASSERT(shell != nullptr, "Shell instance must not be null.");
        WSS_ASSERT(client != nullptr, "IPCClientInfo must not be null.");
        int notifyCount = 0;
        auto it = m_Listeners.find(type);
        if (it != m_Listeners.end()) {
            for (auto& cb : it->second) {
                cb(shell, client, payload);
                notifyCount++;
            }
        }

        if (notifyCount == 0) {
            WSS_WARN("No listeners found for IPC message type: {}", type);
        }
    }

    [[nodiscard]] bool IsRunning() const { return m_Running.load(); }
    [[nodiscard]] lws_context* GetContext() const { return m_Context; }
    [[nodiscard]] Shell* GetShell() { return m_Shell; }
    [[nodiscard]] std::unordered_map<lws*, IPCClientInfo>& GetClientInfoMap() { return m_ClientInfoMap; }

    [[nodiscard]] const IPCClientInfo* GetClientInfo(lws* wsi) {
        std::lock_guard lock(m_ClientInfoMutex);
        auto it = m_ClientInfoMap.find(wsi);
        if (it != m_ClientInfoMap.end()) {
            return &it->second;
        }
        return nullptr;
    }

    [[nodiscard]] std::mutex& GetClientInfoMutex() { return m_ClientInfoMutex; }
};
} // namespace WSS

#endif // IPC_H
