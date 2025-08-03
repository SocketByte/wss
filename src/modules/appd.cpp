#include "appd.h"
#include <sys/inotify.h>

void WSS::Appd::WatchApplicationDirectory() {
    const std::string dirToWatch = "/usr/share/applications";

    int fd = inotify_init1(IN_NONBLOCK);
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

    std::cout << "Watching for new apps in: " << dirToWatch << std::endl;

    const size_t bufLen = 1024 * (sizeof(struct inotify_event) + NAME_MAX + 1);
    char buffer[bufLen];

    while (true) {
        int length = read(fd, buffer, bufLen);
        if (length < 0 && errno != EAGAIN) {
            perror("read");
            break;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            if (event->len) {
                std::string name(event->name);
                if ((event->mask & IN_CREATE || event->mask & IN_MOVED_TO) && name.ends_with(".desktop")) {
                    std::cout << "New application installed: " << name << std::endl;
                    // Refresh your app list here
                }
            }
            i += sizeof(struct inotify_event) + event->len;
        }

        usleep(100000);
    }

    close(fd);
}

void WSS::Appd::Start() {
    m_Thread = std::thread([this]() {
        try {
            WSS_DEBUG("Starting Appd...");
            m_Running = true;
            while (m_Running) {
            }
            WSS_DEBUG("Appd stopped.");
        } catch (const std::exception& e) {
            WSS_ERROR("Failed to start Appd: {}", e.what());
        }
    });
}