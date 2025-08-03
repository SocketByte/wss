#include "widget.h"

#include "shell.h"

#ifndef WSS_USE_QT
#include <EGL/egl.h>
#include <gdk/wayland/gdkwayland.h>
#include <libsoup/soup.h>
#include <wayland-client.h>

static gboolean WebViewContextMenuCallback(WebKitWebView* web_view, WebKitContextMenu* context_menu, GdkEvent* event,
                                           WebKitHitTestResult* hit_test_result, gpointer user_data) {
    // Returning TRUE disables the context menu
    return TRUE;
}

void WSS::Widget::Create(Shell& shell) {
    const size_t monitors = m_Info.Monitors.size();
    for (int i = 0; i < monitors; i++) {
        std::string name = "wss.widget." + m_Info.Name + "." + std::to_string(m_Info.Monitors[i].MonitorId);
        const auto gtkWindow = GTK_WINDOW(gtk_application_window_new(shell.GetApplication()));
        gtk_window_set_title(gtkWindow, name.c_str());

        auto monitorInfo = m_Info.Monitors[i];

        gtk_window_set_default_size(gtkWindow, monitorInfo.Width, monitorInfo.Height);
        gtk_window_set_resizable(gtkWindow, FALSE);
        gtk_window_set_decorated(gtkWindow, FALSE);

        GtkCssProvider* cssProvider = gtk_css_provider_new();
        gtk_css_provider_load_from_string(cssProvider, "* { background-color: transparent; }");
        gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(cssProvider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(cssProvider);

        gtk_layer_init_for_window(gtkWindow);
        gtk_layer_set_namespace(gtkWindow, "wss.shell");
        gtk_layer_set_layer(gtkWindow, static_cast<GtkLayerShellLayer>(m_Info.Layer));

        if (m_Info.Exclusivity) {
            if (m_Info.ExclusivityZone > 0) {
                gtk_layer_set_exclusive_zone(gtkWindow, m_Info.ExclusivityZone);
            } else {
                gtk_layer_auto_exclusive_zone_enable(gtkWindow);
            }
        }

        gtk_layer_set_anchor(gtkWindow, GTK_LAYER_SHELL_EDGE_TOP, IsAnchoredTo(WidgetAnchor::TOP));
        gtk_layer_set_anchor(gtkWindow, GTK_LAYER_SHELL_EDGE_BOTTOM, IsAnchoredTo(WidgetAnchor::BOTTOM));
        gtk_layer_set_anchor(gtkWindow, GTK_LAYER_SHELL_EDGE_LEFT, IsAnchoredTo(WidgetAnchor::LEFT));
        gtk_layer_set_anchor(gtkWindow, GTK_LAYER_SHELL_EDGE_RIGHT, IsAnchoredTo(WidgetAnchor::RIGHT));
        auto* display = gdk_display_get_default();
        if (const auto monitor = g_list_model_get_item(gdk_display_get_monitors(display), m_Info.Monitors[i].MonitorId)) {
            const auto gdkMonitor = GDK_MONITOR(monitor);
            gtk_layer_set_monitor(gtkWindow, gdkMonitor);
            gtk_layer_set_margin(gtkWindow, GTK_LAYER_SHELL_EDGE_TOP, monitorInfo.MarginTop);
            gtk_layer_set_margin(gtkWindow, GTK_LAYER_SHELL_EDGE_BOTTOM, monitorInfo.MarginBottom);
            gtk_layer_set_margin(gtkWindow, GTK_LAYER_SHELL_EDGE_LEFT, monitorInfo.MarginLeft);
            gtk_layer_set_margin(gtkWindow, GTK_LAYER_SHELL_EDGE_RIGHT, monitorInfo.MarginRight);
        }

        WebKitWebView* webview = WEBKIT_WEB_VIEW(webkit_web_view_new());

        std::string uri = std::format("http://localhost:{}", shell.GetSettings().m_FrontendPort);
        if (!m_Info.Route.empty()) {
            uri += m_Info.Route;
        }

        // Add metadata
        uri += "?widgetName=" + m_Info.Name + "&monitorId=" + std::to_string(monitorInfo.MonitorId);

        WSS_DEBUG("Loading URI: {}", uri);

        if (m_Info.Route != "_DEBUG") {
            webkit_web_view_load_uri(webview, uri.c_str());
        } else {
            webkit_web_view_load_uri(webview, "webkit://gpu");
        }

        g_signal_connect(webview, "context-menu", G_CALLBACK(WebViewContextMenuCallback), NULL);

        GdkRGBA rgba = {0.0f, 0.0f, 0.0f, 0.0f};
        webkit_web_view_set_background_color(webview, &rgba);
        gtk_widget_set_vexpand(GTK_WIDGET(webview), TRUE);
        gtk_widget_set_hexpand(GTK_WIDGET(webview), TRUE);

        WebKitSettings* settings = webkit_web_view_get_settings(webview);
        webkit_settings_set_enable_javascript(settings, TRUE);
        webkit_settings_set_enable_webgl(settings, TRUE);
        webkit_settings_set_hardware_acceleration_policy(settings, WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS);

        webkit_web_context_set_cache_model(webkit_web_view_get_context(webview), WEBKIT_CACHE_MODEL_WEB_BROWSER);

        gtk_window_set_child(gtkWindow, GTK_WIDGET(webview));
        gtk_window_present(gtkWindow);

        cairo_region_t* region = cairo_region_create();

        for (const auto& [regionName, regionInfo] : monitorInfo.ClickRegionMap) {
            GdkRectangle clickable_area;
            clickable_area.x = regionInfo.X;
            clickable_area.y = regionInfo.Y;
            clickable_area.width = regionInfo.Width;
            clickable_area.height = regionInfo.Height;

            cairo_region_union_rectangle(region, &clickable_area);
        }

        auto* surface = gtk_native_get_surface(GTK_NATIVE(GTK_WIDGET(gtkWindow)));
        gdk_surface_set_input_region(surface, region);

        cairo_region_destroy(region);

        if (m_Info.DefaultHidden) {
            gtk_widget_set_visible(GTK_WIDGET(gtkWindow), FALSE);
            if (m_Info.Exclusivity) {
                gtk_layer_set_exclusive_zone(gtkWindow, 0);
            }
        }

        m_Windows.emplace(monitorInfo.MonitorId, gtkWindow);
        m_Views.emplace(monitorInfo.MonitorId, webview);
        WSS_DEBUG("Created widget '{}' on monitor ID: {}", m_Info.Name, monitorInfo.MonitorId);
    }
}
#else
#include <QApplication>
#include <QDebug>
#include <QMainWindow>
#include <QScreen>
#include <QUrlQuery>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWindow>
#include <qevent.h>

class NoContextMenuWebEngineView : public QWebEngineView {
    Q_OBJECT
  public:
    explicit NoContextMenuWebEngineView(QWidget* parent = nullptr) : QWebEngineView(parent) {}

  protected:
    void contextMenuEvent(QContextMenuEvent* event) override { event->ignore(); }
};

void WSS::Widget::Create(Shell& shell) {
    const size_t monitors = m_Info.Monitors.size();

    for (int i = 0; i < monitors; ++i) {
        auto monitorInfo = m_Info.Monitors[i];

        QString name = QString("wss.widget.%1.%2").arg(QString::fromStdString(m_Info.Name)).arg(monitorInfo.MonitorId);

        auto* window = new QWidget;
        window->setWindowTitle(name);
        window->setObjectName(name);
        window->setWindowFlag(Qt::FramelessWindowHint);
        window->setWindowFlag(Qt::WindowStaysOnTopHint);
        window->setAttribute(Qt::WA_TranslucentBackground);
        window->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        window->setScreen(QGuiApplication::screens().at(monitorInfo.MonitorId));
        window->show();
        window->hide();

        window->clearFocus();
        auto* webview = new NoContextMenuWebEngineView(window);

        // Load the URL
        QString uri = QString("http://localhost:%1").arg(shell.GetSettings().m_FrontendPort);
        if (!m_Info.Route.empty()) {
            uri += QString::fromStdString(m_Info.Route);
        }

        QUrl url(uri);
        QUrlQuery query;
        query.addQueryItem("widgetName", QString::fromStdString(m_Info.Name));
        query.addQueryItem("monitorId", QString::number(monitorInfo.MonitorId));
        url.setQuery(query);

        WSS_DEBUG("Loading URI: {}", url.toString().toStdString());
        if (m_Info.Route != "_DEBUG") {
            webview->load(url);
        } else {
            webview->load(QUrl("chrome://gpu"));
        }

        // Web engine settings
        QWebEngineSettings* settings = webview->settings();
        settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
        settings->setAttribute(QWebEngineSettings::WebGLEnabled, true);
        settings->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, true);

        // Transparent background support
        if (m_Info.Route != "_DEBUG") {
            webview->setAttribute(Qt::WA_TranslucentBackground);
            webview->setStyleSheet("background: transparent");
            webview->page()->setBackgroundColor(Qt::transparent);
        }

        // webview->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        // Move window to monitor
        const auto screens = QGuiApplication::screens();
        if (monitorInfo.MonitorId < screens.size()) {
            const QRect geometry = screens[monitorInfo.MonitorId]->geometry();

            int x = geometry.x() + monitorInfo.MarginLeft;
            int y = geometry.y() + monitorInfo.MarginTop;
            window->move(x, y);
        }

        window->setLayout(new QVBoxLayout);
        window->layout()->setContentsMargins(0, 0, 0, 0);
        window->layout()->addWidget(webview);
        window->resize(monitorInfo.Width, monitorInfo.Height);
        window->setContentsMargins(0, 0, 0, 0);

        QWindow* lwin = window->windowHandle();
        if (LayerShellQt::Window* lsh = LayerShellQt::Window::get(lwin)) {
            lsh->setScope("wss.shell");
            lsh->setLayer(static_cast<LayerShellQt::Window::Layer>(m_Info.Layer));
            lsh->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);

            lsh->setMargins(QMargins(monitorInfo.MarginLeft, monitorInfo.MarginTop, monitorInfo.MarginRight, monitorInfo.MarginBottom));
            LayerShellQt::Window::Anchors anchors;
            anchors.setFlag(LayerShellQt::Window::AnchorTop, IsAnchoredTo(WidgetAnchor::TOP));
            anchors.setFlag(LayerShellQt::Window::AnchorBottom, IsAnchoredTo(WidgetAnchor::BOTTOM));
            anchors.setFlag(LayerShellQt::Window::AnchorLeft, IsAnchoredTo(WidgetAnchor::LEFT));
            anchors.setFlag(LayerShellQt::Window::AnchorRight, IsAnchoredTo(WidgetAnchor::RIGHT));
            lsh->setAnchors(anchors);
            lsh->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);

            if (m_Info.DefaultHidden) {
                window->hide();
            } else {
                window->show();
                if (m_Info.Exclusivity) {
                    lsh->setExclusiveZone(m_Info.ExclusivityZone);
                }
            }
        }

        QRegion inputRegion;
        if (monitorInfo.ClickRegionMap.empty()) {
            // Since Qt mask doesn't really work with empty regions, we create a 1x1 region as a workaround.
            inputRegion = QRegion(0, 0, 1, 1);
        } else {
            for (const auto& [regionName, regionInfo] : monitorInfo.ClickRegionMap) {
                if (regionInfo.X == 0 && regionInfo.Y == 0 && regionInfo.Width == 0 && regionInfo.Height == 0) {
                    continue;
                }
                const QRect rect(regionInfo.X, regionInfo.Y, regionInfo.Width, regionInfo.Height);
                inputRegion += rect;
            }
        }
        window->setMask(inputRegion);
        window->update();

        m_Windows.emplace(monitorInfo.MonitorId, window);
        m_Views.emplace(monitorInfo.MonitorId, webview);

        WSS_DEBUG("Created widget '{}' on monitor ID: {}", m_Info.Name, monitorInfo.MonitorId);
    }
}
#include "widget.moc"
#endif