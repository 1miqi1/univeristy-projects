#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "protocol.hpp"
#include "exceptions.hpp" // Added our new exceptions header

void init_http_response(HttpResponse& resp) {
    resp.status_code = 0;
    resp.location[0] = '\0'; // Using char arrays for storage
    resp.icy_metaint = 0;
    resp.is_chunked = false;
    resp.cookie[0] = '\0';
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

#include <string>
#include <cstring> // for strncasecmp
#include <algorithm>

void parse_http_response_line(const std::string &line, int line_count, HttpResponse *resp) {
    // line is a reference, so it cannot be null. We only check resp.
    if (!resp) throw InvalidRequestException("Null response object passed to parser");

    if (line_count == 0) {

        // Use c_str() to provide the char* sscanf expects
        if (sscanf(line.c_str(), "HTTP/1.%*d %d", &resp->status_code) != 1) {
            // Check for ICY protocol (common in SHOUTcast/Icecast streams)
            if (line.compare(0, 7, "ICY 200") == 0) {
                resp->status_code = 200;
            } else {
                throw ProtocolError("Invalid or unsupported protocol header");
            }
        }
    } else {
        // We use c_str() for strncasecmp as C++ std::string doesn't have 
        // a built-in case-insensitive "starts_with" until C++20/newer logic.
        const char* c_line = line.c_str();

        if (strncasecmp(c_line, "Location:", 9) == 0) {
            // Pointer arithmetic requires starting from c_str()
            if (sscanf(c_line + 9, " %1023[^\r\n]", resp->location) != 1) {
                throw ProtocolError("Malformed Location header");
            }
        } 
        else if (strncasecmp(c_line, "Set-Cookie:", 11) == 0) {
            sscanf(c_line + 11, " %1023[^;\r\n]", resp->cookie);
        }
        else if (strncasecmp(c_line, "icy-metaint:", 12) == 0) {
            if (sscanf(c_line + 12, " %d", &resp->icy_metaint) != 1) {
                throw ProtocolError("Invalid icy-metaint value");
            }
        }
        else if (strncasecmp(c_line, "Transfer-Encoding:", 18) == 0) {
            // Use std::string::find for the substring check
            if (line.find("chunked") != std::string::npos) {
                resp->is_chunked = true;
            }
        }
    }
}