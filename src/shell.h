#ifndef SHELL_H
#define SHELL_H

#include "ipc.h"
#include "modules/appd.h"

#include <modules/notifd.h>
#include <pch.h>
#include <widget.h>

typedef struct {
    WSS::Shell* shell;
    std::string configPath;
} ActivateCallbackData;

#ifndef WSS_USE_QT
typedef GtkApplication RenderApplication;
typedef gpointer ActivateCallbackPtr;
#else
typedef QApplication RenderApplication;
typedef ActivateCallbackData* ActivateCallbackPtr;
#endif

namespace WSS {
#ifndef WSS_USE_QT
static double GetScreenWidth(int monitorId) {
    GdkDisplay* display = gdk_display_get_default();
    GListModel* monitorList = gdk_display_get_monitors(display);
    const auto monitor = GDK_MONITOR(g_list_model_get_item(monitorList, monitorId));
    if (!monitor) {
        WSS_ERROR("Monitor ID '{}' does not exist.", monitorId);
        return -1;
    }

    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    return geometry.width;
}

static double GetScreenHeight(int monitorId) {
    GdkDisplay* display = gdk_display_get_default();
    GListModel* monitorList = gdk_display_get_monitors(display);
    const auto monitor = GDK_MONITOR(g_list_model_get_item(monitorList, monitorId));
    if (!monitor) {
        WSS_ERROR("Monitor ID '{}' does not exist.", monitorId);
        return -1;
    }

    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    return geometry.height;
}

#else
static double GetScreenWidth(int monitorId) {
    QList<QScreen*> screens = QGuiApplication::screens();

    if (monitorId < 0 || monitorId >= screens.size()) {
        qWarning("Monitor ID '%d' does not exist.", monitorId);
        return -1;
    }

    QScreen* screen = screens.at(monitorId);
    return screen->geometry().width();
}

static double GetScreenHeight(int monitorId) {
    QList<QScreen*> screens = QGuiApplication::screens();

    if (monitorId < 0 || monitorId >= screens.size()) {
        qWarning("Monitor ID '%d' does not exist.", monitorId);
        return -1;
    }

    QScreen* screen = screens.at(monitorId);
    return screen->geometry().height();
}
#endif

static std::atomic_bool IsRunning{true};

class ShellSettings {
  public:
    int m_FrontendPort;
    int m_IpcPort;
    int m_NotificationTimeout;
};

/**
 * Represents the main application shell for WSS.
 * This class is responsible for initializing and managing the entire GTK application.
 */
class Shell {
    RenderApplication* m_Application = nullptr;
    IPC m_IPC{this};
    Notifd m_Notifd{this};
    Appd m_Appd{this};
    ShellSettings m_Settings;

    std::unordered_map<std::string, std::shared_ptr<Widget>> m_Widgets;

    static void OnActivate(RenderApplication* app, ActivateCallbackPtr data);

    void LoadConfig(const std::string& configPath);

  public:
    Shell() = default;
    ~Shell() = default;
    Shell(const Shell&) = delete;
    Shell(Shell&&) = delete;
    Shell& operator=(Shell&&) = delete;

#ifndef WSS_USE_QT
    int Init(const std::string& appId, GApplicationFlags flags, const std::string& configPath);
#else
    int Init(const std::string& appId, const std::string& configPath);
#endif

    [[nodiscard]] IPC& GetIPC() { return m_IPC; }
    [[nodiscard]] Notifd& GetNotifd() { return m_Notifd; }
    [[nodiscard]] Appd& GetAppd() { return m_Appd; }

    [[nodiscard]] std::shared_ptr<Widget> GetWidget(const std::string& name) const {
        if (const auto it = m_Widgets.find(name); it != m_Widgets.end()) {
            return it->second;
        }
        return nullptr;
    }

    [[nodiscard]] bool IsValid() const { return m_Application != nullptr; }

    [[nodiscard]] RenderApplication* GetApplication() const { return m_Application; }

    [[nodiscard]] ShellSettings& GetSettings() { return m_Settings; }
};
} // namespace WSS

#endif // SHELL_H
