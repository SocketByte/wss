#ifndef PCH_H
#define PCH_H

#include <config.h>

#include "json-c/json.h"
#include "libwebsockets.h"
#include <CLI/CLI11.hpp>
#include <sdbus-c++/sdbus-c++.h>
#include <toml++/toml.hpp>

#ifndef WSS_USE_QT
#include <glib.h>
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <webkitgtk-6.0/webkit/webkit.h>
#else
#include <LayerShellQt/shell.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QtWebEngineWidgets/QWebEngineView>
#endif // WSS_USE_QT

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "spdlog/spdlog.h"
#define WSS_INFO(message, ...) spdlog::info(message, ##__VA_ARGS__)
#define WSS_WARN(message, ...) spdlog::warn(message, ##__VA_ARGS__)
#define WSS_ERROR(message, ...) spdlog::error(message, ##__VA_ARGS__)
#define WSS_DEBUG(message, ...) spdlog::debug(message, ##__VA_ARGS__)
#define WSS_TRACE(message, ...) spdlog::trace(message, ##__VA_ARGS__)
#define WSS_CRITICAL(message, ...) spdlog::critical(message, ##__VA_ARGS__)

#define WSS_ASSERT(condition, message)                                                                                                     \
    if (!(condition)) {                                                                                                                    \
        WSS_CRITICAL("Assertion failed: {}, in file {}, line {}", message, __FILE__, __LINE__);                                            \
        std::abort();                                                                                                                      \
    }

#endif // PCH_H
