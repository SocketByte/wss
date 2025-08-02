#ifndef NOTIFD_H
#define NOTIFD_H

#include <pch.h>

namespace WSS {
class Shell;
}
namespace WSS {
enum class NotificationCloseReason : uint32_t { EXPIRED = 1, DISMISSED = 2, CLOSED_BY_CLIENT = 3, RESERVED = 4 };

/**
 * Represents a notification structure used by the Notifd daemon.
 * This structure contains all the necessary information to display a notification,
 */
struct Notification {
    uint32_t Id = 0;
    std::string AppName;
    std::string AppIcon;
    std::string Summary;
    std::string Body;
    std::vector<std::string> Actions;
    std::map<std::string, sdbus::Variant> Hints;
    int32_t ExpireTimeout = -1; // Default to -1 for no expiration
};

/**
 * Notifd is a simple notification daemon that implements the
 * org.freedesktop.Notifications D-Bus interface.
 */
class Notifd {
    Shell* m_Shell = nullptr;
    std::unique_ptr<sdbus::IConnection> m_Connection;
    std::unique_ptr<sdbus::IObject> m_NotificationObject;
    std::unordered_map<uint32_t, Notification> m_Notifications;
    mutable std::mutex m_NotificationsMutex;
    std::thread m_Thread;

    void StartExpirationTimer(uint32_t id, int32_t timeoutMs);
    json_object* CreateNotificationPayload(const Notification& notification) const;

  public:
    explicit Notifd(Shell* shell) : m_Shell(shell) {
        WSS_ASSERT(m_Shell != nullptr, "Shell instance must not be null.");
        WSS_DEBUG("Notifd initialized with Shell instance.");
    }

    ~Notifd() {
        m_Connection->leaveEventLoop();
        if (m_Thread.joinable()) {
            m_Thread.join();
        }
        WSS_DEBUG("Notifd destroyed.");
    }

    Notifd(const Notifd&) = delete;
    Notifd(Notifd&&) = delete;
    Notifd& operator=(Notifd&&) = delete;

    [[nodiscard]] const std::unordered_map<uint32_t, Notification>& GetNotifications() const {
        std::lock_guard lock(m_NotificationsMutex);
        return m_Notifications;
    }

    /**
     * Adds a notification to the Notifd daemon.
     * If a notification with the same ID already exists, it will be replaced.
     * This method is thread-safe and will lock the notifications mutex while adding the notification.
     * It also starts an expiration timer if the notification has a positive timeout.
     * @param notification The notification to add.
     */
    void AddNotification(const Notification& notification);
    /**
     * Signals that a notification has been closed by the user.
     * @param id The ID of the notification to remove.
     * @param reason The reason for closing the notification. Defaults to DISMISSED.
     */
    void SignalNotificationClosed(uint32_t id, NotificationCloseReason reason = NotificationCloseReason::DISMISSED);

    /**
     * Signals that an action has been invoked on a notification.
     * @param id The ID of the notification to invoke an action for.
     * @param action The action to invoke on the notification. Must be one of the actions defined in the notification.
     */
    void SignalActionInvoked(uint32_t id, const std::string& action);

    void Start();
};
} // namespace WSS

#endif // NOTIFD_H
