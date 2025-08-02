#include "ipc.h"
#include "shell.h"

#define JSON_GET_STR(obj, key) json_object_get_string(json_object_object_get(obj, key))
#define JSON_GET_INT(obj, key) json_object_get_int(json_object_object_get(obj, key))
#define JSON_GET_BOOL(obj, key) json_object_get_boolean(json_object_object_get(obj, key))
#define JSON_GET_OBJ(obj, key) json_object_object_get(obj, key)
#define JSON_GET_ARRAY(obj, key) json_object_object_get(obj, key)

static int callback_ws(lws* wsi, const lws_callback_reasons reason, void* user, void* in, const size_t len) {
    auto* shell = static_cast<WSS::Shell*>(lws_context_user(lws_get_context(wsi)));
    if (!shell) {
        WSS_ERROR("Shell instance is null in WebSocket callback.");
        return -1;
    }

    auto& ipc = shell->GetIPC();

    switch (reason) {
    case LWS_CALLBACK_RECEIVE: {
        std::string message(static_cast<const char*>(in), len);
        json_object* jobj = json_tokener_parse(message.c_str());
        if (!jobj) {
            WSS_ERROR("Failed to parse JSON from received message.");
            return -1;
        }

        json_object *type_obj, *payload_obj;
        if (!json_object_object_get_ex(jobj, "type", &type_obj) || !json_object_object_get_ex(jobj, "payload", &payload_obj)) {
            WSS_ERROR("Received JSON does not contain 'type' or 'payload' fields.");
            json_object_put(jobj);
            return -1;
        }

        const auto type = std::string(json_object_get_string(type_obj));
        if (type == "handshake") {
            int monitorId = json_object_get_int(json_object_object_get(payload_obj, "monitorId"));
            std::string widgetName = json_object_get_string(json_object_object_get(payload_obj, "widgetName"));
            {
                std::lock_guard lock(ipc.GetClientInfoMutex());
                ipc.GetClientInfoMap().emplace(wsi, WSS::IPCClientInfo{wsi, monitorId, widgetName});
            }
            WSS_DEBUG("Client identified with monitor ID: {}, widget name: {}", monitorId, widgetName);
            break;
        }

        const WSS::IPCClientInfo* clientInfo = ipc.GetClientInfo(wsi);
        if (!clientInfo) {
            WSS_WARN("No client info found for WebSocket connection.");
        }

        ipc.Notify(type, shell, clientInfo, payload_obj);
        break;
    }
    case LWS_CALLBACK_CLOSED: {

        // Remove client info on close
        {
            std::lock_guard lock(ipc.GetClientInfoMutex());
            auto& clientInfoMap = ipc.GetClientInfoMap();
            auto it = clientInfoMap.find(wsi);
            if (it != clientInfoMap.end()) {
                clientInfoMap.erase(it);
            }
        }
        break;
    }
    }

    return 0;
}

WSS::IPC::~IPC() {
    if (m_Running) {
        m_Running = false;
        if (m_Context) {
            lws_cancel_service(m_Context); // This wakes up lws_service()
        }
        if (m_Thread.joinable()) {
            m_Thread.join();
        }
    }
    WSS_DEBUG("IPC context destroyed and resources cleaned up.");
}

void WSS::IPC::Start() {
    WSS_ASSERT(m_Shell != nullptr, "Shell instance must not be null.");

    Listen("window-update-click-region", [this](Shell* shell, const IPCClientInfo* client, const json_object* payload) {
        int monitorId = client->monitorId;
        std::string widgetName = client->widgetName;

        auto widget = shell->GetWidget(widgetName);
        if (!widget) {
            WSS_ERROR("Widget '{}' not found for monitor ID: {}", widgetName, monitorId);
            return;
        }
        const auto regionName = JSON_GET_STR(payload, "name");
        const int x = JSON_GET_INT(payload, "x");
        const int y = JSON_GET_INT(payload, "y");
        const int width = JSON_GET_INT(payload, "width");
        const int height = JSON_GET_INT(payload, "height");

        const WidgetClickRegionInfo regionInfo{.X = x, .Y = y, .Width = width, .Height = height};
        widget->SetClickableRegion(monitorId, regionName, regionInfo);
    });

    Listen("notifd-notification-dismiss", [this](Shell* shell, const IPCClientInfo* client, const json_object* payload) {
        uint32_t id = JSON_GET_INT(payload, "id");
        shell->GetNotifd().SignalNotificationClosed(id, NotificationCloseReason::DISMISSED);
    });

    m_Running = true;
    m_Thread = std::thread([this]() {
        try {
            lws_protocols protocols[] = {{"wss.ipc", callback_ws, 0, 4096}, {nullptr, nullptr, 0, 0}};

            lws_context_creation_info info{};
            info.port = m_Shell->GetSettings().m_IpcPort;
            info.protocols = protocols;
            info.gid = -1;
            info.uid = -1;
            info.user = m_Shell;

            m_Context = lws_create_context(&info);
            if (!m_Context) {
                WSS_ERROR("Failed to create IPC context");
                return;
            }

            while (m_Running) {
                int ret = lws_service(m_Context, 0);
                if (ret < 0) {
                    WSS_ERROR("IPC service error: {}", ret);
                    break;
                }
            }

            WSS_DEBUG("IPC service loop exited, cleaning up resources.");
        } catch (const std::exception& e) {
            WSS_ERROR("Unhandled exception in IPC thread: {}", e.what());
        } catch (...) {
            WSS_ERROR("Unknown exception occurred in IPC thread.");
        }
    });
}

void WSS::IPC::Broadcast(const std::string& type, json_object* payload) {
    std::lock_guard lock(GetClientInfoMutex());

    // Compose the JSON message
    json_object* message = json_object_new_object();
    json_object_object_add(message, "type", json_object_new_string(type.c_str()));
    json_object_object_add(message, "payload", json_object_get(payload));

    const std::string messageStr = json_object_to_json_string(message);

    for (const auto& [wsi, clientInfo] : GetClientInfoMap()) {
        if (!wsi)
            continue;

        size_t len = LWS_PRE + messageStr.size();
        std::vector<unsigned char> buf(len);
        memcpy(&buf[LWS_PRE], messageStr.c_str(), messageStr.size());

        int n = lws_write(wsi, &buf[LWS_PRE], messageStr.size(), LWS_WRITE_TEXT);
        if (n < 0) {
            WSS_WARN("Failed to broadcast message to a client.");
        }
    }

    WSS_DEBUG("Broadcasted IPC message of type: {}: {}", type, messageStr);
    json_object_put(message);
}
void WSS::IPC::Send(lws* wsi, const std::string& type, json_object* payload) {
    WSS_ASSERT(wsi != nullptr, "WebSocket instance must not be null.");
    WSS_ASSERT(!type.empty(), "Message type must not be empty.");
    WSS_ASSERT(payload != nullptr, "Payload must not be null.");

    // Compose the JSON message
    json_object* message = json_object_new_object();
    json_object_object_add(message, "type", json_object_new_string(type.c_str()));
    json_object_object_add(message, "payload", json_object_get(payload));

    const std::string messageStr = json_object_to_json_string(message);

    size_t len = LWS_PRE + messageStr.size();
    std::vector<unsigned char> buf(len);
    memcpy(&buf[LWS_PRE], messageStr.c_str(), messageStr.size());

    int n = lws_write(wsi, &buf[LWS_PRE], messageStr.size(), LWS_WRITE_TEXT);
    if (n < 0) {
        WSS_ERROR("Failed to send IPC message: {}", type);
    }

    WSS_DEBUG("Sent IPC message of type: {}: {}", type, messageStr);
    json_object_put(message);
}