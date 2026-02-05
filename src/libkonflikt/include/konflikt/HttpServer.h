#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace konflikt {

/// HTTP request information
struct HttpRequest
{
    std::string method;
    std::string path;
    std::string query;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

/// HTTP response
struct HttpResponse
{
    int statusCode { 200 };
    std::string statusMessage { "OK" };
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::string contentType { "text/plain" };
};

/// Route handler callback
using RouteHandler = std::function<HttpResponse(const HttpRequest &request)>;

/// HTTP server using uWebSockets
class HttpServer
{
public:
    explicit HttpServer(int port);
    ~HttpServer();

    // Non-copyable
    HttpServer(const HttpServer &) = delete;
    HttpServer &operator=(const HttpServer &) = delete;

    /// Add a route handler
    void route(const std::string &method, const std::string &path, RouteHandler handler);

    /// Serve static files from a directory
    void serveStatic(const std::string &urlPrefix, const std::string &directory);

    /// Start the server (non-blocking)
    bool start();

    /// Stop the server
    void stop();

    /// Get the actual port
    int port() const { return m_port; }

    /// Check if running
    bool isRunning() const { return m_running; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    int m_port;
    bool m_running { false };
    std::string m_staticDir;
    std::string m_staticPrefix;
};

} // namespace konflikt
