#include "widget.h"

#include "shell.h"

static gboolean WebViewContextMenuCallback(WebKitWebView* web_view, WebKitContextMenu* context_menu, GdkEvent* event,
                                           WebKitHitTestResult* hit_test_result, gpointer user_data) {
    // Returning TRUE disables the context menu
    return TRUE;
}

static gboolean GtkWidgetRealizeSetRegionsCallback(GtkWidget* widget, gpointer data) {
    const auto* cbMonitorInfo = static_cast<WSS::WidgetMonitorInfo*>(data);
    auto& map = cbMonitorInfo->ClickRegionMap;

    cairo_region_t* region = cairo_region_create();

    for (const auto& [regionName, regionInfo] : map) {
        GdkRectangle clickable_area;
        clickable_area.x = regionInfo.X;
        clickable_area.y = regionInfo.Y;
        clickable_area.width = regionInfo.Width;
        clickable_area.height = regionInfo.Height;

        cairo_region_union_rectangle(region, &clickable_area);
    }

    auto* surface = gtk_native_get_surface(GTK_NATIVE(widget));
    gdk_surface_set_input_region(surface, region);

    cairo_region_destroy(region);
    WSS_DEBUG("Updated clickable regions for widget '{}' on monitor ID: {}", cbMonitorInfo->MonitorId, cbMonitorInfo->MonitorId);
    return TRUE;
}

void WSS::Widget::Create(const Shell& shell) {
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

        const char* uri = "http://localhost:3000";
        if (!m_Info.Route.empty()) {
            char* fullUri = g_strdup_printf("%s/%s", uri, m_Info.Route);
            webkit_web_view_load_uri(webview, fullUri);
            g_free(fullUri);
        } else {
            webkit_web_view_load_uri(webview, uri);
        }

        g_signal_connect(webview, "context-menu", G_CALLBACK(WebViewContextMenuCallback), NULL);

        GdkRGBA rgba = {0.0f, 0.0f, 0.0f, 0.0f};
        webkit_web_view_set_background_color(webview, &rgba);
        gtk_widget_set_vexpand(GTK_WIDGET(webview), TRUE);
        gtk_widget_set_hexpand(GTK_WIDGET(webview), TRUE);

        WebKitSettings* settings = webkit_web_view_get_settings(webview);
        webkit_settings_set_enable_javascript(settings, TRUE);
        webkit_settings_set_hardware_acceleration_policy(settings, WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS);

        webkit_web_context_set_cache_model(webkit_web_view_get_context(webview), WEBKIT_CACHE_MODEL_WEB_BROWSER);

        g_signal_connect(gtkWindow, "realize", G_CALLBACK(GtkWidgetRealizeSetRegionsCallback), &monitorInfo);

        gtk_window_set_child(gtkWindow, GTK_WIDGET(webview));
        gtk_window_present(gtkWindow);

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