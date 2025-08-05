#include "notifd.h"

#include "shell.h"
#include "util/dbus_vreader.h"

void WSS::Notifd::StartExpirationTimer(uint32_t id, int32_t timeoutMs) {
    std::thread([this, id, timeoutMs]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        {
            std::lock_guard lock(m_NotificationsMutex);
            if (m_Notifications.find(id) != m_Notifications.end()) {
                WSS_DEBUG("Notification ID={} expired after {} ms", id, timeoutMs);
            } else {
                return; // Already removed or closed
            }
        }
        SignalNotificationClosed(id, NotificationCloseReason::EXPIRED);
    }).detach();
}

json WSS::Notifd::CreateNotificationPayload(const Notification& notification) const {
    json payload;

    payload["id"] = static_cast<int32_t>(notification.Id);
    payload["appName"] = notification.AppName;
    payload["appIcon"] = notification.AppIcon;
    payload["summary"] = notification.Summary;
    payload["body"] = notification.Body;

    // Convert actions
    payload["actions"] = json::array();
    for (const auto& action : notification.Actions) {
        payload["actions"].push_back(action);
    }

    // Convert hints
    json hints;
    for (const auto& [key, value] : notification.Hints) {
        auto valueStr = ReadDbusVariant(value);
        hints[key] = valueStr;
    }
    payload["hints"] = hints;

    payload["expireTimeout"] = notification.ExpireTimeout;

    return payload;
}

void WSS::Notifd::Start() {
    m_Thread = std::thread([this]() {
        try {
            WSS_DEBUG("Starting Notifd...");
            sdbus::ServiceName serviceName{"org.freedesktop.Notifications"};
            m_Connection = sdbus::createSessionBusConnection(serviceName);

            sdbus::ObjectPath objectPath{"/org/freedesktop/Notifications"};
            m_NotificationObject = sdbus::createObject(*m_Connection, std::move(objectPath));

            auto notify = [&](const std::string& app_name, const uint32_t replaces_id, const std::string& app_icon,
                              const std::string& summary, const std::string& body, const std::vector<std::string>& actions,
                              const std::map<std::string, sdbus::Variant>& hints, const int32_t expire_timeout) -> uint32_t {
                const uint32_t id =
                    (replaces_id == 0) ? m_NotificationCounter.fetch_add(1, std::memory_order_relaxed) : replaces_id;

                WSS_DEBUG("Received notification: ID={}, AppName={}, Summary={}, Body={}, Actions={}, Hints={}, ExpireTimeout={}",
                          id, app_name, summary, body, actions.size(), hints.size(), expire_timeout);

                {
                    const Notification notification{.Id = id,
                                                    .AppName = app_name,
                                                    .AppIcon = app_icon,
                                                    .Summary = summary,
                                                    .Body = body,
                                                    .Actions = actions,
                                                    .Hints = hints,
                                                    .ExpireTimeout = m_Shell->GetSettings().m_NotificationTimeout};
                    AddNotification(notification);
                }

                return id;
            };
            auto closeNotification = [&](const uint32_t id) {
                SignalNotificationClosed(id, NotificationCloseReason::CLOSED_BY_CLIENT);
            };
            auto getCapabilities = []() -> std::vector<std::string> {
                return {"body", "actions", "icon-static", "icon-multi", "persistence", "sound"};
            };

            auto getServerInformation = []() -> std::tuple<std::string, std::string, std::string, std::string> {
                return {"wss-notifd", "WebShellSystem", "1.0", "1.3"};
            };

            m_NotificationObject
                ->addVTable(sdbus::registerMethod("Notify").implementedAs(notify),
                            sdbus::registerMethod("CloseNotification").implementedAs(closeNotification),
                            sdbus::registerMethod("GetCapabilities").implementedAs(getCapabilities),
                            sdbus::registerMethod("GetServerInformation").implementedAs(getServerInformation))
                .forInterface("org.freedesktop.Notifications");

            m_Connection->enterEventLoop();
            WSS_DEBUG("Notifd started successfully.");
        } catch (const std::exception& e) {
            WSS_ERROR("Failed to start Notifd: {}", e.what());
        }
    });
}

void WSS::Notifd::AddNotification(const Notification& notification) {
    std::lock_guard lock(m_NotificationsMutex);

    if (m_Notifications.find(notification.Id) != m_Notifications.end()) {
        WSS_WARN("Notification with ID {} already exists. Replacing it.", notification.Id);
    }

    m_Notifications[notification.Id] = notification;

    nlohmann::json payload = CreateNotificationPayload(notification);
    m_Shell->GetIPC().Broadcast("notifd-notification", payload);

    if (notification.ExpireTimeout > 0) {
        StartExpirationTimer(notification.Id, notification.ExpireTimeout);
    }
}

void WSS::Notifd::SignalNotificationClosed(uint32_t id, NotificationCloseReason reason) {
    std::lock_guard lock(m_NotificationsMutex);
    auto it = m_Notifications.find(id);

    if (it != m_Notifications.end()) {
        nlohmann::json payload = {{"id", static_cast<int32_t>(it->second.Id)}, {"reason", static_cast<int32_t>(reason)}};

        m_Shell->GetIPC().Broadcast("notifd-notification-closed", payload);
        m_Notifications.erase(it);

        m_NotificationObject->emitSignal(sdbus::SignalName("CloseNotification"))
            .onInterface("org.freedesktop.Notifications")
            .withArguments(id, static_cast<uint32_t>(reason));
    } else {
        WSS_WARN("Notification with ID {} not found.", id);
    }
}

void WSS::Notifd::SignalActionInvoked(uint32_t id, const std::string& action) {
    std::lock_guard lock(m_NotificationsMutex);
    auto it = m_Notifications.find(id);
    if (it != m_Notifications.end()) {
        WSS_DEBUG("Invoking action '{}' on notification: ID={}, Summary={}", action, it->second.Id, it->second.Summary);
        m_NotificationObject->emitSignal(sdbus::SignalName("ActionInvoked"))
            .onInterface("org.freedesktop.Notifications")
            .withArguments(it->second.Id, action);
    } else {
        WSS_WARN("Notification with ID {} not found for action '{}'.", id, action);
    }
}