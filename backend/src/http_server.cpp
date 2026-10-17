#include "server/http_server.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

namespace goride {
namespace server {

HttpServer::HttpServer() {
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
}

HttpServer::~HttpServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

void HttpServer::get(const std::string& path, HttpHandler handler) {
    get_routes_[path] = std::move(handler);
}

void HttpServer::post(const std::string& path, HttpHandler handler) {
    post_routes_[path] = std::move(handler);
}

std::string HttpServer::url_decode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%') {
            if (i + 2 < in.size()) {
                int hex_val = 0;
                std::istringstream hex_ss(in.substr(i + 1, 2));
                if (hex_ss >> std::hex >> hex_val) {
                    out.push_back(static_cast<char>(hex_val));
                    i += 2;
                } else {
                    out.push_back('%');
                }
            } else {
                out.push_back('%');
            }
        } else if (in[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(in[i]);
        }
    }
    return out;
}

std::unordered_map<std::string, std::string> HttpServer::parse_query_params(const std::string& qs) {
    std::unordered_map<std::string, std::string> params;
    size_t start = 0;
    while (start < qs.size()) {
        size_t amp = qs.find('&', start);
        if (amp == std::string::npos) amp = qs.size();
        std::string pair = qs.substr(start, amp - start);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string k = url_decode(pair.substr(0, eq));
            std::string v = url_decode(pair.substr(eq + 1));
            params[k] = v;
        } else if (!pair.empty()) {
            params[url_decode(pair)] = "";
        }
        start = amp + 1;
    }
    return params;
}

HttpRequest HttpServer::parse_request(const std::string& raw_http) {
    HttpRequest req;
    std::istringstream ss(raw_http);
    std::string line;

    // 1. Request Line: GET /api/route?a=1 HTTP/1.1
    if (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream line_ss(line);
        std::string raw_url;
        line_ss >> req.method >> raw_url;

        size_t qmark = raw_url.find('?');
        if (qmark != std::string::npos) {
            req.path = raw_url.substr(0, qmark);
            req.query_string = raw_url.substr(qmark + 1);
            req.params = parse_query_params(req.query_string);
        } else {
            req.path = raw_url;
        }
    }

    // 2. Headers
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break; // Header end

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string k = line.substr(0, colon);
            std::string v = line.substr(colon + 1);
            while (!v.empty() && v.front() == ' ') v.erase(v.begin());
            req.headers[k] = v;
        }
    }

    // 3. Body
    std::ostringstream body_ss;
    body_ss << ss.rdbuf();
    req.body = body_ss.str();

    return req;
}

std::string HttpServer::format_response(const HttpResponse& res) {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << res.status_code << " " << res.status_text << "\r\n";
    ss << "Content-Length: " << res.body.size() << "\r\n";
    ss << "Connection: close\r\n";

    // Set CORS headers by default
    ss << "Access-Control-Allow-Origin: *\r\n";
    ss << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    ss << "Access-Control-Allow-Headers: Content-Type, Authorization, Accept, X-Requested-With, Origin\r\n";

    for (const auto& [k, v] : res.headers) {
        if (k != "Content-Length" && k != "Connection" &&
            k != "Access-Control-Allow-Origin" && k != "Access-Control-Allow-Methods" &&
            k != "Access-Control-Allow-Headers") {
            ss << k << ": " << v << "\r\n";
        }
    }
    ss << "\r\n";
    ss << res.body;
    return ss.str();
}

void HttpServer::dispatch(const HttpRequest& req, HttpResponse& res) {
    // 1. CORS Preflight
    if (req.method == "OPTIONS") {
        res.status_code = 204;
        res.status_text = "No Content";
        res.body = "";
        return;
    }

    // 2. GET route
    if (req.method == "GET") {
        auto it = get_routes_.find(req.path);
        if (it != get_routes_.end()) {
            it->second(req, res);
            return;
        }
    }

    // 3. POST route
    if (req.method == "POST") {
        auto it = post_routes_.find(req.path);
        if (it != post_routes_.end()) {
            it->second(req, res);
            return;
        }
    }

    // 4. Not Found
    res.set_json(
        "{\"success\":false,\"error\":\"Endpoint not found: " + req.method + " " + req.path + "\"}",
        404
    );
}

void HttpServer::handle_client(uintptr_t client_sock) {
    SOCKET sock = static_cast<SOCKET>(client_sock);
    std::string raw_buffer;
    raw_buffer.resize(4096);

    int bytes_received = recv(sock, &raw_buffer[0], static_cast<int>(raw_buffer.size() - 1), 0);
    if (bytes_received > 0) {
        raw_buffer.resize(bytes_received);
        HttpRequest req = parse_request(raw_buffer);
        HttpResponse res;

        try {
            dispatch(req, res);
        } catch (const std::exception& ex) {
            res.set_json(
                std::string("{\"success\":false,\"error\":\"Internal Server Error: ") + ex.what() + "\"}",
                500
            );
        } catch (...) {
            res.set_json("{\"success\":false,\"error\":\"Unknown Internal Server Error\"}", 500);
        }

        std::string raw_response = format_response(res);
        send(sock, raw_response.data(), static_cast<int>(raw_response.size()), 0);
    }

    closesocket(sock);
}

bool HttpServer::start(int port, const std::string& host) {
    port_ = port;
    host_ = host;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        std::cerr << "[HttpServer] Failed to create socket." << std::endl;
        return false;
    }

    // Reuse address
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(port_));
    if (host_ == "0.0.0.0" || host_.empty()) {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr);
    }

    if (bind(s, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "[HttpServer] Failed to bind to port " << port_ << "." << std::endl;
        closesocket(s);
        return false;
    }

    if (listen(s, 64) == SOCKET_ERROR) {
        std::cerr << "[HttpServer] Failed to listen on socket." << std::endl;
        closesocket(s);
        return false;
    }

    server_socket_ = static_cast<uintptr_t>(s);
    is_running_.store(true);

    std::cout << "[HttpServer] Server listening on http://" << host_ << ":" << port_ << "/" << std::endl;

    while (is_running_.load()) {
        sockaddr_in client_addr{};
        int client_len = sizeof(client_addr);
        SOCKET client_sock = accept(s, reinterpret_cast<sockaddr*>(&client_addr), &client_len);

        if (client_sock == INVALID_SOCKET) {
            if (!is_running_.load()) break;
            continue;
        }

        // Spawn client thread
        std::thread([this, client_sock]() {
            handle_client(static_cast<uintptr_t>(client_sock));
        }).detach();
    }

    return true;
}

void HttpServer::stop() {
    if (is_running_.load()) {
        is_running_.store(false);
        if (server_socket_ != 0) {
            closesocket(static_cast<SOCKET>(server_socket_));
            server_socket_ = 0;
        }
    }
}

} // namespace server
} // namespace goride
