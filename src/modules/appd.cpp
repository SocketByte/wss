#include "appd.h"
#include <regex>
#include <shell.h>
#include <sys/inotify.h>

namespace fs = std::filesystem;

std::vector<int> GetAvailableIconSizes(const std::string& iconName) {
    std::vector<int> sizes;
    std::string basePath = "/usr/share/icons/hicolor";

    std::regex sizeDirRegex(R"((\d+)x\1)"); // matches "16x16", "32x32", etc.

    for (const auto& entry : std::filesystem::directory_iterator(basePath)) {
        if (!entry.is_directory())
            continue;

        std::smatch match;
        std::string dirName = entry.path().filename().string();
        if (std::regex_match(dirName, match, sizeDirRegex)) {
            int size = std::stoi(match[1]);

            std::filesystem::path iconPath = entry.path() / "apps" / (iconName + ".png");
            if (std::filesystem::exists(iconPath)) {
                sizes.push_back(size);
            }
        }
    }

    return sizes;
}

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string EncodeBase64(const std::vector<unsigned char>& data) {
    std::string result;
    int val = 0, valb = -6;
    for (uint8_t c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(b64_table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        result.push_back(b64_table[((val << 8) >> (valb + 8)) & 0x3F]);
    while (result.size() % 4)
        result.push_back('=');
    return result;
}

std::optional<fs::path> FindIconPath(const std::string& iconName, int sizeHint = 64) {
    const std::vector<std::string> iconDirs = {
        "/usr/share/icons/hicolor",
        "/usr/share/icons",
        "/usr/share/pixmaps",
        std::string(getenv("HOME")) + "/.local/share/icons",
    };

    const std::vector<std::string> extensions = {".png", ".svg", ".xpm"};

    for (const auto& dir : iconDirs) {
        for (const auto& ext : extensions) {
            fs::path iconPath = fs::path(dir) / (std::to_string(sizeHint) + "x" + std::to_string(sizeHint)) / "apps" / (iconName + ext);
            if (fs::exists(iconPath))
                return iconPath;

            iconPath = fs::path(dir) / (iconName + ext);
            if (fs::exists(iconPath))
                return iconPath;
        }
    }
    return std::nullopt;
}

std::vector<unsigned char> ReadFileBytes(const fs::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

WSS::Application WSS::Appd::ReadDesktopFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file) {
        std::cerr << "Could not open desktop file: " << filePath << "\n";
        return {};
    }
    std::unordered_map<std::string, std::string> entryMap;
    std::string line;
    bool inDesktopEntry = false;

    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        if (line.empty() || line[0] == '#')
            continue;

        if (line[0] == '[' && line.back() == ']') {
            inDesktopEntry = (line == "[Desktop Entry]");
            continue;
        }

        if (!inDesktopEntry)
            continue;

        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Trim
        key.erase(0, key.find_first_not_of(" \t\n\r"));
        key.erase(key.find_last_not_of(" \t\n\r") + 1);
        value.erase(0, value.find_first_not_of(" \t\n\r"));
        value.erase(value.find_last_not_of(" \t\n\r") + 1);

        entryMap[key] = value;
    }

    std::string name = entryMap["Name"];
    std::string comment = entryMap["Comment"];
    std::string exec = entryMap["Exec"];
    std::string icon = entryMap["Icon"];

    if (name.empty() || exec.empty()) {
        return {};
    }

    std::string iconSmallBase64, iconLargeBase64;

    if (!icon.empty()) {
        auto availableSizes = GetAvailableIconSizes(icon); // Returns list like {16, 24, 32, 48, 64, 128, 256}

        auto findClosestSize = [&](int targetSize) -> std::optional<int> {
            if (availableSizes.empty())
                return std::nullopt;

            auto closest = availableSizes.front();
            for (int size : availableSizes) {
                if (std::abs(size - targetSize) < std::abs(closest - targetSize)) {
                    closest = size;
                }
            }
            return closest;
        };

        // Small icon
        if (auto smallSize = findClosestSize(48)) {
            if (auto smallPath = FindIconPath(icon, *smallSize)) {
                iconSmallBase64 = EncodeBase64(ReadFileBytes(*smallPath));
            }
        }
        if (iconSmallBase64.empty()) {
            std::filesystem::path pixmapPath = "/usr/share/pixmaps/" + icon + ".png";
            if (std::filesystem::exists(pixmapPath)) {
                iconSmallBase64 = EncodeBase64(ReadFileBytes(pixmapPath.string()));
            }
        }

        // Large icon
        if (auto largeSize = findClosestSize(128)) {
            if (auto largePath = FindIconPath(icon, *largeSize)) {
                iconLargeBase64 = EncodeBase64(ReadFileBytes(*largePath));
            }
        }
        if (iconLargeBase64.empty()) {
            std::filesystem::path pixmapPath = "/usr/share/pixmaps/" + icon + ".png";
            if (std::filesystem::exists(pixmapPath)) {
                iconLargeBase64 = EncodeBase64(ReadFileBytes(pixmapPath.string()));
            }
        }
    }
    std::string fileId = fs::path(filePath).filename().string();
    if (fileId.ends_with(".desktop")) {
        fileId = fileId.substr(0, fileId.size() - 8);
    }
    return {fileId, name, comment, exec, iconLargeBase64, iconSmallBase64};
}

void WSS::Appd::LoadApplication(const std::string& filePath) {
    fs::path path(filePath);
    if (fs::exists(filePath)) {
        if (Application app = ReadDesktopFile(path.string()); !app.Name.empty()) {
            AddApplication(app.Id, app);
            WSS_DEBUG("Loaded application: Id='{}', Name='{}', Comment='{}', Exec='{}'", app.Id, app.Name, app.Comment, app.Exec);
        } else {
            WSS_WARN("Failed to read application from file: {}", path.string());
        }
    } else {
        WSS_WARN("File does not exist: {}", path.string());
    }
}

void WSS::Appd::WatchApplicationDirectory() {
    const std::string dirToWatch = "/usr/share/applications";

    const int fd = inotify_init1(IN_NONBLOCK);
    if (fd == -1) {
        perror("inotify_init1");
        return;
    }

    int wd = inotify_add_watch(fd, dirToWatch.c_str(), IN_CREATE | IN_MOVED_TO);
    if (wd == -1) {
        perror("inotify_add_watch");
        close(fd);
        return;
    }

    WSS_DEBUG("Watching application directory: {}", dirToWatch);

    constexpr size_t bufLen = 1024 * (sizeof(struct inotify_event) + NAME_MAX + 1);
    char buffer[bufLen];

    while (m_Running) {
        const int length = read(fd, buffer, bufLen);
        if (length < 0 && errno != EAGAIN) {
            perror("read");
            break;
        }

        int i = 0;
        while (i < length) {
            const auto event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
            if (event->len) {
                std::string name(event->name);
                if ((event->mask & IN_CREATE || event->mask & IN_MOVED_TO) && name.ends_with(".desktop")) {
                    fs::path filePath = fs::path(dirToWatch) / name;
                    LoadApplication(filePath.string());
                } else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                    std::lock_guard lock(m_ApplicationsMutex);
                    m_Applications.erase(name);
                } else {
                    WSS_TRACE("Ignored file watch event for file: {}", name);
                }
            }
            i += sizeof(inotify_event) + event->len;
        }

        usleep(100000);
    }

    close(fd);
}

void WSS::Appd::AddApplication(const std::string& name, const Application& app) {
    std::lock_guard lock(m_ApplicationsMutex);
    m_Applications[name] = app;
    SendAppIPC(app);
}

void WSS::Appd::RunApplication(const std::string& prefix, const std::string& appId) {
    std::lock_guard lock(m_ApplicationsMutex);
    auto it = m_Applications.find(appId);
    if (it == m_Applications.end()) {
        WSS_ERROR("Application with ID '{}' not found.", appId);
        return;
    }

    const Application& app = it->second;
    std::string command = prefix + " " + app.Exec;

    int status = system(command.c_str());
    if (status != 0) {
        WSS_ERROR("Failed to run the application with status code: {}", status);
    }

    json_object* payload = json_object_new_object();
    json_object_object_add(payload, "id", json_object_new_string(app.Id.c_str()));
    json_object_object_add(payload, "name", json_object_new_string(app.Name.c_str()));
    json_object_object_add(payload, "status", json_object_new_int(status));
    m_Shell->GetIPC().Broadcast("appd-application-result", payload);
}
void WSS::Appd::SendAppIPC(const Application& app) {
    json_object* payload = json_object_new_object();
    json_object_object_add(payload, "id", json_object_new_string(app.Id.c_str()));
    json_object_object_add(payload, "name", json_object_new_string(app.Name.c_str()));
    json_object_object_add(payload, "comment", json_object_new_string(app.Comment.c_str()));
    json_object_object_add(payload, "exec", json_object_new_string(app.Exec.c_str()));
    json_object_object_add(payload, "iconBase64Large", json_object_new_string(app.IconBase64Large.c_str()));
    json_object_object_add(payload, "iconBase64Small", json_object_new_string(app.IconBase64Small.c_str()));

    m_Shell->GetIPC().Broadcast("appd-application-added", payload);
    json_object_put(payload);
}

void WSS::Appd::Start() {
    m_Thread = std::thread([this]() {
        try {
            WSS_DEBUG("Starting Appd...");
            m_Running = true;
            const std::string appDir = "/usr/share/applications";
            if (!fs::exists(appDir)) {
                WSS_ERROR("Application directory does not exist: {}", appDir);
                return;
            }

            WSS_DEBUG("Loading existing applications from directory: {}", appDir);
            for (const auto& entry : fs::directory_iterator(appDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".desktop") {
                    LoadApplication(entry.path().string());
                }
            }

            WSS_DEBUG("Loaded {} desktop applications.", m_Applications.size());

            WatchApplicationDirectory();
            WSS_DEBUG("Appd stopped.");
        } catch (const std::exception& e) {
            WSS_ERROR("Failed to start Appd: {}", e.what());
        }
    });
}