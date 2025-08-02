#include "shell.h"

#include <iostream>

#include <pch.h>

int LaunchApplication(const std::string& configPath) {
    WSS_INFO("Initializing Web Shell System (WSS)...");
    WSS_INFO("-- GTK version: {}.{}.{}", gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version());
    WSS_INFO("-- GTK Layer Shell version: {}.{}.{}", gtk_layer_get_major_version(), gtk_layer_get_minor_version(),
             gtk_layer_get_micro_version());
    WSS_INFO("-- WebKitGTK version: {}.{}.{}", webkit_get_major_version(), webkit_get_minor_version(), webkit_get_micro_version());

    if (getenv("HYPRLAND_INSTANCE_SIGNATURE") == nullptr) {
        WSS_WARN("Not running on Hyprland. Some features may not work as expected.");
    }

    if (!gtk_layer_is_supported()) {
        WSS_CRITICAL("Layer shell protocol is not supported on this platform.");
        WSS_CRITICAL("Ensure you are running on Wayland with a compatible compositor.");
        return EXIT_FAILURE;
    }

    WSS_INFO("Running on Wayland with layer shell support.");
    if (gtk_get_major_version() < 4 || (gtk_get_major_version() == 4 && gtk_get_minor_version() < 18)) {
        WSS_CRITICAL("GTK version 4.18 or higher is required.");
        WSS_CRITICAL("Update your system or install the latest version of GTK.");
        return EXIT_FAILURE;
    }

    if (webkit_get_major_version() < 2 || (webkit_get_major_version() == 2 && webkit_get_minor_version() < 48)) {
        WSS_CRITICAL("WebkitGTK version 2.48 or higher is required.");
        WSS_CRITICAL("Update your system or install the latest version of WebkitGTK.");
        return EXIT_FAILURE;
    }

    WSS_INFO("Using configuration file at {}", configPath);
    WSS_INFO("All dependencies are satisfied.");

#ifdef WSS_EXPERIMENTAL
    WSS_WARN("This is an experimental version of WSS.");
    WSS_WARN("Use at your own risk and be aware that it may not be stable.");
    WSS_WARN("Please report any issues you encounter. I will be very thankful :)");
    WSS_WARN("For more information, visit: https://github.com/SocketByte/web-shell-system");
#endif

    WSS::Shell shell;
    shell.Init("wss.shell", G_APPLICATION_DEFAULT_FLAGS, configPath);

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);

    const char* configDir = std::getenv("XDG_CONFIG_HOME");
    if (!configDir) {
        configDir = std::getenv("HOME");
    }
    std::string configPath = std::string(configDir) + "/wss/config.toml";
    if (!std::filesystem::exists(configPath)) {
        std::filesystem::create_directories(std::filesystem::path(configPath).parent_path());
        if (std::ofstream configFile(configPath); configFile.is_open()) {
            configFile << "# Web Shell System Configuration\n";
            configFile << "# Add your configuration options here.\n";
            configFile.close();
            WSS_INFO("Created configuration file at {}", configPath);
        } else {
            WSS_ERROR("Failed to create configuration file at {}", configPath);
            return EXIT_FAILURE;
        }
    }

    CLI::App app{"Web Shell System (WSS)"};
    app.require_subcommand(1);
    app.set_help_flag("-h,--help", "Display help message");
    app.set_version_flag("-v,--version", "Display version information");

    std::string customConfigPath;
    bool debugMode = false;
    const auto run = app.add_subcommand("start", "Run the Web Shell System");
    run->add_option("-c,--config", customConfigPath, "Path to the configuration file")->default_val(configPath)->check(CLI::ExistingFile);
    run->add_flag("-d,--debug", debugMode, "Enable debug mode for verbose logging");

    run->callback([&customConfigPath, &debugMode] {
        if (debugMode) {
            spdlog::set_level(spdlog::level::debug);
        }
        exit(LaunchApplication(customConfigPath));
    });

    CLI11_PARSE(app, argc, argv);
    spdlog::shutdown();
    return 0;
}
