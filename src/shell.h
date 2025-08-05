#ifndef SHELL_H
#define SHELL_H

#include <modules/notifd.h>
#include <pch.h>
#include <widget.h>

#include "ipc.h"
#include "modules/appd.h"

typedef struct {
    WSS::Shell* shell;
    std::string configPath;
} ActivateCallbackData;

typedef QApplication RenderApplication;
typedef ActivateCallbackData* ActivateCallbackPtr;

namespace WSS {

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

    int Init(const std::string& appId, const std::string& configPath);

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
