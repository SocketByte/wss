#include "ipc.h"

#include <WebSocket.h>

#include <queue>

#include "shell.h"

void WSS::IPC::IPCCallback(WSClient* ws, std::string_view message, uWS::OpCode opCode) {
    json jobj;
    try {
        jobj = json::parse(message);
    } catch (const std::exception& e) {
        WSS_ERROR("Failed to parse JSON: {}", e.what());
        ws->close();
        return;
    }

    if (!jobj.contains("type") || !jobj.contains("payload")) {
        WSS_ERROR("Received JSON does not contain 'type' or 'payload' fields.");
        ws->close();
        return;
    }

    const std::string type = jobj["type"];
    const json& payload = jobj["payload"];

    if (type == "handshake") {
        int monitorId = payload["monitorId"];
        std::string widgetName = payload["widgetName"];

        ws->getUserData()->monitorId = monitorId;
        ws->getUserData()->widgetName = widgetName;
        WSS_DEBUG("Client identified with monitor ID: {}, widget name: {}", monitorId, widgetName);
        return;
    }

    if (!ws->getUserData()) {
        WSS_WARN("No client info found for WebSocket connection.");
        return;
    }

    m_Shell->GetIPC().Notify(type, m_Shell, ws, payload);
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

    Listen("window-update-click-region", [this](Shell* shell, WSClient* client, const json& payload) {
        int monitorId = client->getUserData()->monitorId;
        std::string widgetName = client->getUserData()->widgetName;

        auto widget = shell->GetWidget(widgetName);
        if (!widget) {
            WSS_ERROR("Widget '{}' not found for monitor ID: {}", widgetName, monitorId);
            return;
        }

        WidgetClickRegionInfo regionInfo{
            .X = payload["x"],
            .Y = payload["y"],
            .Width = payload["width"],
            .Height = payload["height"],
            ._QT_padding = widget->GetInfo()._QT_padding,
        };

        const std::string regionName = payload["name"];
        widget->SetClickableRegion(monitorId, regionName, regionInfo);
    });

    Listen("notifd-notification-dismiss", [this](Shell* shell, WSClient* client, const json& payload) {
        uint32_t id = payload["id"];
        shell->GetNotifd().SignalNotificationClosed(id, NotificationCloseReason::DISMISSED);
    });

    Listen("notifd-notification-action", [this](Shell* shell, WSClient* client, const json& payload) {
        uint32_t id = payload["id"];
        std::string action = payload["action"];
        shell->GetNotifd().SignalActionInvoked(id, action);
    });

    Listen("appd-application-run", [this](Shell* shell, WSClient* client, const json& payload) {
        std::string prefix = payload["prefix"];
        std::string appId = payload["appId"];
        WSS_DEBUG("Running application with ID: {} using prefix: {}", appId, prefix);
        shell->GetAppd().RunApplication(prefix, appId);
    });

    Listen("appd-application-list-request", [this](Shell* shell, WSClient* client, const json& payload) {
        json response = json::array();
        for (const auto& [name, app] : shell->GetAppd().GetApplications()) {
            response.push_back({
                {"id", app.Id},
                {"name", app.Name},
                {"comment", app.Comment},
                {"exec", app.Exec},
                {"iconBase64Large", app.IconBase64Large},
                {"iconBase64Small", app.IconBase64Small},
            });
        }
        Send(client, "appd-application-list-response", response);
    });

    Listen("monitor-info-request", [this](Shell* shell, WSClient* client, const json& payload) {
        auto monitor = shell->GetWidget(client->getUserData()->widgetName)->GetMonitorInfo(client->getUserData()->monitorId);
        int id = client->getUserData()->monitorId;

        json response = {
            {"id", id},
            {"width", GetScreenWidth(id)},
            {"height", GetScreenHeight(id)},
        };

        Send(client, "monitor-info-response", response);
    });

    Listen("widget-set-keyboard-interactivity", [this](Shell* shell, WSClient* client, const json& payload) {
        std::string widgetName = client->getUserData()->widgetName;
        int monitorId = client->getUserData()->monitorId;
        auto widget = shell->GetWidget(widgetName);
        if (!widget) {
            WSS_ERROR("Widget '{}' not found for setting keyboard interactivity.", widgetName);
            return;
        }

        bool interactive = payload["interactive"];
        widget->SetKeyboardInteractivity(monitorId, interactive);
    });

    m_MousePositionRunning = true;
    m_MousePositionThread = std::thread([this]() {
        try {
            while (m_MousePositionRunning) {
                const char* cmd = "hyprctl cursorpos";
                FILE* pipe = popen(cmd, "r");
                if (!pipe) {
                    WSS_ERROR("Failed to run command '{}'", cmd);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                char buffer[128];
                std::string result;
                while (fgets(buffer, sizeof(buffer), pipe)) {
                    result += buffer;
                }

                pclose(pipe);
                if (result.empty()) {
                    WSS_ERROR("Command '{}' returned empty result", cmd);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                size_t commaPos = result.find(',');
                if (commaPos == std::string::npos) {
                    WSS_ERROR("Invalid mouse position format: '{}'", result);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                int mousePosX = std::stoi(result.substr(0, commaPos));
                int mousePosY = std::stoi(result.substr(commaPos + 1));

                json payload = {{"x", mousePosX}, {"y", mousePosY}};

                Broadcast("mouse-position-update", payload);
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
                ->ws<IPCClientInfo>("/*", {.compression = uWS::DEDICATED_COMPRESSOR_256KB,
                                           .maxPayloadLength = 16 * 1024 * 1024,
                                           .idleTimeout = 60,
                                           .open =
                                               [this](WSClient* ws) {
                                                   ws->subscribe("monitor-info-response");
                                                   ws->subscribe("appd-application-list-response");
                                                   ws->subscribe("appd-application-added");
                                                   ws->subscribe("mouse-position-update");
                                               },
                                           .message = [this](WSClient* ws, const std::string_view message,
                                                             const uWS::OpCode opCode) { IPCCallback(ws, message, opCode); }})
                .listen(port,
                        [=, this](auto* token) {
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

void WSS::IPC::Broadcast(const std::string& type, const json& payload) {
    json message = {{"type", type}, {"payload", payload}};

    std::string jsonStr = message.dump();

    m_App->publish(type, jsonStr, uWS::TEXT, true);
}

void WSS::IPC::Send(WSClient* wsi, const std::string& type, const json& payload) {
    if (!wsi) return;

    json message = {{"type", type}, {"payload", payload}};

    wsi->send(message.dump(), uWS::TEXT, true);
}