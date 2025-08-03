cmake_minimum_required(VERSION 3.31)
project(wss)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)

find_package(sdbus-c++ REQUIRED)
find_package(LayerShellQt REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(WEBKITGTK REQUIRED webkitgtk-6.0)
pkg_check_modules(JSONC REQUIRED json-c)
pkg_check_modules(LIBWS REQUIRED libwebsockets)
pkg_check_modules(SOUP REQUIRED libsoup-3.0)

add_subdirectory(lib/spdlog)
add_subdirectory(lib/tomlplusplus)

include_directories(/usr/include/gtk4-layer-shell)
link_directories(/usr/lib)

include_directories(src)

if (DEFINED WSS_USE_QT)
    add_compile_definitions(${PROJECT_NAME} -DWSS_USE_QT)
    message(STATUS "Building with Qt support")
endif ()

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/pch.h
        src/shell.cpp
        src/shell.h
        src/widget.cpp
        src/widget.h
        src/ipc.cpp
        src/ipc.h
        src/modules/notifd.cpp
        src/modules/notifd.h
        src/util/dbus_vreader.h
        src/modules/appd.cpp
        src/modules/appd.h
        src/config.h
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_include_directories(${PROJECT_NAME} PRIVATE
        /usr/include/gtk4-layer-shell
        /usr/include/webkitgtk-6.0/
        /usr/include/webkitgtk-6.0/webkit
        /usr/include/LayerShellQt
        lib/cli11
        lib/spdlog/include
        lib/tomlplusplus/include
        ${GTK4_INCLUDE_DIRS}
        ${WEBKITGTK_INCLUDE_DIRS}
        ${JSONC_INCLUDE_DIRS}
        ${LIBWS_INCLUDE_DIRS}
        ${SOUP_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        gtk4-layer-shell
        ${GTK4_LIBRARIES}
        ${WEBKITGTK_LIBRARIES}
        ${JSONC_LIBRARIES}
        ${LIBWS_LIBRARIES}
        ${SOUP_LIBRARIES}
        ${WAYLAND_LIBRARIES}
        spdlog
        tomlplusplus::tomlplusplus
        SDBusCpp::sdbus-c++
        Qt6::Widgets
        Qt6::WebEngineWidgets
        LayerShellQt::Interface
)
target_compile_options(${PROJECT_NAME} PRIVATE
        ${WEBKIT2GTK_CFLAGS_OTHER}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        AUTOMOC ON
)