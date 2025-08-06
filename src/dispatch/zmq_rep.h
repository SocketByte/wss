#ifndef ZMQ_REP_H
#define ZMQ_REP_H

#include <functional>
#include <pch.h>
#include <string>
#include <thread>
#include <zmq.hpp>

namespace WSS {

class ZMQRep {
    zmq::context_t m_Context;
    zmq::socket_t m_Socket;

    std::atomic_bool m_Running{false};
    std::thread m_Thread;

    std::unordered_map<std::string, std::function<void(const json&)>> m_Listeners;
    std::mutex m_ListenersMutex;

  public:
    ZMQRep() = default;
    ~ZMQRep();

    void RunAsync();
    void Listen(const std::string& type, std::function<void(const json&)> listener);
};

inline ZMQRep::~ZMQRep() {
    if (m_Running) {
        m_Running = false;
        if (m_Thread.joinable()) {
            m_Thread.join();
        }
    }

    if (m_Socket.handle() != nullptr)
        m_Socket.close();
    if (m_Context.handle() != nullptr)
        m_Context.close();
}

inline void ZMQRep::RunAsync() {
    m_Socket = zmq::socket_t(m_Context, zmq::socket_type::rep);
    m_Socket.set(zmq::sockopt::linger, 0); // Set linger to 0 to avoid blocking on close
    m_Thread = std::thread([this]() {
        try {
            m_Running = true;
            m_Socket.bind("ipc:///tmp/wss_ipc");

            while (m_Running) {
                zmq::message_t request;
                auto result = m_Socket.recv(request, zmq::recv_flags::none);
                if (!result) {
                    fprintf(stderr, "WSS-ZMQRep recv failed: %s\n", zmq_strerror(zmq_errno()));
                    continue;
                }

                std::string message(static_cast<char*>(request.data()), request.size());
                WSS_DEBUG("[WSS-ZMQ] Received message: {}", message);
                json response;
                {
                    json msg = json::parse(message, nullptr, false);

                    std::lock_guard lock(m_ListenersMutex);
                    auto it = m_Listeners.find(msg["type"]);
                    if (it != m_Listeners.end()) {
                        try {
                            it->second(msg["payload"]);
                            response = {{"status", "success"}, {"message", "Listener executed successfully"}};
                        } catch (const std::exception& e) {
                            response = {{"error", "Listener execution failed"}, {"details", e.what()}};
                        }
                    } else {
                        response = {{"error", "No listener for this message type"}};
                    }
                }

                std::string responseStr = response.dump();
                zmq::message_t zmqResponse(responseStr.size());
                memcpy(zmqResponse.data(), responseStr.data(), responseStr.size());
                if (!m_Socket.send(zmqResponse, zmq::send_flags::none))
                    fprintf(stderr, "WSS-ZMQRep send failed: %s\n", zmq_strerror(zmq_errno()));
                WSS_DEBUG("[WSS-ZMQ] Sent response: {}", responseStr);
            }

        } catch (const std::exception& e) {
            WSS_ERROR("[WSS-ZMQ] Exception in ZMQRep thread: {}", e.what());
        } catch (...) {
            WSS_ERROR("[WSS-ZMQ] Unknown exception in ZMQRep thread.");
        }
    });
}

inline void ZMQRep::Listen(const std::string& type, std::function<void(const json&)> listener) {
    std::lock_guard lock(m_ListenersMutex);
    if (m_Listeners.find(type) != m_Listeners.end()) {
        WSS_WARN("[WSS-ZMQ] Listener for type '{}' already exists, replacing it.", type);
    }
    m_Listeners[type] = std::move(listener);
    WSS_DEBUG("[WSS-ZMQ] Registered listener for type '{}'", type);
}
} // namespace WSS

#endif // ZMQ_REP_H
