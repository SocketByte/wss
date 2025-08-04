#ifndef IPC_H
#define IPC_H

#include <App.h>
#include <WebSocket.h>
#include <pch.h>
#include <queue>

namespace WSS {
class Shell;
}
namespace WSS {
struct PendingMessage {
    std::string type;
    std::string json;
};

struct IPCClientInfo {
    int monitorId;
    std::string widgetName;
};

typedef uWS::WebSocket<false, true, IPCClientInfo> WSClient;

class IPC {
    uWS::App* m_App = nullptr;
    Shell* m_Shell = nullptr;

    std::thread m_Thread;
    std::atomic_bool m_Running{false};

    std::thread m_MousePositionThread;
    std::atomic_bool m_MousePositionRunning{false};

    using ListenerCallback = std::function<void(Shell* shell, WSClient* client, const json_object* payload)>;

    std::mutex m_ListenersMutex;
    std::unordered_map<std::string, std::vector<ListenerCallback>> m_Listeners;

    void IPCCallback(WSS::WSClient* ws, std::string_view message, uWS::OpCode opCode);

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
    void Broadcast(const std::string& type, json_object* payload);
    void Send(WSClient* wsi, const std::string& type, json_object* payload);

    void Listen(const std::string& type, ListenerCallback callback) {
        std::lock_guard lock(m_ListenersMutex);
        m_Listeners[type].push_back(std::move(callback));
    }

    void Notify(const std::string& type, Shell* shell, WSClient* client, const json_object* payload) {
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
    [[nodiscard]] Shell* GetShell() { return m_Shell; }
};
} // namespace WSS

#endif // IPC_H
