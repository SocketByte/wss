#ifndef SHELL_H
#define SHELL_H

#include "ipc.h"

#include <pch.h>
#include <widget.h>

namespace WSS {
static std::atomic_bool IsRunning{true};
/**
 * Represents the main application shell for WSS.
 * This class is responsible for initializing and managing the entire GTK application.
 */
class Shell {
    GtkApplication* m_Application = nullptr;
    IPC m_IPC{this};
    std::unordered_map<std::string, std::shared_ptr<Widget>> m_Widgets;

    static void GtkOnActivate(GtkApplication* app, gpointer data);

  public:
    Shell() = default;
    ~Shell() = default;
    Shell(const Shell&) = delete;
    Shell(Shell&&) = delete;
    Shell& operator=(Shell&&) = delete;

    void Init(const std::string& appId, GApplicationFlags flags, const std::string& configPath);

    [[nodiscard]] bool IsValid() const { return m_Application != nullptr; }

    [[nodiscard]] GtkApplication* GetApplication() const { return m_Application; }
};
} // namespace WSS

#endif // SHELL_H
