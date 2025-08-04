#include "ipc.h"
#include "shell.h"
#include <WebSocket.h>
#include <queue>

#define JSON_GET_STR(obj, key) json_object_get_string(json_object_object_get(obj, key))
#define JSON_GET_INT(obj, key) json_object_get_int(json_object_object_get(obj, key))
#define JSON_GET_BOOL(obj, key) json_object_get_boolean(json_object_object_get(obj, key))
#define JSON_GET_OBJ(obj, key) json_object_object_get(obj, key)
#define JSON_GET_ARRAY(obj, key) json_object_object_get(obj, key)

void WSS::IPC::IPCCallback(WSClient* ws, std::string_view message, uWS::OpCode opCode) {
    std::string msgStr(message);
    json_object* jobj = json_tokener_parse(msgStr.c_str());
    if (!jobj) {
        WSS_ERROR("Failed to parse JSON from received message.");
        ws->close();
        return;
    }

    json_object *type_obj, *payload_obj;
    if (!json_object_object_get_ex(jobj, "type", &type_obj) || !json_object_object_get_ex(jobj, "payload", &payload_obj)) {
        WSS_ERROR("Received JSON does not contain 'type' or 'payload' fields.");
        json_object_put(jobj);
        ws->close();
        return;
    }

    std::string type = json_object_get_string(type_obj);
    if (type == "handshake") {
        int monitorId = json_object_get_int(json_object_object_get(payload_obj, "monitorId"));
        std::string widgetName = json_object_get_string(json_object_object_get(payload_obj, "widgetName"));

        ws->getUserData()->monitorId = monitorId;
        ws->getUserData()->widgetName = widgetName;
        WSS_DEBUG("Client identified with monitor ID: {}, widget name: {}", monitorId, widgetName);
        json_object_put(jobj);
        return;
    }

    if (!ws->getUserData()) {
        WSS_WARN("No client info found for WebSocket connection.");
        json_object_put(jobj);
        return;
    }

    m_Shell->GetIPC().Notify(type, m_Shell, ws, payload_obj);

    json_object_put(jobj);
}

WSS::IPC::~IPC() {
    if (m_Running) {
        m_Running = false;
        m_App->close();
        if (m_Thread.joinable()) {
            m_Thread.join();
        }
    }
    WSS_DEBUG("IPC context destroyed and resources cleaned up.");
}

void WSS::IPC::Start() {
    WSS_ASSERT(m_Shell != nullptr, "Shell instance must not be null.");

    Listen("window-update-click-region", [this](Shell* shell, WSClient* client, const json_object* payload) {
        int monitorId = client->getUserData()->monitorId;
        std::string widgetName = client->getUserData()->widgetName;

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

        const WidgetClickRegionInfo regionInfo{
            .X = x, .Y = y, .Width = width, .Height = height, ._QT_padding = widget->GetInfo()._QT_padding};
        widget->SetClickableRegion(monitorId, regionName, regionInfo);
    });

    Listen("notifd-notification-dismiss", [this](Shell* shell, WSClient* client, const json_object* payload) {
        uint32_t id = JSON_GET_INT(payload, "id");
        shell->GetNotifd().SignalNotificationClosed(id, NotificationCloseReason::DISMISSED);
    });

    Listen("notifd-notification-action", [this](Shell* shell, WSClient* client, const json_object* payload) {
        uint32_t id = JSON_GET_INT(payload, "id");
        std::string action = JSON_GET_STR(payload, "action");
        shell->GetNotifd().SignalActionInvoked(id, action);
    });

    Listen("appd-application-run", [this](Shell* shell, WSClient* client, const json_object* payload) {
        std::string prefix = JSON_GET_STR(payload, "prefix");
        std::string appId = JSON_GET_STR(payload, "appId");
        WSS_DEBUG("Running application with ID: {} using prefix: {}", appId, prefix);
        shell->GetAppd().RunApplication(prefix, appId);
    });

    Listen("appd-application-list-request", [this](Shell* shell, WSClient* client, const json_object* payload) {
        json_object* response = json_object_new_array();
        for (const auto& [name, app] : shell->GetAppd().GetApplications()) {
            json_object* appObj = json_object_new_object();
            json_object_object_add(appObj, "id", json_object_new_string(app.Id.c_str()));
            json_object_object_add(appObj, "name", json_object_new_string(app.Name.c_str()));
            json_object_object_add(appObj, "comment", json_object_new_string(app.Comment.c_str()));
            json_object_object_add(appObj, "exec", json_object_new_string(app.Exec.c_str()));
            json_object_object_add(appObj, "iconBase64Large", json_object_new_string(app.IconBase64Large.c_str()));
            json_object_object_add(appObj, "iconBase64Small", json_object_new_string(app.IconBase64Small.c_str()));
            json_object_array_add(response, appObj);
        }
        Send(client, "appd-application-list-response", response);
        json_object_put(response);
    });

    Listen("monitor-info-request", [this](Shell* shell, WSClient* client, const json_object* payload) {
        auto monitor = shell->GetWidget(client->getUserData()->widgetName)->GetMonitorInfo(client->getUserData()->monitorId);

        json_object* response = json_object_new_object();
        json_object_object_add(response, "id", json_object_new_int(client->getUserData()->monitorId));
        json_object_object_add(response, "width", json_object_new_int(GetScreenWidth(client->getUserData()->monitorId)));
        json_object_object_add(response, "height", json_object_new_int(GetScreenHeight(client->getUserData()->monitorId)));
        Send(client, "monitor-info-response", response);
        json_object_put(response);
    });

    Listen("widget-set-keyboard-interactivity", [this](Shell* shell, WSClient* client, const json_object* payload) {
        std::string widgetName = client->getUserData()->widgetName;
        const int monitorId = client->getUserData()->monitorId;
        const auto widget = shell->GetWidget(widgetName);
        if (!widget) {
            WSS_ERROR("Widget '{}' not found for setting keyboard interactivity.", widgetName);
            return;
        }

        const bool interactive = JSON_GET_BOOL(payload, "interactive");
        widget->SetKeyboardInteractivity(monitorId, interactive);
    });

    m_MousePositionRunning = true;
    m_MousePositionThread = std::thread([this]() {
        try {
            while (m_MousePositionRunning) {
                // TODO: Definitely use Hypr IPC for this instead of shelling out
                // to a command. This is just a placeholder for demonstration.
                const char* cmd = "hyprctl cursorpos";
                FILE* pipe = popen(cmd, "r");
                if (!pipe) {
                    WSS_ERROR("Failed to run command '{}'", cmd);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                char buffer[128];
                std::string result;
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    result += buffer;
                }

                pclose(pipe);
                if (result.empty()) {
                    WSS_ERROR("Command '{}' returned empty result", cmd);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                // Parse the result to extract mouse position with format X, Y
                size_t commaPos = result.find(',');
                if (commaPos == std::string::npos) {
                    WSS_ERROR("Invalid mouse position format: '{}'", result);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                int mousePosX = std::stoi(result.substr(0, commaPos));
                int mousePosY = std::stoi(result.substr(commaPos + 1));

                json_object* payload = json_object_new_object();
                json_object_object_add(payload, "x", json_object_new_int(mousePosX));
                json_object_object_add(payload, "y", json_object_new_int(mousePosY));
                Broadcast("mouse-position-update", payload);
                json_object_put(payload);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } catch (const std::exception& e) {
            WSS_ERROR("Unhandled exception in mouse position thread: {}", e.what());
        } catch (...) {
            WSS_ERROR("Unknown exception occurred in mouse position thread.");
        }
    });

    m_Running = true;
    m_Thread = std::thread([this]() {
        const int port = m_Shell->GetSettings().m_IpcPort;
        try {
            m_App = new uWS::App();
            m_App
                ->ws<IPCClientInfo>("/*", {
                                              .compression = uWS::DEDICATED_COMPRESSOR_256KB,
                                              .maxPayloadLength = 16 * 1024 * 1024, // 16 MB
                                              .idleTimeout = 60,
                                              .open =
                                                  [this](WSClient* ws) {
                                                      ws->subscribe("mouse-position-update");
                                                      WSS_DEBUG("New IPC client connected.");
                                                  },
                                              .message = [this](WSClient* ws, const std::string_view message,
                                                                const uWS::OpCode opCode) { IPCCallback(ws, message, opCode); },
                                          })
                .listen(port,
                        [=, this](const auto* token) {
                            if (token) {
                                WSS_INFO("IPC service started on port {}.", port);
                            } else {
                                WSS_ERROR("Failed to start IPC service on port {}. Is it already in use?", port);
                            }
                        })
                .run();

            WSS_DEBUG("IPC service loop exited, cleaning up resources.");
        } catch (const std::exception& e) {
            WSS_ERROR("Unhandled exception in IPC thread: {}", e.what());
        } catch (...) {
            WSS_ERROR("Unknown exception occurred in IPC thread.");
        }
    });
}

void WSS::IPC::Broadcast(const std::string& type, json_object* payload) {
    json_object* message = json_object_new_object();
    json_object_object_add(message, "type", json_object_new_string(type.c_str()));
    json_object_object_add(message, "payload", json_object_get(payload));
    const std::string jsonStr = json_object_to_json_string(message);

    bool status = m_App->publish(type, jsonStr, uWS::TEXT, true);
    if (!status) {
        WSS_WARN("Failed to broadcast message of type '{}'", type);
    }
    json_object_put(message);
}

void WSS::IPC::Send(WSClient* wsi, const std::string& type, json_object* payload) {
    if (!wsi || !payload)
        return;

    json_object* message = json_object_new_object();
    json_object_object_add(message, "type", json_object_new_string(type.c_str()));
    json_object_object_add(message, "payload", json_object_get(payload));
    const std::string jsonStr = json_object_to_json_string(message);
    wsi->send(jsonStr, uWS::TEXT, true);
    json_object_put(message);
}