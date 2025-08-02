cmake_minimum_required(VERSION 3.31)
project(wss)

set(CMAKE_CXX_STANDARD 23)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(WEBKITGTK REQUIRED webkitgtk-6.0)
pkg_check_modules(JSONC REQUIRED json-c)
pkg_check_modules(LIBWS REQUIRED libwebsockets)

add_subdirectory(lib/spdlog)
add_subdirectory(lib/tomlplusplus)

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
        src/dimparser.h
        src/ipc.cpp
        src/ipc.h
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_include_directories(${PROJECT_NAME} PRIVATE
        /usr/include/gtk4-layer-shell
        /usr/include/webkitgtk-6.0/
        /usr/include/webkitgtk-6.0/webkit
        /usr/include/libsoup-3.0
        lib/cli11
        lib/spdlog/include
        lib/tomlplusplus/include
        ${GTK4_INCLUDE_DIRS}
        ${WEBKIT2GTK_INCLUDE_DIRS}
        ${JSONC_INCLUDE_DIRS}
        ${LIBWS_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        gtk4-layer-shell
        webkitgtk-6.0
        ${GTK4_LIBRARIES}
        ${WEBKIT2GTK_LIBRARIES}
        ${JSONC_LIBRARIES}
        ${LIBWS_LIBRARIES}
        spdlog
        tomlplusplus::tomlplusplus
)
target_compile_options(${PROJECT_NAME} PRIVATE
        ${WEBKIT2GTK_CFLAGS_OTHER}
)