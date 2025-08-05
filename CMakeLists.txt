cmake_minimum_required(VERSION 3.31)
project(wss)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(JSON_BuildTests OFF CACHE INTERNAL "")

find_package(sdbus-c++ REQUIRED)
find_package(LayerShellQt REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIOMM REQUIRED giomm-2.4)

add_subdirectory(lib/spdlog)
add_subdirectory(lib/tomlplusplus)
add_subdirectory(lib/nlohmann_json)

include_directories(/usr/include/gtk4-layer-shell)
link_directories(/usr/lib)

include_directories(src)

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

message("GLIB_INCLUDE_DIRS: ${GLIB_INCLUDE_DIRS}")
message("GIOMM_INCLUDE_DIRS: ${GIOMM_INCLUDE_DIRS}")

target_include_directories(${PROJECT_NAME} PRIVATE
        ${GLIB_INCLUDE_DIRS}
        ${GIOMM_INCLUDE_DIRS}
        /usr/include/LayerShellQt
        /usr/include/uWebSockets
        lib/cli11
        lib/spdlog/include
        lib/tomlplusplus/include
        lib/nlohmann_json/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${GLIB_LIBRARIES}
        ${GIOMM_LIBRARIES}
        /usr/lib/libusockets.so
        /usr/lib/libz.so
        spdlog
        tomlplusplus::tomlplusplus
        SDBusCpp::sdbus-c++
        Qt6::Widgets
        Qt6::WebEngineWidgets
        LayerShellQt::Interface
        ${NLOHMANN_JSON_TARGET_NAME}

)

set_target_properties(${PROJECT_NAME} PROPERTIES
        AUTOMOC ON
)