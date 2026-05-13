#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include "protocol.hpp"

// Define a specific exception for protocol-level failures
class ProtocolException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

void init_http_response(HttpResponse *resp) {
    if (!resp) throw std::invalid_argument("Response pointer is null");
    
    resp->status_code = 0;
    resp->location[0] = '\0'; // Using char arrays for storage
    resp->icy_metaint = 0;
    resp->is_chunked = false;
    resp->cookie[0] = '\0';
}

// Now returns the string directly to avoid manual buffer management
std::string create_http_request(const std::string &host,
                                const std::string &path,
                                bool request_meta,
                                const std::string &cookie) {
    // We use a local string to build the request safely
    std::string request;
    request.reserve(512); // Pre-allocate for performance

    request += "GET " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "Connection: Keep-Alive\r\n";

    if (request_meta)
        request += "Icy-MetaData: 1\r\n";

    if (!cookie.empty())
        request += "Cookie: " + cookie + "\r\n";

    request += "\r\n";

    return request;
}

void parse_http_response_line(const char* line, int line_count, HttpResponse *resp) {
    if (!line || !resp) throw std::invalid_argument("Null arguments passed to parser");

    if (line_count == 0) {
        init_http_response(resp);

        // Exception: Protocol must start with HTTP or ICY
        if (sscanf(line, "HTTP/1.%*d %d", &resp->status_code) != 1) {
            if (strncmp(line, "ICY 200", 7) == 0) {
                resp->status_code = 200;
            } else {
                throw ProtocolException("Invalid or unsupported protocol header");
            }
        }
    } else {
        // Handle headers using case-insensitive checks
        
        if (strncasecmp(line, "Location:", 9) == 0) {
            // Exception: If Location is present but empty/malformed
            if (sscanf(line + 9, " %1023[^\r\n]", resp->location) != 1) {
                throw ProtocolException("Malformed Location header");
            }
        } 
        else if (strncasecmp(line, "Set-Cookie:", 11) == 0) {
            // We don't throw here because a bad cookie shouldn't kill the stream
            sscanf(line + 11, " %1023[^;\r\n]", resp->cookie);
        }
        else if (strncasecmp(line, "icy-metaint:", 12) == 0) {
            // Exception: Metadata interval must be a valid integer
            if (sscanf(line + 12, " %d", &resp->icy_metaint) != 1) {
                throw ProtocolException("Invalid icy-metaint value");
            }
        }
        else if (strncasecmp(line, "Transfer-Encoding:", 18) == 0) {
            if (strstr(line, "chunked")) {
                resp->is_chunked = true;
            }
        }
    }
}