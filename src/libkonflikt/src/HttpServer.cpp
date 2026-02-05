#include "konflikt/HttpServer.h"

#include <App.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace konflikt {

namespace {

std::string getMimeType(const std::string &path)
{
    static const std::unordered_map<std::string, std::string> mimeTypes = {
        { ".html", "text/html" },
        { ".htm", "text/html" },
        { ".css", "text/css" },
        { ".js", "application/javascript" },
        { ".mjs", "application/javascript" },
        { ".json", "application/json" },
        { ".png", "image/png" },
        { ".jpg", "image/jpeg" },
        { ".jpeg", "image/jpeg" },
        { ".gif", "image/gif" },
        { ".svg", "image/svg+xml" },
        { ".ico", "image/x-icon" },
        { ".woff", "font/woff" },
        { ".woff2", "font/woff2" },
        { ".ttf", "font/ttf" },
        { ".txt", "text/plain" },
        { ".xml", "application/xml" },
    };

    auto ext = std::filesystem::path(path).extension().string();
    auto it = mimeTypes.find(ext);
    if (it != mimeTypes.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

std::string readFile(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }
    return std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
}

} // namespace

struct HttpServer::Impl
{
    std::thread serverThread;
    std::atomic<bool> running { false };
    us_listen_socket_t *listenSocket { nullptr };
    uWS::Loop *loop { nullptr };

    std::unordered_map<std::string, RouteHandler> routes;
    std::string staticDir;
    std::string staticPrefix;
    int port { 0 };

    void run(int requestedPort)
    {
        uWS::App app;

        // Serve static files
        if (!staticDir.empty()) {
            app.get(staticPrefix + "*", [this](auto *res, auto *req) {
                std::string urlPath(req->getUrl());

                // Remove prefix to get relative path
                std::string relativePath = urlPath.substr(staticPrefix.length());
                if (relativePath.empty() || relativePath == "/") {
                    relativePath = "index.html";
                }

                std::string filePath = staticDir + "/" + relativePath;

                // Security: prevent directory traversal
                auto canonical = std::filesystem::weakly_canonical(filePath);
                auto staticCanonical = std::filesystem::weakly_canonical(staticDir);
                if (canonical.string().find(staticCanonical.string()) != 0) {
                    res->writeStatus("403 Forbidden")->end("Forbidden");
                    return;
                }

                if (!std::filesystem::exists(filePath)) {
                    res->writeStatus("404 Not Found")->end("Not Found");
                    return;
                }

                std::string content = readFile(filePath);
                std::string mimeType = getMimeType(filePath);

                res->writeHeader("Content-Type", mimeType);
                res->end(content);
            });
        }

        // Redirect root to UI
        app.get("/", [this](auto *res, auto * /*req*/) {
            if (!staticPrefix.empty()) {
                res->writeStatus("302 Found");
                res->writeHeader("Location", staticPrefix);
                res->end();
            } else {
                res->end("Konflikt Server");
            }
        });

        // Custom routes
        for (const auto &[key, handler] : routes) {
            // Parse method and path from key (format: "METHOD /path")
            auto spacePos = key.find(' ');
            if (spacePos == std::string::npos)
                continue;

            std::string method = key.substr(0, spacePos);
            std::string path = key.substr(spacePos + 1);

            auto routeHandler = [handler](auto *res, auto *req) {
                HttpRequest httpReq;
                httpReq.method = std::string(req->getMethod());
                httpReq.path = std::string(req->getUrl());
                httpReq.query = std::string(req->getQuery());

                // Get commonly used headers
                auto contentType = req->getHeader("content-type");
                if (!contentType.empty()) {
                    httpReq.headers["content-type"] = std::string(contentType);
                }
                auto accept = req->getHeader("accept");
                if (!accept.empty()) {
                    httpReq.headers["accept"] = std::string(accept);
                }

                HttpResponse httpRes = handler(httpReq);

                res->writeStatus(std::to_string(httpRes.statusCode) + " " + httpRes.statusMessage);
                res->writeHeader("Content-Type", httpRes.contentType);
                for (const auto &[hkey, hvalue] : httpRes.headers) {
                    res->writeHeader(hkey, hvalue);
                }
                res->end(httpRes.body);
            };

            if (method == "GET") {
                app.get(path, routeHandler);
            } else if (method == "POST") {
                app.post(path, routeHandler);
            } else if (method == "PUT") {
                app.put(path, routeHandler);
            } else if (method == "DELETE") {
                app.del(path, routeHandler);
            }
        }

        app.listen(requestedPort, [this, requestedPort](us_listen_socket_t *socket) {
            if (socket) {
                listenSocket = socket;
                port = us_socket_local_port(false, reinterpret_cast<us_socket_t *>(socket));
                running = true;
            } else {
                port = 0;
            }
        });

        loop = uWS::Loop::get();
        app.run();

        running = false;
    }
};

HttpServer::HttpServer(int port)
    : m_impl(std::make_unique<Impl>())
    , m_port(port)
{
}

HttpServer::~HttpServer()
{
    stop();
}

void HttpServer::route(const std::string &method, const std::string &path, RouteHandler handler)
{
    m_impl->routes[method + " " + path] = std::move(handler);
}

void HttpServer::serveStatic(const std::string &urlPrefix, const std::string &directory)
{
    m_impl->staticPrefix = urlPrefix;
    m_impl->staticDir = directory;
    m_staticDir = directory;
    m_staticPrefix = urlPrefix;
}

bool HttpServer::start()
{
    if (m_running) {
        return true;
    }

    m_impl->serverThread = std::thread([this]() {
        m_impl->run(m_port);
    });

    // Wait for server to start
    while (!m_impl->running && m_impl->serverThread.joinable()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (m_impl->port == 0 && m_impl->listenSocket == nullptr) {
            break;
        }
    }

    m_running = m_impl->running;
    if (m_running) {
        m_port = m_impl->port;
    }
    return m_running;
}

void HttpServer::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    m_impl->running = false;

    if (m_impl->listenSocket) {
        us_listen_socket_close(false, m_impl->listenSocket);
        m_impl->listenSocket = nullptr;
    }

    if (m_impl->serverThread.joinable()) {
        m_impl->serverThread.join();
    }
}

} // namespace konflikt
