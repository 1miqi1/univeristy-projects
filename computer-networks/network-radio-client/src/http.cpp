#include "http.hpp"
#include "exceptions.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>

namespace {

// HTTP Header Constants
constexpr std::string_view HEADER_LOCATION     = "Location:";
constexpr std::string_view HEADER_SET_COOKIE   = "Set-Cookie:";
constexpr std::string_view HEADER_ICY_METAINT  = "icy-metaint:";
constexpr std::string_view HEADER_TRANSFER_ENC = "Transfer-Encoding:";
constexpr std::string_view CHUNKED_ENC_VAL     = "chunked";
constexpr std::string_view ICY_200_OK_STATUS   = "ICY 200";




/**
 * @brief Helper to compare a string prefix case-insensitively.
 */
static bool iequals_prefix(const std::string &line, std::string_view prefix) {
    if (line.size() < prefix.size()) return false;

    for (size_t i = 0; i < prefix.size(); i++) {
        char a = line[i];
        char b = prefix[i];

        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;

        if (a != b) return false;
    }
    return true;
}

} // anonymous namespace

void merge_cookie(std::vector<HttpCookie>& client_cookies, const std::string& set_cookie_header, const std::string& current_host) {
    if (set_cookie_header.empty()) return;

    // Helper to trim whitespace from ends of strings
    auto trim = [](std::string s) {
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        size_t last = s.find_last_not_of(" \t\r\n");
        if (last != std::string::npos) s.erase(last + 1);
        return s;
    };

    // Helper to convert to lowercase for case-insensitive attribute checking
    auto to_lower = [](std::string s) {
        for (char &c : s) {
            if (c >= 'A' && c <= 'Z') c += 32;
        }
        return s;
    };

    // 1. Extract the primary key=value pair (before the first ';')
    size_t semi_pos = set_cookie_header.find(';');
    std::string kv_part = (semi_pos == std::string::npos) ? set_cookie_header : set_cookie_header.substr(0, semi_pos);
    
    size_t eq_pos = kv_part.find('=');
    if (eq_pos == std::string::npos) return; // Invalid cookie format

    std::string new_key = trim(kv_part.substr(0, eq_pos));
    std::string new_value = trim(kv_part.substr(eq_pos + 1));

    // Default domain and path
    std::string cookie_domain = current_host; 
    std::string cookie_path = "/";

    // 2. Parse remaining attributes (Domain, Path, etc.)
    if (semi_pos != std::string::npos) {
        size_t start = semi_pos + 1;
        while (start < set_cookie_header.length()) {
            size_t end = set_cookie_header.find(';', start);
            std::string attr = (end == std::string::npos) 
                               ? set_cookie_header.substr(start) 
                               : set_cookie_header.substr(start, end - start);
            
            attr = trim(attr);
            if (!attr.empty()) {
                size_t attr_eq = attr.find('=');
                std::string attr_name, attr_val;
                
                if (attr_eq != std::string::npos) {
                    attr_name = to_lower(trim(attr.substr(0, attr_eq)));
                    attr_val = trim(attr.substr(attr_eq + 1));
                } else {
                    attr_name = to_lower(attr);
                }

                if (attr_name == "domain") {
                    // Strip the leading dot for domain matching (e.g., .example.com -> example.com)
                    if (!attr_val.empty() && attr_val[0] == '.') {
                        cookie_domain = attr_val.substr(1);
                    } else {
                        cookie_domain = attr_val;
                    }
                } else if (attr_name == "path") {
                    cookie_path = attr_val;
                }
            }

            if (end == std::string::npos) break;
            start = end + 1;
        }
    }

    // 3. Update if exists for this domain and path, otherwise append
    // Note: HTTP cookies are uniquely identified by their Name, Domain, AND Path.
    bool found = false;
    for (auto& cookie : client_cookies) {
        if (cookie.key == new_key && cookie.domain == cookie_domain && cookie.path == cookie_path) {
            cookie.value = new_value;
            found = true;
            break;
        }
    }

    if (!found) {
        client_cookies.push_back({new_key, new_value, cookie_domain, cookie_path});
    }
}

void init_http_response(HttpResponse& resp) {
    resp.status_code = 0;
    resp.location.clear();
    resp.icy_metaint = 0;
    resp.is_chunked = false;
    resp.set_cookies.clear();
}

size_t create_http_request(char *request,
                           const std::string& host,
                           const std::string& path,
                           bool request_meta,
                           const std::vector<HttpCookie>& client_cookies) 
{
    // Note: Assuming `request` points to a sufficiently large buffer. 
    // In modern C++, consider passing a buffer limit and using `snprintf`.
    int written = sprintf(request,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: Keep-Alive\r\n",
        path.c_str(), host.c_str());

    char* ptr = request + written;

    if (request_meta) {
        ptr += sprintf(ptr, "Icy-MetaData: 1\r\n");
    }

    // Filter and build the Cookie header
    std::string cookie_header_val;
    for (const auto& cookie : client_cookies) {
        // Basic domain matching: check if the target host ends with the cookie's domain
        // (e.g., host "stream.radio.com" matches cookie domain "radio.com")
        if (host.length() >= cookie.domain.length() && 
            host.compare(host.length() - cookie.domain.length(), cookie.domain.length(), cookie.domain) == 0) {
            
            if (!cookie_header_val.empty()) {
                cookie_header_val += "; ";
            }
            cookie_header_val += cookie.key + "=" + cookie.value;
        }
    }

    if (!cookie_header_val.empty()) {
        ptr += sprintf(ptr, "Cookie: %s\r\n", cookie_header_val.c_str());
    }

    ptr += sprintf(ptr, "\r\n"); // End of headers

    return static_cast<size_t>(ptr - request);
}

void parse_http_response_line(const std::string &line,
                              int line_count,
                              HttpResponse &resp) 
{
    if (line_count == 0) {
        int code = 0;

        if (sscanf(line.c_str(), "HTTP/1.%*d %d", &code) == 1) {
            resp.status_code = code;
            return;
        }

        if (line.rfind(ICY_200_OK_STATUS.data(), 0) == 0) {
            resp.status_code = 200;
            return;
        }

        throw ProtocolError("Invalid or unsupported protocol header");
    }

    auto trim_left = [](const std::string &s, size_t pos) {
        while (pos < s.size() && s[pos] == ' ') {
            pos++;
        }
        return pos;
    };

    const std::string &l = line;

    if (iequals_prefix(l, HEADER_LOCATION)) {
        size_t pos = trim_left(l, HEADER_LOCATION.size());
        resp.location = l.substr(pos);
        return;
    }

    if (iequals_prefix(l, HEADER_SET_COOKIE)) {
        size_t pos = trim_left(l, HEADER_SET_COOKIE.size());
        
        // Push the entire raw cookie string into the vector so that 
        // the client can parse the `Domain=` and `Path=` attributes later.
        resp.set_cookies.push_back(l.substr(pos));
        return;
    }

    if (iequals_prefix(l, HEADER_ICY_METAINT)) {
        size_t pos = trim_left(l, HEADER_ICY_METAINT.size());
        try {
            resp.icy_metaint = std::stoi(l.substr(pos));
        } catch (...) {
            throw ProtocolError("Invalid icy-metaint value");
        }
        return;
    }

    if (iequals_prefix(l, HEADER_TRANSFER_ENC)) {
        if (l.find(CHUNKED_ENC_VAL.data()) != std::string::npos) {
            resp.is_chunked = true;
        }
        return;
    }
}