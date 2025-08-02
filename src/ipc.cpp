#include "ipc.h"
#include "shell.h"

static int callback_ws(lws* wsi, const lws_callback_reasons reason, void* user, void* in, const size_t len) {
    auto* shell = static_cast<WSS::Shell*>(lws_context_user(lws_get_context(wsi)));
    if (!shell) {
        WSS_ERROR("Shell instance is null in WebSocket callback.");
        return -1;
    }

    switch (reason) {
    case LWS_CALLBACK_RECEIVE: {
        json_object* jobj = json_tokener_parse(static_cast<const char*>(in));
        if (!jobj) {
            WSS_ERROR("Failed to parse JSON from received message.");
            return -1;
        }

        json_object *type_obj, *payload_obj;
        if (!json_object_object_get_ex(jobj, "type", &type_obj) || !json_object_object_get_ex(jobj, "payload", &payload_obj)) {
            WSS_ERROR("Received JSON does not contain 'type' or 'payload' fields.");
            json_object_put(jobj);
            return -1;
        }

        const char* type = json_object_get_string(type_obj);
        WSS_DEBUG("Received IPC message of type: {}", type);
        WSS_DEBUG("-- Payload: {}", json_object_to_json_string(payload_obj));
        break;
    }
    default:
        break;
    }

    return 0;
}

WSS::IPC::~IPC() {
    if (m_Running) {
        m_Running = false;
        if (m_Context) {
            lws_cancel_service(m_Context); // This wakes up lws_service()
        }
        if (m_Thread.joinable()) {
            m_Thread.join();
        }
    }
    WSS_DEBUG("IPC context destroyed and resources cleaned up.");
}

void WSS::IPC::Start() {
    WSS_ASSERT(m_Shell != nullptr, "Shell instance must not be null.");
    m_Running = true;
    m_Thread = std::thread([this]() {
        try {
            lws_protocols protocols[] = {{"wss.ipc", callback_ws, 0, 4096}, {nullptr, nullptr, 0, 0}};

            lws_context_creation_info info{};
            info.port = 8080;
            info.protocols = protocols;
            info.gid = -1;
            info.uid = -1;
            info.user = m_Shell;

            m_Context = lws_create_context(&info);
            if (!m_Context) {
                WSS_ERROR("Failed to create IPC context");
                return;
            }

            while (m_Running) {
                int ret = lws_service(m_Context, 0);
                if (ret < 0) {
                    WSS_ERROR("IPC service error: {}", ret);
                    break;
                }
            }

            WSS_DEBUG("IPC service loop exited, cleaning up resources.");
        } catch (const std::exception& e) {
            WSS_ERROR("Unhandled exception in IPC thread: {}", e.what());
        } catch (...) {
            WSS_ERROR("Unknown exception occurred in IPC thread.");
        }
    });
}