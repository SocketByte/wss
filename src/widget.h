#ifndef WIDGET_H
#define WIDGET_H

#include <pch.h>

#include <utility>

namespace WSS {
class Shell;
}
namespace WSS {

typedef QWidget Window;
typedef QWebEngineView WebView;

/**
 * Defines the anchor points for a widget.
 */
enum class WidgetAnchor : uint8_t {
    TOP = 0x01,
    BOTTOM = 0x02,
    LEFT = 0x04,
    RIGHT = 0x08,
};

/**
 * Defines the layer in which a widget will be displayed.
 * The layer determines the stacking order of the widget in relation to other widgets.
 */
enum class WidgetLayer : uint8_t {
    TOP = LayerShellQt::Window::LayerTop,
    BOTTOM = LayerShellQt::Window::LayerBottom,
    OVERLAY = LayerShellQt::Window::LayerOverlay,
    BACKGROUND = LayerShellQt::Window::LayerBackground,
};

/**
 * Represents the clickable region information for a widget.
 * Useful for making widgets that are larger than the clickable area to make space for popovers etc.
 */
typedef struct {
    int X = 0;
    int Y = 0;
    int Width = 0;
    int Height = 0;
    int _QT_padding = 0; // Padding for Qt compatibility, not used in GTK
} WidgetClickRegionInfo;

/**
 * Represents the monitor information for a widget.
 * This structure contains the monitor ID and the dimensions of the widget on that monitor.
 * It also includes margins to adjust the position of the widget on the screen.
 */
typedef struct {
    uint8_t MonitorId;
    int Width;
    int Height;
    int MarginTop;
    int MarginBottom;
    int MarginLeft;
    int MarginRight;
    std::unordered_map<std::string, WidgetClickRegionInfo> ClickRegionMap;
} WidgetMonitorInfo;

/**
 * Represents the information required to create a widget in WSS.
 */
typedef struct {
    std::string Name;
    std::string Route;
    std::vector<WidgetMonitorInfo> Monitors;
    WidgetLayer Layer;
    uint8_t AnchorBitmask;
    int ExclusivityZone;
    bool Exclusivity;
    bool DefaultHidden;
    int _QT_padding = 0; // Padding for Qt compatibility, not used in GTK
} WidgetInfo;

/**
 * Represents a widget in the WSS.
 * A widget can be anchored to specific sides of the screen and can have multiple windows and web
 * views associated with it.
 */
class Widget {
    std::unordered_map<uint8_t, Window*> m_Windows;
    std::unordered_map<uint8_t, WebView*> m_Views;
    WidgetInfo m_Info;

   public:
    explicit Widget(WidgetInfo info) : m_Info(std::move(info)) { WSS_DEBUG("Creating widget: {}", m_Info.Name); }

    ~Widget() noexcept {
        try {
            for (auto& [monitorId, window] : m_Windows) {
                if (window) {
                    window->deleteLater();
                }
            }

            m_Windows.clear();
            m_Views.clear();
            WSS_DEBUG("Destroying widget: {}", m_Info.Name);
        } catch (const std::exception& e) {
            WSS_ERROR("Exception in Widget destructor: {}", e.what());
        }
    }

    void Create(Shell& shell);

    [[nodiscard]] const WidgetInfo& GetInfo() const { return m_Info; }

    [[nodiscard]] bool IsAnchoredTo(WidgetAnchor anchor) const {
        return (m_Info.AnchorBitmask & static_cast<uint8_t>(anchor)) != 0;
    }

    [[nodiscard]] Window* GetWindow(const uint8_t monitorId) const {
        if (const auto it = m_Windows.find(monitorId); it != m_Windows.end()) {
            return it->second;
        }
        return nullptr;
    }

    [[nodiscard]] WebView* GetWebView(const uint8_t monitorId) const {
        if (const auto it = m_Views.find(monitorId); it != m_Views.end()) {
            return it->second;
        }
        return nullptr;
    }

    [[nodiscard]] bool IsValid() const { return !m_Windows.empty() || !m_Views.empty(); }

    /**
     * Gets the monitor information for the specified monitor ID.
     * @param monitorId The ID of the monitor to get information for.
     * @return The monitor information for the specified monitor ID.
     */
    [[nodiscard]] const WidgetMonitorInfo& GetMonitorInfo(const uint8_t monitorId) const {
        if (const auto it = std::ranges::find_if(
                m_Info.Monitors, [monitorId](const WidgetMonitorInfo& info) { return info.MonitorId == monitorId; });
            it != m_Info.Monitors.end()) {
            return *it;
        }
        WSS_ERROR("Monitor ID '{}' does not exist in widget '{}'.", monitorId, m_Info.Name);
        throw std::out_of_range("Monitor ID does not exist");
    }

    /**
     * Gets the clickable region for the specified monitor ID.
     * @param monitorId The ID of the monitor to get the clickable region for.
     * @param regionName The name of the clickable region to retrieve.
     * @return The clickable region information for the specified monitor ID.
     */
    [[nodiscard]] WidgetClickRegionInfo GetClickableRegion(const uint8_t monitorId, const std::string& regionName) {
        const auto& monitorInfo = GetMonitorInfo(monitorId);
        if (const auto it = monitorInfo.ClickRegionMap.find(regionName); it != monitorInfo.ClickRegionMap.end()) {
            return it->second;
        }
        WSS_ERROR("Clickable region '{}' does not exist for monitor ID '{}' in widget '{}'.", regionName, monitorId, m_Info.Name);
        throw std::out_of_range("Clickable region does not exist");
    }

    /**
     * Sets the clickable region for the specified monitor ID.
     * If the region does not exist, it will be created.
     * @param monitorId The ID of the monitor to update the clickable region for.
     * @param regionName The name of the clickable region to update.
     * @param regionInfo The new clickable region information.
     */
    void SetClickableRegion(const uint8_t monitorId, const std::string& regionName,
                            const WidgetClickRegionInfo& regionInfo) const {
        auto& monitorInfo = const_cast<WidgetMonitorInfo&>(GetMonitorInfo(monitorId));
        monitorInfo.ClickRegionMap[regionName] = regionInfo;

        if (auto* window = GetWindow(monitorId); window) {
            QRegion inputRegion(0, 0, 1, 1);
            for (const auto& [name, info] : monitorInfo.ClickRegionMap) {
                if (info.X == 0 && info.Y == 0 && info.Width == 0 && info.Height == 0) {
                    continue;
                }

                const int padding = info._QT_padding > 0 ? info._QT_padding : 0;
                const QRect rect(info.X - padding, info.Y - padding, info.Width + 2 * padding, info.Height + 2 * padding);
                inputRegion += rect;
            }

            window->setMask(inputRegion);
            window->update();
        } else {
            WSS_ERROR("Attempted to update clickable region for an invalid or non-existent window on monitor ID: {}", monitorId);
        }
    }

    /**
     * Reloads the web view for the specified monitor ID.
     * @param monitorId The ID of the monitor to reload the web view for.
     */
    void Reload(const uint8_t monitorId) const {
        if (auto* view = GetWebView(monitorId); view) {
            view->reload();
            return;
        }
        WSS_WARN("Attempted to reload an invalid or non-existent web view on monitor ID: {}", monitorId);
    }

    /**
     * Reloads all web views associated with this widget.
     */
    void ReloadAll() const {
        for (const auto& [monitorId, view] : m_Views) {
            view->reload();
        }
    }

    /**
     * Sets the visibility of the window for the specified monitor ID.
     * If the window is hidden and exclusivity is enabled, it will also set the exclusivity for that
     * window.
     * @param monitorId The ID of the monitor to set visibility for.
     * @param visible Whether the window should be visible or not.
     */
    void SetVisible(const uint8_t monitorId, const bool visible) const {
        if (auto* window = GetWindow(monitorId); window) {
            window->setVisible(visible);

            // If the window is hidden then there's no need for exclusivity.
            // TODO: Determine if that's ever something a user would wish to omit.
            if (m_Info.Exclusivity) {
                SetExclusivity(monitorId, visible);
                if (m_Info.ExclusivityZone) {
                    SetExclusivity(monitorId, visible, m_Info.ExclusivityZone);
                }
            }
            return;
        }
        WSS_WARN("Attempted to set visibility for an invalid or non-existent window on monitor ID: {}", monitorId);
    }

    /**
     * Sets the visibility of all windows associated with this widget.
     * @see SetVisible
     * @param visible Whether all windows should be visible or not.
     */
    void SetVisibleAll(const bool visible) const {
        for (const auto& [monitorId, window] : m_Windows) {
            window->setVisible(visible);

            if (m_Info.Exclusivity) {
                SetExclusivity(monitorId, visible);
                if (m_Info.ExclusivityZone) {
                    SetExclusivity(monitorId, visible, m_Info.ExclusivityZone);
                }
            }
        }
    }

    void SetKeyboardInteractivity(const uint8_t monitorId, const bool interactive) const {
        if (auto* window = GetWindow(monitorId); window) {
            auto* layer = LayerShellQt::Window::get(window->windowHandle());
            if (layer) {
                layer->setKeyboardInteractivity(interactive ? LayerShellQt::Window::KeyboardInteractivityOnDemand
                                                            : LayerShellQt::Window::KeyboardInteractivityNone);
                window->update();
            } else {
                WSS_WARN("LayerShellQt::Window not found for monitor ID: {}", monitorId);
            }
        }
    }

    /**
     * Sets the exclusivity for the window on the specified monitor ID.
     * If the window is set to exclusive mode, it will prevent other windows from overlapping with
     * it.
     * @param monitorId The ID of the monitor to set exclusivity for.
     * @param exclusive Whether the window should be set to exclusive mode or not.
     */
    void SetExclusivity(const uint8_t monitorId, const bool exclusive, int zone = 0) const {
        if (auto* window = GetWindow(monitorId); window) {
            auto* layer = LayerShellQt::Window::get(window->windowHandle());
            if (exclusive) {
                layer->setExclusiveZone(zone);
            } else {
                layer->setExclusiveZone(0);
            }
            return;
        }
        WSS_WARN("Attempted to set exclusivity for an invalid or non-existent window on monitor ID: {}", monitorId);
    }
};
} // namespace WSS

#endif // WIDGET_H
