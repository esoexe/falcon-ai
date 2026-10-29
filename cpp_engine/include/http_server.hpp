#pragma once
/**
 * OpenMesh Studio — Multi-Threaded POSIX Socket C++20 HTTP/1.1 Server
 * with Multipart Form-Data Binary Upload Parser, CORS, and Static File Serving.
 */

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <atomic>
#include <csignal>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace openmesh {

#ifdef _WIN32
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
inline void close_socket_fd(socket_t s) { if (s != INVALID_SOCKET) ::closesocket(s); }
#else
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
inline void close_socket_fd(socket_t s) { if (s >= 0) ::close(s); }
#endif

struct MultipartPart {
    std::string name;
    std::string filename;
    std::string content_type;
    std::vector<uint8_t> data;

    std::string as_string() const {
        return std::string(reinterpret_cast<const char*>(data.data()), data.size());
    }
};

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    std::string body_str() const {
        return std::string(reinterpret_cast<const char*>(body.data()), body.size());
    }

    std::map<std::string, MultipartPart> parse_multipart() const {
        std::map<std::string, MultipartPart> parts;
        auto it = headers.find("content-type");
        if (it == headers.end()) return parts;
        const std::string& ct = it->second;
        auto bpos = ct.find("boundary=");
        if (bpos == std::string::npos) return parts;
        std::string boundary = ct.substr(bpos + 9);
        if (!boundary.empty() && boundary.front() == '"') {
            boundary = boundary.substr(1, boundary.find('"', 1) - 1);
        } else {
            auto sc = boundary.find(';');
            if (sc != std::string::npos) boundary = boundary.substr(0, sc);
        }
        std::string delim = "--" + boundary;
        std::string_view raw(reinterpret_cast<const char*>(body.data()), body.size());

        size_t pos = raw.find(delim);
        while (pos != std::string_view::npos) {
            pos += delim.size();
            if (pos + 2 <= raw.size() && raw.substr(pos, 2) == "--") break;
            if (pos + 2 <= raw.size() && raw.substr(pos, 2) == "\r\n") pos += 2;

            size_t next_delim = raw.find(delim, pos);
            if (next_delim == std::string_view::npos) break;

            size_t hdr_end = raw.find("\r\n\r\n", pos);
            if (hdr_end != std::string_view::npos && hdr_end < next_delim) {
                std::string_view hdr_block = raw.substr(pos, hdr_end - pos);
                size_t data_start = hdr_end + 4;
                size_t data_end = next_delim;
                if (data_end >= 2 && raw.substr(data_end - 2, 2) == "\r\n") data_end -= 2;

                MultipartPart mp;
                auto extract_attr = [&](const char* key) -> std::string {
                    auto kp = hdr_block.find(key);
                    if (kp == std::string_view::npos) return "";
                    kp += std::strlen(key);
                    auto eq = hdr_block.find('"', kp);
                    if (eq == std::string_view::npos) return "";
                    return std::string(hdr_block.substr(kp, eq - kp));
                };
                mp.name = extract_attr("name=\"");
                mp.filename = extract_attr("filename=\"");
                if (data_end >= data_start) {
                    mp.data.assign(body.begin() + static_cast<ptrdiff_t>(data_start),
                                   body.begin() + static_cast<ptrdiff_t>(data_end));
                }
                if (!mp.name.empty()) {
                    parts[mp.name] = std::move(mp);
                }
            }
            pos = next_delim;
        }
        return parts;
    }
};

struct HttpResponse {
    int status = 200;
    std::string content_type = "application/json; charset=utf-8";
    std::vector<std::pair<std::string, std::string>> extra_headers;
    std::vector<uint8_t> body;

    static HttpResponse json(const std::string& j, int code = 200) {
        HttpResponse r;
        r.status = code;
        r.content_type = "application/json; charset=utf-8";
        r.body.assign(j.begin(), j.end());
        return r;
    }

    static HttpResponse bytes(std::vector<uint8_t> b, const std::string& mime, const std::string& download_name = "") {
        HttpResponse r;
        r.status = 200;
        r.content_type = mime;
        if (!download_name.empty()) {
            r.extra_headers.emplace_back("Content-Disposition", "attachment; filename=\"" + download_name + "\"");
        }
        r.body = std::move(b);
        return r;
    }
};

class HttpServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    explicit HttpServer(Handler handler) : handler_(std::move(handler)) {}

    void listen_forever(const std::string& host, uint16_t port) {
#ifdef _WIN32
        WSADATA wsa{};
        if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            throw std::runtime_error("WSAStartup() failed");
        }
#else
        std::signal(SIGPIPE, SIG_IGN);
#endif

        socket_t server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == kInvalidSocket) throw std::runtime_error("socket() failed");

        int opt = 1;
        ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = (host == "0.0.0.0") ? INADDR_ANY : ::inet_addr(host.c_str());

        if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            close_socket_fd(server_fd);
            throw std::runtime_error("bind() failed on port " + std::to_string(port));
        }
        if (::listen(server_fd, 128) != 0) {
            close_socket_fd(server_fd);
            throw std::runtime_error("listen() failed");
        }

        std::cout << "[Falcon AI C++20 Server] Listening on http://" << host << ":" << port << std::endl;

        while (true) {
            sockaddr_in client_addr{};
            socklen_t clen = sizeof(client_addr);
            socket_t cfd = ::accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &clen);
            if (cfd == kInvalidSocket) continue;

            std::thread([this, cfd]() {
                this->handle_client(cfd);
            }).detach();
        }
    }

private:
    Handler handler_;

    static std::string status_reason(int code) {
        switch (code) {
            case 200: return "OK";
            case 204: return "No Content";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            default: return "OK";
        }
    }

    static bool send_all(socket_t fd, const uint8_t* buf, size_t len) {
        size_t sent = 0;
        while (sent < len) {
            size_t chunk_sz = std::min<size_t>(len - sent, 1 << 20);
#ifdef _WIN32
            int n = ::send(fd, reinterpret_cast<const char*>(buf + sent), static_cast<int>(chunk_sz), 0);
#else
            ssize_t n = ::send(fd, buf + sent, chunk_sz, MSG_NOSIGNAL);
#endif
            if (n <= 0) return false;
            sent += static_cast<size_t>(n);
        }
        return true;
    }

    void handle_client(socket_t fd) {
#ifdef _WIN32
        DWORD tv_ms = 30000;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv_ms), sizeof(tv_ms));
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv_ms), sizeof(tv_ms));
#else
        timeval tv{};
        tv.tv_sec = 30;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

        std::vector<uint8_t> raw;
        raw.reserve(8192);
        uint8_t chunk[8192];
        size_t hdr_end = std::string::npos;
        size_t content_len = 0;

        while (true) {
#ifdef _WIN32
            int n = ::recv(fd, reinterpret_cast<char*>(chunk), static_cast<int>(sizeof(chunk)), 0);
#else
            ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
#endif
            if (n <= 0) break;
            raw.insert(raw.end(), chunk, chunk + n);

            if (hdr_end == std::string::npos) {
                std::string_view sv(reinterpret_cast<const char*>(raw.data()), raw.size());
                size_t p = sv.find("\r\n\r\n");
                if (p != std::string_view::npos) {
                    hdr_end = p + 4;
                    // Parse Content-Length
                    std::string hdr_str(sv.substr(0, p));
                    std::istringstream iss(hdr_str);
                    std::string line;
                    std::getline(iss, line);
                    while (std::getline(iss, line)) {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        auto col = line.find(':');
                        if (col != std::string::npos) {
                            std::string k = line.substr(0, col);
                            for (char& c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                            if (k == "content-length") {
                                try { content_len = std::stoull(line.substr(col + 1)); } catch (...) {}
                            }
                        }
                    }
                    if (content_len > 32 * 1024 * 1024) { close_socket_fd(fd); return; }
                }
            }
            if (hdr_end != std::string::npos && raw.size() >= hdr_end + content_len) {
                break;
            }
        }

        if (hdr_end == std::string::npos) {
            close_socket_fd(fd);
            return;
        }

        HttpRequest req;
        std::string hdr_part(reinterpret_cast<const char*>(raw.data()), hdr_end - 4);
        std::istringstream iss(hdr_part);
        std::string req_line;
        std::getline(iss, req_line);
        if (!req_line.empty() && req_line.back() == '\r') req_line.pop_back();

        {
            std::istringstream rls(req_line);
            std::string full_uri;
            rls >> req.method >> full_uri;
            auto qpos = full_uri.find('?');
            if (qpos != std::string::npos) {
                req.path = full_uri.substr(0, qpos);
                req.query = full_uri.substr(qpos + 1);
            } else {
                req.path = full_uri;
            }
        }

        std::string hline;
        while (std::getline(iss, hline)) {
            if (!hline.empty() && hline.back() == '\r') hline.pop_back();
            auto col = hline.find(':');
            if (col != std::string::npos) {
                std::string k = hline.substr(0, col);
                for (char& c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                std::string v = hline.substr(col + 1);
                while (!v.empty() && v.front() == ' ') v.erase(0, 1);
                req.headers[k] = v;
            }
        }

        if (content_len > 0 && raw.size() >= hdr_end + content_len) {
            req.body.assign(raw.begin() + static_cast<ptrdiff_t>(hdr_end),
                            raw.begin() + static_cast<ptrdiff_t>(hdr_end + content_len));
        }

        HttpResponse resp;
        if (req.method == "OPTIONS") {
            resp.status = 204;
            resp.content_type = "text/plain";
        } else {
            try {
                resp = handler_(req);
            } catch (const std::exception& ex) {
                std::ostringstream err;
                err << "{\"error\":" << JsonValue::escape_str(ex.what()) << "}";
                resp = HttpResponse::json(err.str(), 400);
            }
        }

        std::ostringstream h_out;
        h_out << "HTTP/1.1 " << resp.status << " " << status_reason(resp.status) << "\r\n"
              << "Content-Type: " << resp.content_type << "\r\n"
              << "Content-Length: " << resp.body.size() << "\r\n"
              << "Access-Control-Allow-Origin: *\r\n"
              << "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
              << "Access-Control-Allow-Headers: *\r\n"
              << "Connection: close\r\n";
        for (const auto& kv : resp.extra_headers) {
            h_out << kv.first << ": " << kv.second << "\r\n";
        }
        h_out << "\r\n";

        std::string h_bytes = h_out.str();
        send_all(fd, reinterpret_cast<const uint8_t*>(h_bytes.data()), h_bytes.size());
        if (!resp.body.empty()) {
            send_all(fd, resp.body.data(), resp.body.size());
        }
        close_socket_fd(fd);
    }
};

} // namespace openmesh
