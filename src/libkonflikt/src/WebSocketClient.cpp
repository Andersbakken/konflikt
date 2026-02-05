#include "konflikt/WebSocketClient.h"

#include <libusockets.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

namespace konflikt {

namespace {

// Base64 encoding for WebSocket key
const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const unsigned char *data, size_t len)
{
    std::string result;
    int val = 0;
    int valb = -6;

    for (size_t i = 0; i < len; i++) {
        val = (val << 8) + data[i];
        valb += 8;
        while (valb >= 0) {
            result.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        result.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (result.size() % 4) {
        result.push_back('=');
    }

    return result;
}

std::string generateWebSocketKey()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    unsigned char key[16];
    for (int i = 0; i < 16; i++) {
        key[i] = static_cast<unsigned char>(dis(gen));
    }

    return base64_encode(key, 16);
}

} // namespace

struct WebSocketClient::Impl
{
    // Socket state
    struct us_loop_t *loop { nullptr };
    struct us_socket_context_t *context { nullptr };
    struct us_socket_t *socket { nullptr };

    std::thread clientThread;
    std::atomic<bool> running { false };
    std::atomic<bool> shouldStop { false };

    std::mutex mutex;
    std::queue<std::string> outgoingMessages;
    bool shouldConnect { false };
    bool shouldDisconnect { false };
    std::string connectHost;
    int connectPort { 0 };
    std::string connectPath;
    std::string websocketKey;

    WebSocketClientCallbacks callbacks;
    WebSocketState state { WebSocketState::Disconnected };

    // Receive buffer for partial frames
    std::vector<char> receiveBuffer;
    bool handshakeComplete { false };

    // WebSocket frame helpers
    void sendFrame(uint8_t opcode, const char *data, size_t len)
    {
        if (!socket)
            return;

        std::vector<uint8_t> frame;

        // FIN bit + opcode
        frame.push_back(0x80 | opcode);

        // Mask bit + payload length
        if (len < 126) {
            frame.push_back(0x80 | static_cast<uint8_t>(len));
        } else if (len < 65536) {
            frame.push_back(0x80 | 126);
            frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
            frame.push_back(static_cast<uint8_t>(len & 0xFF));
        } else {
            frame.push_back(0x80 | 127);
            for (int i = 7; i >= 0; i--) {
                frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
            }
        }

        // Masking key (required for client-to-server)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        uint8_t mask[4];
        for (int i = 0; i < 4; i++) {
            mask[i] = static_cast<uint8_t>(dis(gen));
            frame.push_back(mask[i]);
        }

        // Masked payload
        for (size_t i = 0; i < len; i++) {
            frame.push_back(static_cast<uint8_t>(data[i]) ^ mask[i % 4]);
        }

        us_socket_write(0, socket, reinterpret_cast<const char *>(frame.data()), static_cast<int>(frame.size()), 0);
    }

    void processReceivedData(const char *data, int length)
    {
        receiveBuffer.insert(receiveBuffer.end(), data, data + length);

        if (!handshakeComplete) {
            // Look for end of HTTP headers
            std::string bufStr(receiveBuffer.begin(), receiveBuffer.end());
            size_t headerEnd = bufStr.find("\r\n\r\n");

            if (headerEnd != std::string::npos) {
                // Check for successful upgrade
                if (bufStr.find("101") != std::string::npos &&
                    bufStr.find("Upgrade") != std::string::npos) {
                    handshakeComplete = true;
                    state = WebSocketState::Connected;

                    if (callbacks.onConnect) {
                        callbacks.onConnect();
                    }

                    // Remove handshake from buffer
                    receiveBuffer.erase(receiveBuffer.begin(), receiveBuffer.begin() + headerEnd + 4);
                } else {
                    state = WebSocketState::Error;
                    if (callbacks.onError) {
                        callbacks.onError("WebSocket handshake failed");
                    }
                    return;
                }
            } else {
                return; // Wait for more data
            }
        }

        // Process WebSocket frames
        while (receiveBuffer.size() >= 2) {
            size_t offset = 0;
            uint8_t byte0 = static_cast<uint8_t>(receiveBuffer[0]);
            uint8_t byte1 = static_cast<uint8_t>(receiveBuffer[1]);

            bool fin = (byte0 & 0x80) != 0;
            uint8_t opcode = byte0 & 0x0F;
            bool masked = (byte1 & 0x80) != 0;
            uint64_t payloadLen = byte1 & 0x7F;

            offset = 2;

            if (payloadLen == 126) {
                if (receiveBuffer.size() < 4)
                    return;
                payloadLen = (static_cast<uint64_t>(static_cast<uint8_t>(receiveBuffer[2])) << 8) |
                    static_cast<uint64_t>(static_cast<uint8_t>(receiveBuffer[3]));
                offset = 4;
            } else if (payloadLen == 127) {
                if (receiveBuffer.size() < 10)
                    return;
                payloadLen = 0;
                for (int i = 0; i < 8; i++) {
                    payloadLen = (payloadLen << 8) | static_cast<uint64_t>(static_cast<uint8_t>(receiveBuffer[2 + i]));
                }
                offset = 10;
            }

            uint8_t mask[4] = { 0, 0, 0, 0 };
            if (masked) {
                if (receiveBuffer.size() < offset + 4)
                    return;
                for (int i = 0; i < 4; i++) {
                    mask[i] = static_cast<uint8_t>(receiveBuffer[offset + i]);
                }
                offset += 4;
            }

            if (receiveBuffer.size() < offset + payloadLen)
                return;

            // Extract and unmask payload
            std::string payload;
            payload.reserve(payloadLen);
            for (uint64_t i = 0; i < payloadLen; i++) {
                char c = receiveBuffer[offset + i];
                if (masked) {
                    c ^= mask[i % 4];
                }
                payload.push_back(c);
            }

            // Remove processed frame from buffer
            receiveBuffer.erase(receiveBuffer.begin(), receiveBuffer.begin() + offset + payloadLen);

            // Handle frame
            (void)fin; // TODO: handle fragmented frames
            switch (opcode) {
                case 0x01: // Text frame
                case 0x02: // Binary frame
                    if (callbacks.onMessage) {
                        callbacks.onMessage(payload);
                    }
                    break;
                case 0x08: // Close
                    if (socket) {
                        // Send close frame back
                        sendFrame(0x08, nullptr, 0);
                        us_socket_close(0, socket, 0, nullptr);
                    }
                    break;
                case 0x09: // Ping
                    // Send pong
                    sendFrame(0x0A, payload.c_str(), payload.size());
                    break;
                case 0x0A: // Pong
                    // Ignore
                    break;
            }
        }
    }

    // Static callbacks for uSockets
    static struct us_socket_t *onOpen(struct us_socket_t *s, int is_client, char *ip, int ip_length)
    {
        (void)is_client;
        (void)ip;
        (void)ip_length;

        Impl *impl = static_cast<Impl *>(us_socket_context_ext(0, us_socket_context(0, s)));
        impl->socket = s;

        // Send WebSocket handshake
        impl->websocketKey = generateWebSocketKey();
        std::ostringstream request;
        request << "GET " << impl->connectPath << " HTTP/1.1\r\n"
                << "Host: " << impl->connectHost << ":" << impl->connectPort << "\r\n"
                << "Upgrade: websocket\r\n"
                << "Connection: Upgrade\r\n"
                << "Sec-WebSocket-Key: " << impl->websocketKey << "\r\n"
                << "Sec-WebSocket-Version: 13\r\n"
                << "\r\n";

        std::string requestStr = request.str();
        us_socket_write(0, s, requestStr.c_str(), static_cast<int>(requestStr.size()), 0);

        return s;
    }

    static struct us_socket_t *onData(struct us_socket_t *s, char *data, int length)
    {
        Impl *impl = static_cast<Impl *>(us_socket_context_ext(0, us_socket_context(0, s)));
        impl->processReceivedData(data, length);
        return s;
    }

    static struct us_socket_t *onWritable(struct us_socket_t *s)
    {
        Impl *impl = static_cast<Impl *>(us_socket_context_ext(0, us_socket_context(0, s)));

        // Send queued messages
        std::lock_guard<std::mutex> lock(impl->mutex);
        while (!impl->outgoingMessages.empty() && impl->handshakeComplete) {
            const std::string &msg = impl->outgoingMessages.front();
            impl->sendFrame(0x01, msg.c_str(), msg.size()); // Text frame
            impl->outgoingMessages.pop();
        }

        return s;
    }

    static struct us_socket_t *onClose(struct us_socket_t *s, int code, void *reason)
    {
        (void)code;
        (void)reason;

        Impl *impl = static_cast<Impl *>(us_socket_context_ext(0, us_socket_context(0, s)));
        impl->socket = nullptr;
        impl->handshakeComplete = false;

        WebSocketState prevState = impl->state;
        impl->state = WebSocketState::Disconnected;

        if (prevState == WebSocketState::Connected && impl->callbacks.onDisconnect) {
            impl->callbacks.onDisconnect("Connection closed");
        }

        return s;
    }

    static struct us_socket_t *onEnd(struct us_socket_t *s)
    {
        return us_socket_close(0, s, 0, nullptr);
    }

    static struct us_socket_t *onTimeout(struct us_socket_t *s)
    {
        return s;
    }

    static struct us_socket_t *onConnectError(struct us_socket_t *s, int code)
    {
        (void)code;

        Impl *impl = static_cast<Impl *>(us_socket_context_ext(0, us_socket_context(0, s)));
        impl->state = WebSocketState::Error;

        if (impl->callbacks.onError) {
            impl->callbacks.onError("Failed to connect to server");
        }

        return s;
    }

    void run()
    {
        running = true;

        while (!shouldStop) {
            std::string host;
            int port = 0;
            std::string path;

            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!shouldConnect) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                host = connectHost;
                port = connectPort;
                path = connectPath;
                shouldConnect = false;
            }

            state = WebSocketState::Connecting;
            handshakeComplete = false;
            receiveBuffer.clear();

            // Create event loop
            loop = us_create_loop(nullptr, [](struct us_loop_t *) {
            }, [](struct us_loop_t *) {
            }, [](struct us_loop_t *) {
            }, sizeof(Impl *));

            // Store our pointer in the loop extension
            *static_cast<Impl **>(us_loop_ext(loop)) = this;

            // Create socket context
            struct us_socket_context_options_t options = {};
            context = us_create_socket_context(0, loop, sizeof(Impl *), options);

            if (!context) {
                state = WebSocketState::Error;
                if (callbacks.onError) {
                    callbacks.onError("Failed to create socket context");
                }
                us_loop_free(loop);
                loop = nullptr;
                continue;
            }

            // Store our pointer in the context extension
            *static_cast<Impl **>(us_socket_context_ext(0, context)) = this;

            // Set up callbacks
            us_socket_context_on_open(0, context, onOpen);
            us_socket_context_on_data(0, context, onData);
            us_socket_context_on_writable(0, context, onWritable);
            us_socket_context_on_close(0, context, onClose);
            us_socket_context_on_end(0, context, onEnd);
            us_socket_context_on_timeout(0, context, onTimeout);
            us_socket_context_on_connect_error(0, context, onConnectError);

            // Connect
            socket = us_socket_context_connect(0, context, host.c_str(), port, nullptr, 0, 0);

            if (!socket) {
                state = WebSocketState::Error;
                if (callbacks.onError) {
                    callbacks.onError("Failed to initiate connection");
                }
                us_socket_context_free(0, context);
                us_loop_free(loop);
                context = nullptr;
                loop = nullptr;
                continue;
            }

            // Run loop until disconnected or stopped
            while (!shouldStop && state != WebSocketState::Disconnected && state != WebSocketState::Error) {
                // Process queued messages
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    if (shouldDisconnect && socket) {
                        sendFrame(0x08, nullptr, 0); // Close frame
                        us_socket_close(0, socket, 0, nullptr);
                        shouldDisconnect = false;
                    }

                    // Send queued messages
                    while (!outgoingMessages.empty() && handshakeComplete && socket) {
                        const std::string &msg = outgoingMessages.front();
                        sendFrame(0x01, msg.c_str(), msg.size());
                        outgoingMessages.pop();
                    }
                }

                // Run one iteration of the loop
                us_loop_run(loop);
            }

            // Cleanup
            if (context) {
                us_socket_context_free(0, context);
                context = nullptr;
            }
            if (loop) {
                us_loop_free(loop);
                loop = nullptr;
            }
            socket = nullptr;
        }

        running = false;
    }
};

WebSocketClient::WebSocketClient()
    : m_impl(std::make_unique<Impl>())
{
}

WebSocketClient::~WebSocketClient()
{
    disconnect();
    m_impl->shouldStop = true;
    if (m_impl->clientThread.joinable()) {
        m_impl->clientThread.join();
    }
}

void WebSocketClient::setCallbacks(WebSocketClientCallbacks callbacks)
{
    m_impl->callbacks = std::move(callbacks);
}

bool WebSocketClient::connect(const std::string &host, int port, const std::string &path)
{
    m_host = host;
    m_port = port;

    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->connectHost = host;
        m_impl->connectPort = port;
        m_impl->connectPath = path.empty() ? "/" : path;
        m_impl->shouldConnect = true;
    }

    // Start client thread if not running
    if (!m_impl->clientThread.joinable()) {
        m_impl->clientThread = std::thread([this]() {
            m_impl->run();
        });
    }

    m_state = WebSocketState::Connecting;
    return true;
}

void WebSocketClient::disconnect()
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->shouldDisconnect = true;
}

void WebSocketClient::send(const std::string &message)
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->outgoingMessages.push(message);
}

void WebSocketClient::poll()
{
    m_state = m_impl->state;
}

} // namespace konflikt
