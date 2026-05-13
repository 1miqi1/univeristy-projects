#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <protocol.hpp>
#include <logger.hpp>


void init_http_response(HttpResponse *resp) {
    resp->status_code = 0;
    resp->location = {}; // Initialize as empty string
    resp->icy_metaint = 0;
    resp->is_chunked = false;
    resp->cookie = {};   // Initialize as empty string
}

int create_http_request(char *request,
                        const std::string &host,
                        const std::string &path,
                        bool request_meta,
                        const std::string &cookie) {

    std::string tmp;

    tmp += "GET " + path + " HTTP/1.1\r\n";
    tmp += "Host: " + host + "\r\n";
    tmp += "Connection: Keep-Alive\r\n";

    if (request_meta)
        tmp += "Icy-MetaData: 1\r\n";

    if (!cookie.empty())
        tmp += "Cookie: " + cookie + "\r\n";

    tmp += "\r\n";


    memcpy(request, tmp.c_str(), tmp.size());
    return (int)tmp.size();
}

// Function to parse individual lines of an HTTP response
bool parse_http_response(const char* line, int bytes, int line_count, 
                         HttpResponse *resp) {

    // On the very first line, reset the response structure
    if (line_count == 0) {
        init_http_response(resp);
    }

    // Analyze the first line (Status Line: e.g., HTTP/1.1 200 OK)
    if (line_count == 0) {
        if (sscanf(line, "HTTP/1.%*d %d", &resp->status_code) != 1) {
            // Support for older Shoutcast versions starting with "ICY 200 OK"
            if (strncmp(line, "ICY 200", 7) == 0) {
                resp->status_code = 200;
            } else {
                return false; // Invalid protocol format
            }
        }
    } else {
        // Case-insensitive header analysis
        
        // Handle redirection: "Location: https://..."
        if (strncasecmp(line, "Location:", 9) == 0) {
            sscanf(line + 9, " %1023[^\r\n]", resp->location);
        } 
        // Handle cookies: extract only the part before the first semicolon
        else if (strncasecmp(line, "Set-Cookie:", 11) == 0) {
            sscanf(line + 11, " %1023[^;\r\n]", resp->cookie);
        }
        // Metadata interval for Shoutcast streams
        else if (strncasecmp(line, "icy-metaint:", 12) == 0) {
            sscanf(line + 12, " %d", &resp->icy_metaint);
        }
        // Check if the body is sent in chunks
        else if (strncasecmp(line, "Transfer-Encoding:", 18) == 0) {
            if (strstr(line, "chunked")) {
                resp->is_chunked = true;
            }
        }
    }


    return true;
}