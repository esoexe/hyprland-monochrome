// ═══════════════════════════════════════════════════════════════════════════
//  tools/serve-site.c — Zero-Dependency Pure C11 Live Website HTTP Server
//
//  Serves `site/` on 0.0.0.0:8080 (or --port <N> / PORT env) with:
//    - Host/Origin-agnostic routing for live cloud/browser iframe previews
//    - Virtual mounts for `/wallpapers/`, `/media/` (`docs/`), and
//      `/startpage.html` (`site/newtab.html`) so no large binary assets are
//      duplicated in git
//    - Full RFC-7233 `Range: bytes=...` (`206 Partial Content`) support for
//      instant HTML5 `<video>` playback & scrubbing
// ═══════════════════════════════════════════════════════════════════════════

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <arpa/inet.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    int fd;
    char repo[PATH_MAX];
} ClientJob;

static const char *mime_for(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    char ext[32];
    size_t i = 0;
    while (dot[i] != '\0' && i + 1 < sizeof(ext)) {
        ext[i] = (char)tolower((unsigned char)dot[i]);
        ++i;
    }
    ext[i] = '\0';

    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0)  return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0)   return "application/javascript; charset=utf-8";
    if (strcmp(ext, ".json") == 0) return "application/json; charset=utf-8";
    if (strcmp(ext, ".svg") == 0)  return "image/svg+xml";
    if (strcmp(ext, ".png") == 0)  return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0)  return "image/gif";
    if (strcmp(ext, ".webp") == 0) return "image/webp";
    if (strcmp(ext, ".mp4") == 0)  return "video/mp4";
    if (strcmp(ext, ".webm") == 0) return "video/webm";
    if (strcmp(ext, ".ico") == 0)  return "image/x-icon";
    if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".md") == 0 || strcmp(ext, ".sha256") == 0) {
        return "text/plain; charset=utf-8";
    }
    if (strcmp(ext, ".gz") == 0)   return "application/gzip";
    return "application/octet-stream";
}

static bool send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

static void url_decode(const char *in, char *out, size_t out_sz) {
    size_t wp = 0;
    for (size_t i = 0; in[i] != '\0' && wp + 1 < out_sz; ++i) {
        if (in[i] == '%' && isxdigit((unsigned char)in[i + 1]) && isxdigit((unsigned char)in[i + 2])) {
            char hex[3] = { in[i + 1], in[i + 2], '\0' };
            out[wp++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (in[i] == '+') {
            out[wp++] = ' ';
        } else {
            out[wp++] = in[i];
        }
    }
    out[wp] = '\0';
}

static bool is_within_regular(const char *cand, const char *base_dir, char *resolved_out) {
    char real_cand[PATH_MAX];
    char real_base[PATH_MAX];
    if (!realpath(cand, real_cand)) return false;
    if (!realpath(base_dir, real_base)) return false;

    size_t blen = strlen(real_base);
    if (strncmp(real_cand, real_base, blen) != 0) return false;
    if (real_cand[blen] != '/' && real_cand[blen] != '\0') return false;

    struct stat st;
    if (stat(real_cand, &st) != 0 || !S_ISREG(st.st_mode)) return false;
    snprintf(resolved_out, PATH_MAX, "%s", real_cand);
    return true;
}

static bool path_join(char *dst, size_t cap, const char *a, const char *b) {
    int n = snprintf(dst, cap, "%s/%s", a, b);
    return n > 0 && (size_t)n < cap;
}

static bool resolve_target(const char *repo, const char *raw_uri, char *out_path) {
    char trimmed[2048];
    size_t k = 0;
    while (raw_uri[k] != '\0' && raw_uri[k] != '?' && k + 1 < sizeof(trimmed)) {
        trimmed[k] = raw_uri[k];
        ++k;
    }
    trimmed[k] = '\0';

    char uri[2048];
    url_decode(trimmed, uri, sizeof(uri));
    if (uri[0] != '/') return false;

    char site_dir[PATH_MAX], wall_dir[PATH_MAX], docs_dir[PATH_MAX], cand[PATH_MAX];
    if (!path_join(site_dir, sizeof(site_dir), repo, "site") ||
        !path_join(wall_dir, sizeof(wall_dir), repo, "wallpapers") ||
        !path_join(docs_dir, sizeof(docs_dir), repo, "docs")) {
        return false;
    }

    if (strcmp(uri, "/") == 0 || strcmp(uri, "/index.html") == 0) {
        return path_join(cand, sizeof(cand), site_dir, "index.html") &&
               is_within_regular(cand, site_dir, out_path);
    }
    if (strcmp(uri, "/startpage.html") == 0 || strcmp(uri, "/newtab.html") == 0) {
        return path_join(cand, sizeof(cand), site_dir, "newtab.html") &&
               is_within_regular(cand, site_dir, out_path);
    }
    if (strcmp(uri, "/preview.html") == 0) {
        return path_join(cand, sizeof(cand), docs_dir, "preview.html") &&
               is_within_regular(cand, docs_dir, out_path);
    }
    if (strncmp(uri, "/wallpapers/", 12) == 0) {
        return path_join(cand, sizeof(cand), wall_dir, uri + 12) &&
               is_within_regular(cand, wall_dir, out_path);
    }
    if (strncmp(uri, "/media/", 7) == 0) {
        return path_join(cand, sizeof(cand), docs_dir, uri + 7) &&
               is_within_regular(cand, docs_dir, out_path);
    }

    if (!path_join(cand, sizeof(cand), site_dir, uri + 1)) return false;
    struct stat st;
    if (stat(cand, &st) == 0 && S_ISDIR(st.st_mode)) {
        char idx[PATH_MAX];
        return path_join(idx, sizeof(idx), cand, "index.html") &&
               is_within_regular(idx, site_dir, out_path);
    }
    return is_within_regular(cand, site_dir, out_path);
}

static bool starts_with_ci(const char *s, const char *prefix) {
    for (size_t i = 0; prefix[i] != '\0'; ++i) {
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)prefix[i])) return false;
    }
    return true;
}

static void *client_worker(void *arg) {
    ClientJob *job = (ClientJob *)arg;
    int fd = job->fd;
    char repo[PATH_MAX];
    snprintf(repo, sizeof(repo), "%s", job->repo);
    free(job);

    char req[32768];
    size_t req_len = 0;
    req[0] = '\0';

    while (strstr(req, "\r\n\r\n") == NULL && req_len + 1 < sizeof(req)) {
        ssize_t n = recv(fd, req + req_len, sizeof(req) - 1 - req_len, 0);
        if (n <= 0) {
            close(fd);
            return NULL;
        }
        req_len += (size_t)n;
        req[req_len] = '\0';
    }

    char method[16] = {0}, uri[2048] = {0}, version[32] = {0};
    if (sscanf(req, "%15s %2047s %31s", method, uri, version) != 3) {
        close(fd);
        return NULL;
    }

    if (strcmp(method, "OPTIONS") == 0) {
        const char *hdr =
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, HEAD, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Range, Content-Type\r\n"
            "Connection: close\r\n\r\n";
        send_all(fd, hdr, strlen(hdr));
        close(fd);
        return NULL;
    }

    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
        const char *hdr =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        send_all(fd, hdr, strlen(hdr));
        close(fd);
        return NULL;
    }

    char file_path[PATH_MAX];
    struct stat st;
    if (!resolve_target(repo, uri, file_path) || stat(file_path, &st) != 0) {
        const char *body = "404 Not Found\n";
        char hdr[256];
        int hlen = snprintf(hdr, sizeof(hdr),
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n", strlen(body));
        send_all(fd, hdr, (size_t)hlen);
        if (strcmp(method, "GET") == 0) send_all(fd, body, strlen(body));
        close(fd);
        return NULL;
    }

    uint64_t total = (uint64_t)st.st_size;
    uint64_t start = 0;
    uint64_t end = (total > 0) ? (total - 1) : 0;
    bool partial = false;

    const char *line = strchr(req, '\n');
    while (line && *line == '\n') {
        line++;
        const char *eol = strchr(line, '\n');
        size_t llen = eol ? (size_t)(eol - line) : strlen(line);
        if (llen > 0 && line[llen - 1] == '\r') llen--;
        if (llen == 0) break;

        if (total > 0 && llen > 13 && starts_with_ci(line, "range: bytes=")) {
            char spec[128];
            size_t slen = llen - 13;
            if (slen >= sizeof(spec)) slen = sizeof(spec) - 1;
            memcpy(spec, line + 13, slen);
            spec[slen] = '\0';
            char *dash = strchr(spec, '-');
            if (dash) {
                *dash = '\0';
                const char *s_str = spec;
                const char *e_str = dash + 1;
                if (*s_str != '\0') start = strtoull(s_str, NULL, 10);
                if (*e_str != '\0') end = strtoull(e_str, NULL, 10);
                if (start < total) {
                    if (end >= total) end = total - 1;
                    if (start <= end) partial = true;
                }
            }
        }
        line = eol;
    }

    uint64_t length = (total == 0) ? 0 : (end - start + 1);
    char hdr[1024];
    int hlen = 0;
    if (partial) {
        hlen = snprintf(hdr, sizeof(hdr),
            "HTTP/1.1 206 Partial Content\r\n"
            "Content-Type: %s\r\n"
            "Accept-Ranges: bytes\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Cache-Control: no-cache\r\n"
            "Content-Range: bytes %" PRIu64 "-%" PRIu64 "/%" PRIu64 "\r\n"
            "Content-Length: %" PRIu64 "\r\n"
            "Connection: close\r\n\r\n",
            mime_for(file_path), start, end, total, length);
    } else {
        hlen = snprintf(hdr, sizeof(hdr),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Accept-Ranges: bytes\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Cache-Control: no-cache\r\n"
            "Content-Length: %" PRIu64 "\r\n"
            "Connection: close\r\n\r\n",
            mime_for(file_path), length);
    }

    if (!send_all(fd, hdr, (size_t)hlen) || strcmp(method, "HEAD") == 0 || length == 0) {
        close(fd);
        return NULL;
    }

    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        close(fd);
        return NULL;
    }
    if (fseeko(fp, (off_t)start, SEEK_SET) != 0) {
        fclose(fp);
        close(fd);
        return NULL;
    }

    char *chunk = (char *)malloc(65536);
    if (chunk) {
        uint64_t rem = length;
        while (rem > 0) {
            size_t want = (rem < 65536ULL) ? (size_t)rem : 65536U;
            size_t got = fread(chunk, 1, want, fp);
            if (got == 0) break;
            if (!send_all(fd, chunk, got)) break;
            rem -= (uint64_t)got;
        }
        free(chunk);
    }
    fclose(fp);
    close(fd);
    return NULL;
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);

    int port = 8080;
    const char *env_p = getenv("PORT");
    if (env_p) {
        int p = atoi(env_p);
        if (p > 0 && p < 65536) port = p;
    }

    char repo[PATH_MAX];
    if (!getcwd(repo, sizeof(repo))) {
        snprintf(repo, sizeof(repo), ".");
    }

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--repo") == 0 || strcmp(argv[i], "-r") == 0) && i + 1 < argc) {
            snprintf(repo, sizeof(repo), "%s", argv[++i]);
        }
    }

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("socket");
        return 1;
    }
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(srv);
        return 1;
    }
    if (listen(srv, 64) < 0) {
        perror("listen");
        close(srv);
        return 1;
    }

    printf("hyprland-monochrome live site listening on http://0.0.0.0:%d (root: %s/site)\n", port, repo);
    fflush(stdout);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        int cfd = accept(srv, (struct sockaddr *)&cli, &clen);
        if (cfd < 0) continue;

        ClientJob *job = (ClientJob *)malloc(sizeof(ClientJob));
        if (!job) {
            close(cfd);
            continue;
        }
        job->fd = cfd;
        snprintf(job->repo, sizeof(job->repo), "%s", repo);

        pthread_t tid;
        if (pthread_create(&tid, &attr, client_worker, job) != 0) {
            close(cfd);
            free(job);
        }
    }
}
