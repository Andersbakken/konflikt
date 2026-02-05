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
    int port() const { return mPort; }

    /// Check if running
    bool isRunning() const { return mRunning; }

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;

    int mPort;
    bool mRunning { false };
    std::string mStaticDir;
    std::string mStaticPrefix;
};

} // namespace konflikt
