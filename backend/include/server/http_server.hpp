#ifndef GORIDE_SERVER_HTTP_SERVER_HPP
#define GORIDE_SERVER_HTTP_SERVER_HPP

#include <atomic>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace goride {
namespace server {

struct HttpRequest {
    std::string method;                                  // "GET", "POST", "OPTIONS", etc.
    std::string path;                                    // "/api/route"
    std::string query_string;                            // "from_lat=...&to_lat=..."
    std::unordered_map<std::string, std::string> params; // Parsed query parameters
    std::unordered_map<std::string, std::string> headers;// Request headers
    std::string body;

    bool has_param(const std::string& key) const {
        return params.find(key) != params.end();
    }

    std::string get_param(const std::string& key, const std::string& default_val = "") const {
        auto it = params.find(key);
        if (it != params.end()) return it->second;
        return default_val;
    }

    std::string get_header(const std::string& key, const std::string& default_val = "") const {
        for (const auto& [k, v] : headers) {
            if (k.size() == key.size()) {
                bool match = true;
                for (size_t i = 0; i < k.size(); ++i) {
                    if (std::tolower(static_cast<unsigned char>(k[i])) !=
                        std::tolower(static_cast<unsigned char>(key[i]))) {
                        match = false;
                        break;
                    }
                }
                if (match) return v;
            }
        }
        return default_val;
    }

    std::string get_bearer_token() const {
        std::string auth = get_header("Authorization");
        if (auth.rfind("Bearer ", 0) == 0) {
            return auth.substr(7);
        }
        return "";
    }
};

struct HttpResponse {
    int status_code{200};
    std::string status_text{"OK"};
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    void set_header(const std::string& key, const std::string& val) {
        headers[key] = val;
    }

    void set_json(const std::string& json_str, int code = 200) {
        status_code = code;
        if (code == 200) status_text = "OK";
        else if (code == 201) status_text = "Created";
        else if (code == 204) status_text = "No Content";
        else if (code == 400) status_text = "Bad Request";
        else if (code == 401) status_text = "Unauthorized";
        else if (code == 403) status_text = "Forbidden";
        else if (code == 404) status_text = "Not Found";
        else if (code == 409) status_text = "Conflict";
        else if (code == 422) status_text = "Unprocessable Entity";
        else if (code == 500) status_text = "Internal Server Error";
        else if (code == 503) status_text = "Service Unavailable";
        else status_text = "Response";

        headers["Content-Type"] = "application/json; charset=utf-8";
        body = json_str;
    }
};

using HttpHandler = std::function<void(const HttpRequest& req, HttpResponse& res)>;

class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    // Register route handlers
    void get(const std::string& path, HttpHandler handler);
    void post(const std::string& path, HttpHandler handler);

    // Start listening on port (blocking or non-blocking)
    bool start(int port = 8080, const std::string& host = "0.0.0.0");
    void stop();

    bool is_running() const { return is_running_.load(); }
    int port() const { return port_; }

    // Utility: Parse raw HTTP request text
    static HttpRequest parse_request(const std::string& raw_http);

    // Utility: Format response to raw HTTP string
    static std::string format_response(const HttpResponse& res);

    // Utility: Parse query string e.g. "a=1&b=hello%20world"
    static std::unordered_map<std::string, std::string> parse_query_params(const std::string& qs);

    // Utility: URL-decode string
    static std::string url_decode(const std::string& in);

    // Dispatch request to registered handlers (used by client handler and unit tests)
    void dispatch(const HttpRequest& req, HttpResponse& res);

private:
    int port_{8080};
    std::string host_{"0.0.0.0"};
    std::atomic<bool> is_running_{false};
    uintptr_t server_socket_{0}; // Socket handle (SOCKET on Windows)

    std::unordered_map<std::string, HttpHandler> get_routes_;
    std::unordered_map<std::string, HttpHandler> post_routes_;

    void handle_client(uintptr_t client_sock);
};

} // namespace server
} // namespace goride

#endif // GORIDE_SERVER_HTTP_SERVER_HPP
