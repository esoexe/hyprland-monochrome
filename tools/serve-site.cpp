// ═══════════════════════════════════════════════════════════════════════════
//  tools/serve-site.cpp — Zero-Dependency C++20 Live Website HTTP Server
//
//  Serves `site/` on 0.0.0.0:8080 (or --port <N> / PORT env) with:
//    • Host/Origin-agnostic routing for live cloud/browser iframe previews
//    • Virtual mounts for `/wallpapers/*`, `/media/*` (`docs/*`), and
//      `/startpage.html` (`.config/browser/opencode/startpage.html`) so no
//      large binary assets are duplicated in git
//    • Full RFC-7233 `Range: bytes=...` (`206 Partial Content`) support for
//      instant HTML5 `<video>` playback & scrubbing
// ═══════════════════════════════════════════════════════════════════════════

#include <arpa/inet.h>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string mime_for(const fs::path& p) {
    std::string ext = p.extension().string();
    for (char& c : ext) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css")  return "text/css; charset=utf-8";
    if (ext == ".js")   return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".webp") return "image/webp";
    if (ext == ".mp4")  return "video/mp4";
    if (ext == ".webm") return "video/webm";
    if (ext == ".ico")  return "image/x-icon";
    if (ext == ".txt" || ext == ".md" || ext == ".sha256") return "text/plain; charset=utf-8";
    if (ext == ".gz")   return "application/gzip";
    return "application/octet-stream";
}

bool send_all(int fd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

std::string url_decode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            char hex[3] = { in[i + 1], in[i + 2], '\0' };
            char* endp = nullptr;
            long v = std::strtol(hex, &endp, 16);
            if (endp == hex + 2) {
                out.push_back(static_cast<char>(v));
                i += 2;
                continue;
            }
        } else if (in[i] == '+') {
            out.push_back(' ');
            continue;
        }
        out.push_back(in[i]);
    }
    return out;
}

bool is_within(const fs::path& child, const fs::path& base) {
    std::error_code ec;
    fs::path c = fs::weakly_canonical(child, ec);
    if (ec) return false;
    fs::path b = fs::weakly_canonical(base, ec);
    if (ec) return false;
    auto ci = c.begin();
    for (auto bi = b.begin(); bi != b.end(); ++bi, ++ci) {
        if (ci == c.end() || *ci != *bi) return false;
    }
    return true;
}

fs::path resolve_target(const fs::path& repo, const std::string& raw_uri) {
    std::string uri = raw_uri;
    size_t q = uri.find('?');
    if (q != std::string::npos) uri = uri.substr(0, q);
    uri = url_decode(uri);
    if (uri.empty() || uri[0] != '/') return {};

    const fs::path site_dir = repo / "site";
    const fs::path wall_dir = repo / "wallpapers";
    const fs::path docs_dir = repo / "docs";

    if (uri == "/" || uri == "/index.html") {
        return site_dir / "index.html";
    }
    if (uri == "/startpage.html") {
        return repo / ".config/browser/opencode/startpage.html";
    }
    if (uri == "/preview.html") {
        return docs_dir / "preview.html";
    }
    if (uri.rfind("/wallpapers/", 0) == 0) {
        fs::path cand = wall_dir / uri.substr(12);
        if (is_within(cand, wall_dir) && fs::is_regular_file(cand)) return cand;
        return {};
    }
    if (uri.rfind("/media/", 0) == 0) {
        fs::path cand = docs_dir / uri.substr(7);
        if (is_within(cand, docs_dir) && fs::is_regular_file(cand)) return cand;
        return {};
    }

    fs::path cand = site_dir / uri.substr(1);
    if (fs::is_directory(cand)) cand /= "index.html";
    if (is_within(cand, site_dir) && fs::is_regular_file(cand)) return cand;
    return {};
}

void handle_client(int fd, fs::path repo) {
    struct Guard {
        int s;
        ~Guard() { ::close(s); }
    } guard{fd};

    std::string req;
    char buf[4096];
    while (req.find("\r\n\r\n") == std::string::npos && req.size() < 32768) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) return;
        req.append(buf, static_cast<size_t>(n));
    }

    std::istringstream iss(req);
    std::string method, uri, version;
    if (!(iss >> method >> uri >> version)) return;

    if (method == "OPTIONS") {
        const std::string hdr =
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, HEAD, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Range, Content-Type\r\n"
            "Connection: close\r\n\r\n";
        send_all(fd, hdr.data(), hdr.size());
        return;
    }

    if (method != "GET" && method != "HEAD") {
        const std::string hdr =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        send_all(fd, hdr.data(), hdr.size());
        return;
    }

    fs::path file = resolve_target(repo, uri);
    std::error_code ec;
    if (file.empty() || !fs::exists(file, ec)) {
        const std::string body = "404 Not Found\n";
        std::ostringstream res;
        res << "HTTP/1.1 404 Not Found\r\n"
            << "Content-Type: text/plain; charset=utf-8\r\n"
            << "Access-Control-Allow-Origin: *\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n\r\n";
        std::string h = res.str();
        send_all(fd, h.data(), h.size());
        if (method == "GET") send_all(fd, body.data(), body.size());
        return;
    }

    const uintmax_t total = fs::file_size(file, ec);
    if (ec) return;

    // Parse optional Range: bytes=start-end header for video streaming
    uintmax_t start = 0;
    uintmax_t end = total > 0 ? total - 1 : 0;
    bool partial = false;

    std::string line;
    std::getline(iss, line);
    while (std::getline(iss, line) && line != "\r" && !line.empty()) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string low = line;
        for (char& c : low) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        if (low.rfind("range: bytes=", 0) == 0 && total > 0) {
            std::string spec = low.substr(13);
            size_t dash = spec.find('-');
            if (dash != std::string::npos) {
                std::string s_str = spec.substr(0, dash);
                std::string e_str = spec.substr(dash + 1);
                if (!s_str.empty()) {
                    start = static_cast<uintmax_t>(std::strtoull(s_str.c_str(), nullptr, 10));
                }
                if (!e_str.empty()) {
                    end = static_cast<uintmax_t>(std::strtoull(e_str.c_str(), nullptr, 10));
                }
                if (start < total) {
                    if (end >= total) end = total - 1;
                    if (start <= end) partial = true;
                }
            }
        }
    }

    const uintmax_t length = (total == 0) ? 0 : (end - start + 1);
    std::ostringstream hdr;
    hdr << (partial ? "HTTP/1.1 206 Partial Content\r\n" : "HTTP/1.1 200 OK\r\n")
        << "Content-Type: " << mime_for(file) << "\r\n"
        << "Accept-Ranges: bytes\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Cache-Control: no-cache\r\n";
    if (partial) {
        hdr << "Content-Range: bytes " << start << "-" << end << "/" << total << "\r\n";
    }
    hdr << "Content-Length: " << length << "\r\n"
        << "Connection: close\r\n\r\n";

    std::string h = hdr.str();
    if (!send_all(fd, h.data(), h.size())) return;
    if (method == "HEAD" || length == 0) return;

    std::ifstream in(file, std::ios::binary);
    if (!in) return;
    in.seekg(static_cast<std::streamoff>(start));

    std::vector<char> chunk(65536);
    uintmax_t rem = length;
    while (rem > 0 && in) {
        size_t want = rem < chunk.size() ? static_cast<size_t>(rem) : chunk.size();
        in.read(chunk.data(), static_cast<std::streamsize>(want));
        std::streamsize got = in.gcount();
        if (got <= 0) break;
        if (!send_all(fd, chunk.data(), static_cast<size_t>(got))) break;
        rem -= static_cast<uintmax_t>(got);
    }
}

} // namespace

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);

    int port = 8080;
    if (const char* env_p = std::getenv("PORT")) {
        int p = std::atoi(env_p);
        if (p > 0 && p < 65536) port = p;
    }
    fs::path repo = fs::current_path();

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--port" || a == "-p") && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if ((a == "--repo" || a == "-r") && i + 1 < argc) {
            repo = argv[++i];
        }
    }

    int srv = ::socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        std::perror("socket");
        return 1;
    }
    int yes = 1;
    ::setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0 for live browser preview
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        ::close(srv);
        return 1;
    }
    if (::listen(srv, 64) < 0) {
        std::perror("listen");
        ::close(srv);
        return 1;
    }

    std::cout << "hyprland-monochrome live site listening on http://0.0.0.0:" << port
              << " (root: " << (repo / "site").string() << ")" << std::endl;

    while (true) {
        sockaddr_in cli{};
        socklen_t clen = sizeof(cli);
        int cfd = ::accept(srv, reinterpret_cast<sockaddr*>(&cli), &clen);
        if (cfd < 0) continue;
        std::thread(handle_client, cfd, repo).detach();
    }
}
