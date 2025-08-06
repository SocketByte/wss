#ifndef ZMQ_REQ_H
#define ZMQ_REQ_H

#include <pch.h>
#include <stdexcept>
#include <string>
#include <zmq.hpp>

namespace WSS {

class ZMQReq {
    zmq::context_t m_Context;
    zmq::socket_t m_Socket;

  public:
    ZMQReq();
    ~ZMQReq();

    json Request(const std::string& message, const json request, int timeout_ms = 1000);
};

inline ZMQReq::ZMQReq() : m_Context(1), m_Socket(m_Context, zmq::socket_type::req) {
    m_Socket.connect("ipc:///tmp/wss_ipc");

    // Optional: Set socket receive timeout
    m_Socket.set(zmq::sockopt::rcvtimeo, 1000); // default timeout 1000ms
}

inline ZMQReq::~ZMQReq() {
    if (m_Socket.handle() != nullptr)
        m_Socket.close();
    if (m_Context.handle() != nullptr)
        m_Context.close();
}

inline json ZMQReq::Request(const std::string& message, const json request, int timeout_ms) {
    try {
        json req;
        req["type"] = message;
        req["payload"] = request;

        const std::string reqStr = req.dump();
        zmq::message_t zmqReq(reqStr.size());
        memcpy(zmqReq.data(), reqStr.data(), reqStr.size());
        m_Socket.send(zmqReq, zmq::send_flags::none);

        zmq::message_t zmq_resp;
        if (!m_Socket.recv(zmq_resp, zmq::recv_flags::none))
            throw std::runtime_error("ZMQ receive timeout or error");
        std::string resp_str(static_cast<char*>(zmq_resp.data()), zmq_resp.size());

        return json::parse(resp_str);
    } catch (const std::exception& e) {
        throw std::runtime_error("[WSS-ZMQ] Request failed: " + std::string(e.what()));
    }
}

} // namespace WSS

#endif // ZMQ_REQ_H
