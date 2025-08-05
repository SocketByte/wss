#include "shell.h"

#include <csignal>

#include "modules/notifd.h"
#include "util/dimparser.h"

void WSS::Shell::LoadConfig(const std::string& configPath) {
    toml::table config;
    try {
        config = toml::parse_file(configPath);
    } catch (const toml::parse_error& err) {
        WSS_ERROR("Failed to parse configuration file at {}: {}", configPath, err.description());
    }

    if (config.empty()) {
        WSS_WARN("Configuration file at {} is empty or invalid.", configPath);
        return;
    }

    toml::table* settingsConfig = config.get("settings")->as_table();
    m_Settings.m_FrontendPort =
        settingsConfig->get("frontend_port") ? settingsConfig->get("frontend_port")->value_or<int>(3000) : 0;
    m_Settings.m_IpcPort = settingsConfig->get("ipc_port") ? settingsConfig->get("ipc_port")->value_or<int>(8080) : 0;
    m_Settings.m_NotificationTimeout =
        settingsConfig->get("notification_timeout") ? settingsConfig->get("notification_timeout")->value_or<int>(5000) : 0;
    WSS_INFO("Loaded configuration.");
}

static void HandleSignal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        WSS::IsRunning = false;
        WSS_INFO("Shutdown signal received (signal: {}).", signal);
        // If GTK is running, this safely quits the main loop
#ifndef WSS_USE_QT
        g_application_quit(g_application_get_default());
#else

#endif
    }
}

#ifndef WSS_USE_QT
int WSS::Shell::Init(const std::string& appId, const GApplicationFlags flags, const std::string& configPath) {
    WSS_ASSERT(!appId.empty(), "Application ID must not be empty.");

    // Register shutdown signal handlers
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    const auto activateData = std::make_shared<ActivateCallbackData>();
    activateData->shell = this;
    activateData->configPath = configPath;
    LoadConfig(configPath);

    m_Application = GTK_APPLICATION(gtk_application_new(appId.c_str(), flags));
    g_signal_connect(m_Application, "activate", G_CALLBACK(OnActivate), activateData.get());

    // Run the additional daemons
    m_IPC.Start();
    m_Notifd.Start();

    WSS_INFO("WSS is running. Press Ctrl+C to exit.");
    WSS_INFO("Application ID: {}", appId);

    const int status = g_application_run(G_APPLICATION(m_Application), 0, nullptr);
    if (status != 0) {
        WSS_ERROR("Failed to run the GTK application with status code: {}", status);
    }

    return status;
}
#endif

#ifdef WSS_USE_QT
int WSS::Shell::Init(const std::string& appId, const std::string& configPath) {
    int argc = 1;
    char* argv[] = {const_cast<char*>(appId.c_str()), nullptr};

    LayerShellQt::Shell::useLayerShell();
    QApplication app(argc, argv);

    // std::signal(SIGINT, HandleSignal);
    // std::signal(SIGTERM, HandleSignal);

    LoadConfig(configPath);

    // Yeah...
    qputenv("QT_WEBENGINE_CHROMIUM_FLAGS",
            "--use-gl=egl --enable-zero-copy --ozone-platform=wayland -ozone-platform-hint=auto "
            "--ignore-gpu-blocklist "
            "--enable-gpu-rasterization --disable-frame-rate-limit --no-sandbox"
            "--disable-software-rasterizer --disable-software-vsync --use-vulkan "
            "--enable-unsafe-webgpu --disable-sync-preferences --disable-gpu-vsync "
            "--disable-features=UseSkiaRenderer,UseChromeOSDirectVideoDecoder"
            "--enable-native-gpu-memory-buffers --enable-gpu-memory-buffer-video-frames"
            "--enable-features=Vulkan,VaapiVideoEncoder,VaapiVideoDecoder,CanvasOopRasterization");
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");
    qputenv("QT_QPA_PLATFORM", "wayland");
    qputenv("EGL_PLATFORM", "wayland");

    // QObject::connect(qApp, &QCoreApplication::aboutToQuit, []() {
    //     WSS_INFO("Application is quitting. Cleaning up resources...");
    //     WSS::IsRunning = false;
    //     // If GTK is running, this safely quits the main loop
    //     for (QWidget* w : QApplication::topLevelWidgets()) {
    //         if (auto* webView = qobject_cast<QWebEngineView*>(w)) {
    //             webView->page()->setUrl(QUrl("about:blank")); // Detach heavy content
    //             webView->deleteLater();                       // Mark for deletion
    //         }
    //     }
    //     QCoreApplication::processEvents(QEventLoop::AllEvents, 200);
    //     QApplication::quit();
    // });

    const auto activateData = std::make_shared<ActivateCallbackData>();
    activateData->shell = this;
    activateData->configPath = configPath;
    OnActivate(&app, activateData.get());
    return QApplication::exec();
}
#endif

void WSS::Shell::OnActivate(RenderApplication* app, ActivateCallbackPtr data) {
    WSS_ASSERT(app != nullptr, "GTK Application must not be null.");
    auto* activateData = static_cast<ActivateCallbackData*>(data);
    auto& shell = *activateData->shell;
    const std::string& configPath = activateData->configPath;

    toml::table config;
    try {
        config = toml::parse_file(configPath);
    } catch (const toml::parse_error& err) {
        WSS_ERROR("Failed to parse configuration file at {}: {}", configPath, err.description());
    }

    if (config.empty()) {
        WSS_WARN("Configuration file at {} is empty or invalid.", configPath);
        return;
    }

    toml::table* widgets = config.get("widgets")->as_table();
    WSS_ASSERT(widgets != nullptr, "Widgets configuration must be a table.");
    WSS_ASSERT(!widgets->empty(), "Widgets configuration must not be empty.");

    WSS_INFO("Found {} widgets in configuration.", widgets->size());
    for (const auto& [widgetName, node] : *widgets) {
        const auto info = node.as_table();
        auto name = std::string(widgetName.str());

        WSS_DEBUG("Processing widget: {}", name);
        if (!info) {
            WSS_ERROR("Invalid widget configuration for '{}'. Expected a table.", name);
            continue;
        }

        std::string route = info->get("route") ? info->get("route")->value_or<std::string>("") : "";
        std::string width = info->get("width") ? info->get("width")->value_or<std::string>("") : "";
        std::string height = info->get("height") ? info->get("height")->value_or<std::string>("") : "";
        std::string layer = info->get("layer") ? info->get("layer")->value_or<std::string>("") : "";
        toml::array* anchor = info->get("anchor") ? info->get("anchor")->as_array() : nullptr;
        toml::array* monitors = info->get("monitors") ? info->get("monitors")->as_array() : nullptr;
        int exclusivityZone = info->get("exclusivity_zone") ? info->get("exclusivity_zone")->value_or<int>(0) : 0;
        bool exclusivity = info->get("exclusivity") ? info->get("exclusivity")->value_or<bool>(false) : false;
        bool hidden = info->get("hidden") ? info->get("hidden")->value_or<bool>(false) : false;

        std::string marginTop = info->get("margin_top") ? info->get("margin_top")->value_or<std::string>("0") : "0";
        std::string marginBottom = info->get("margin_bottom") ? info->get("margin_bottom")->value_or<std::string>("0") : "0";
        std::string marginLeft = info->get("margin_left") ? info->get("margin_left")->value_or<std::string>("0") : "0";
        std::string marginRight = info->get("margin_right") ? info->get("margin_right")->value_or<std::string>("0") : "0";

        int _QtPadding =
            info->get("__QT_auto_click_region_padding") ? info->get("__QT_auto_click_region_padding")->value_or<int>(0) : 0;

        std::vector<WidgetMonitorInfo> monitorIds;
        if (!monitors) {
            WSS_ERROR("Monitors configuration is required for widget '{}'.", name);
            continue;
        }
        for (const auto& a : *monitors) {
            try {
                uint8_t monitorId = a.value_or<uint8_t>(0);
                if (monitorId < 0) {
                    WSS_ERROR("Monitor ID '{}' cannot be negative in configuration for '{}'.", monitorId, name);
                    continue;
                }

                std::unordered_map<std::string, WidgetClickRegionInfo> clickRegionMap;
                auto clickRegions = info->get("click_regions")->as_array();
                if (!clickRegions) {
                    WSS_ERROR("Click regions configuration is required for widget '{}'.", name);
                    continue;
                }
                for (const auto& region : *clickRegions) {
                    if (!region.is_table()) {
                        WSS_ERROR("Click region must be a table in configuration for '{}'.", name);
                        continue;
                    }
                    auto regionTable = region.as_table();
                    std::string regionName = regionTable->get("name")->value_or<std::string>("");
                    int x = DimensionParser::Parse(DimensionParser::DimensionType::WIDTH,
                                                   regionTable->get("x")->value_or<std::string>("0"), monitorId);
                    int y = DimensionParser::Parse(DimensionParser::DimensionType::HEIGHT,
                                                   regionTable->get("y")->value_or<std::string>("0"), monitorId);
                    int regionWidth = DimensionParser::Parse(DimensionParser::DimensionType::WIDTH,
                                                             regionTable->get("width")->value_or<std::string>("0"), monitorId);
                    int regionHeight = DimensionParser::Parse(DimensionParser::DimensionType::HEIGHT,
                                                              regionTable->get("height")->value_or<std::string>("0"), monitorId);

                    clickRegionMap[regionName] = {.X = x, .Y = y, .Width = regionWidth, .Height = regionHeight};
                }

                monitorIds.push_back(
                    {.MonitorId = monitorId,
                     .Width = DimensionParser::Parse(DimensionParser::DimensionType::WIDTH, width, monitorId),
                     .Height = DimensionParser::Parse(DimensionParser::DimensionType::HEIGHT, height, monitorId),
                     .MarginTop = DimensionParser::Parse(DimensionParser::DimensionType::HEIGHT, marginTop, monitorId),
                     .MarginBottom = DimensionParser::Parse(DimensionParser::DimensionType::HEIGHT, marginBottom, monitorId),
                     .MarginLeft = DimensionParser::Parse(DimensionParser::DimensionType::WIDTH, marginLeft, monitorId),
                     .MarginRight = DimensionParser::Parse(DimensionParser::DimensionType::WIDTH, marginRight, monitorId),
                     .ClickRegionMap = std::move(clickRegionMap)});
            } catch (const std::invalid_argument& e) {
                WSS_ERROR("Invalid monitor ID in configuration for '{}': {}", name, e.what());
            } catch (const std::out_of_range& e) {
                WSS_ERROR("Monitor ID out of range in configuration for '{}': {}", name, e.what());
            } catch (...) {
                WSS_ERROR("Unknown error while processing monitor ID in configuration for '{}'.", name);
            }
        }

        uint8_t anchorBitmask = 0;
        if (!anchor) {
            WSS_ERROR("Anchor configuration is required for widget '{}'.", name);
            continue;
        }
        for (const auto& a : *anchor) {
            if (a.is_string()) {
                auto anchorStr = a.value_or<std::string>("");
                if (anchorStr == "top") {
                    anchorBitmask |= static_cast<uint8_t>(WidgetAnchor::TOP);
                } else if (anchorStr == "bottom") {
                    anchorBitmask |= static_cast<uint8_t>(WidgetAnchor::BOTTOM);
                } else if (anchorStr == "left") {
                    anchorBitmask |= static_cast<uint8_t>(WidgetAnchor::LEFT);
                } else if (anchorStr == "right") {
                    anchorBitmask |= static_cast<uint8_t>(WidgetAnchor::RIGHT);
                } else {
                    WSS_ERROR("Invalid anchor '{}' in configuration for '{}'.", anchorStr, name);
                }
            } else {
                WSS_ERROR("Anchor must be a string in configuration for '{}'.", name);
            }
        }

        if (anchorBitmask == 0) {
            WSS_ERROR("At least one anchor must be specified for widget '{}'.", name);
            continue;
        }

        if (layer.empty()) {
            WSS_ERROR("Layer is required for widget '{}'.", name);
            continue;
        }

        if (monitorIds.empty()) {
            WSS_ERROR("At least one monitor ID is required for widget '{}'.", name);
            continue;
        }

        WidgetLayer widgetLayer;
        if (layer == "top") {
            widgetLayer = WidgetLayer::TOP;
        } else if (layer == "bottom") {
            widgetLayer = WidgetLayer::BOTTOM;
        } else if (layer == "overlay") {
            widgetLayer = WidgetLayer::OVERLAY;
        } else if (layer == "background") {
            widgetLayer = WidgetLayer::BACKGROUND;
        } else {
            WSS_ERROR("Invalid layer '{}' for widget '{}'.", layer, name);
            continue;
        }

        WidgetInfo widgetInfo{.Name = std::string(name),
                              .Route = route,
                              .Monitors = std::move(monitorIds),
                              .Layer = widgetLayer,
                              .AnchorBitmask = anchorBitmask,
                              .ExclusivityZone = exclusivityZone,
                              .Exclusivity = exclusivity,
                              .DefaultHidden = hidden,
                              ._QT_padding = _QtPadding};

        WSS_DEBUG(
            "Creating widget with info: Name='{}', Route='{}', Layer='{}', "
            "AnchorBitmask='{}', Exclusivity='{}', DefaultHidden='{}'",
            widgetInfo.Name, widgetInfo.Route, static_cast<int>(widgetInfo.Layer), static_cast<int>(widgetInfo.AnchorBitmask),
            widgetInfo.Exclusivity, widgetInfo.DefaultHidden);

        for (const auto& monitor : widgetInfo.Monitors) {
            WSS_DEBUG(
                "-- Monitor ID: {}, Width: {}, Height: {}, Margins: (Top: {}, Bottom: {}, Left: "
                "{}, Right: {})",
                monitor.MonitorId, monitor.Width, monitor.Height, monitor.MarginTop, monitor.MarginBottom, monitor.MarginLeft,
                monitor.MarginRight);
            for (const auto& [regionName, regionInfo] : monitor.ClickRegionMap) {
                WSS_DEBUG("   -- Click Region: {}, X: {}, Y: {}, Width: {}, Height: {}", regionName, regionInfo.X, regionInfo.Y,
                          regionInfo.Width, regionInfo.Height);
            }
        }

        auto widget = std::make_shared<Widget>(widgetInfo);
        widget->Create(shell);
        shell.m_Widgets.emplace(name, std::move(widget));
    }

    shell.m_IPC.Start();
    shell.m_Notifd.Start();
    shell.m_Appd.Start();
}