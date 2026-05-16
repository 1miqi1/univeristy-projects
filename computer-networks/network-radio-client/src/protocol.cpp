#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "protocol.hpp"
#include "exceptions.hpp" // Added our new exceptions header

namespace{
    static bool iequals_prefix(const std::string &line, const char *prefix) {
    size_t n = strlen(prefix);
    if (line.size() < n) return false;

    for (size_t i = 0; i < n; i++) {
        char a = line[i];
        char b = prefix[i];

        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;

        if (a != b) return false;
    }
    return true;
}
}

void init_http_response(HttpResponse& resp) {
    resp.status_code = 0;
    resp.location.clear();
    resp.icy_metaint = 0;
    resp.is_chunked = false;
    resp.cookie.clear();
}

size_t create_http_request(char *request,
                           const std::string& host,
                           const std::string& path,
                           bool request_meta,
                           const std::string& cookie) {
    // Use snprintf to prevent buffer overflows. 
    // Assuming request buffer is at least MAX_BUFFER_SIZE.
    int written = sprintf(request,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: Keep-Alive\r\n",
        path.c_str(), host.c_str());

    char* ptr = request + written;

    if (request_meta) {
        ptr += sprintf(ptr, "Icy-MetaData: 1\r\n");
    }

    if (!cookie.empty()) {
        ptr += sprintf(ptr, "Cookie: %s\r\n", cookie.c_str());
    }

    ptr += sprintf(ptr, "\r\n");

    // Return the total length written
    return (size_t)(ptr - request);
}

#include <string>
#include <cstring> // for strncasecmp
#include <algorithm>

void parse_http_response_line(const std::string &line,
                              int line_count,
                              HttpResponse &resp) {

    // -------------------------
    // Status line (first line)
    // -------------------------
    if (line_count == 0) {
        int code = 0;

        // Try HTTP/1.x format
        if (sscanf(line.c_str(), "HTTP/1.%*d %d", &code) == 1) {
            resp.status_code = code;
            return;
        }

        // ICY fallback (streaming protocols)
        if (line.rfind("ICY 200", 0) == 0) {
            resp.status_code = 200;
            return;
        }

        throw ProtocolError("Invalid or unsupported protocol header");
    }

    // -------------------------
    // Headers
    // -------------------------

    auto trim_left = [](const std::string &s, size_t pos) {
        while (pos < s.size() && s[pos] == ' ') pos++;
        return pos;
    };

    const std::string &l = line;

    // Location
    if (iequals_prefix(l, "Location:")) {
        size_t pos = trim_left(l, 9);
        resp.location = l.substr(pos);
        return;
    }

    // Set-Cookie
    if (iequals_prefix(l, "Set-Cookie:")) {
        size_t pos = trim_left(l, 11);
        std::string cookie = l.substr(pos);

        // Optional: strip attributes after ';'
        size_t semi = cookie.find(';');
        if (semi != std::string::npos)
            cookie = cookie.substr(0, semi);

        resp.cookie = cookie;
        return;
    }

    // icy-metaint
    if (iequals_prefix(l, "icy-metaint:")) {
        size_t pos = trim_left(l, 12);
        try {
            resp.icy_metaint = std::stoi(l.substr(pos));
        } catch (...) {
            throw ProtocolError("Invalid icy-metaint value");
        }
        return;
    }

    // Transfer-Encoding
    if (iequals_prefix(l, "Transfer-Encoding:")) {
        if (l.find("chunked") != std::string::npos) {
            resp.is_chunked = true;
        }
        return;
    }
}