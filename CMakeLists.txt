cmake_minimum_required(VERSION 3.31)
project(wss)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CPPZMQ_BUILD_TESTS OFF CACHE INTERNAL "")
set(JSON_BuildTests OFF CACHE INTERNAL "")

find_package(sdbus-c++ REQUIRED)
find_package(LayerShellQt REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets)

add_subdirectory(lib/spdlog)
add_subdirectory(lib/tomlplusplus)
add_subdirectory(lib/nlohmann_json)
add_subdirectory(lib/cppzmq)

include_directories(/usr/include/gtk4-layer-shell)
link_directories(/usr/lib)

include_directories(src)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/shell.cpp
        src/widget.cpp
        src/ipc.cpp
        src/modules/notifd.cpp
        src/modules/appd.cpp
        src/dispatch/dispatcher.cpp
        src/dispatch/dispatcher.h
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_include_directories(${PROJECT_NAME} PRIVATE
        {LIBZMQ_INCLUDE_DIRS}
        /usr/include/LayerShellQt
        /usr/include/uWebSockets
        lib/cli11
        lib/spdlog/include
        lib/tomlplusplus/include
        lib/nlohmann_json/include
        lib/cppzmq
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        /usr/lib/libusockets.so
        /usr/lib/libz.so
        spdlog
        tomlplusplus::tomlplusplus
        SDBusCpp::sdbus-c++
        Qt6::Widgets
        Qt6::WebEngineWidgets
        LayerShellQt::Interface
        ${NLOHMANN_JSON_TARGET_NAME}
        ${CPPZMQ_LIBRARIES}
        zmq
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        AUTOMOC ON
)