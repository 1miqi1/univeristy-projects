#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <protocol.hpp>
#include <logger.hpp>

#define MAX_HEADER_LINE 2048

void init_http_response(HttpResponse *resp) {
    resp->status_code = 0;
    resp->location[0] = '\0'; // Initialize as empty string
    resp->icy_metaint = 0;
    resp->is_chunked = false;
    resp->cookie[0] = '\0';   // Initialize as empty string
}

int send_http_request(char *request, 
                      const char *host, 
                      const char *path, 
                      bool request_meta, 
                      const char *cookie) {
    int len = 0;

    // Construct the Request Line and mandatory Host header
    len += snprintf(request + len, MAX_HEADER_LINE - len, "GET %s HTTP/1.1\r\n", path);
    len += snprintf(request + len, MAX_HEADER_LINE - len, "Host: %s\r\n", host);
    len += snprintf(request + len, MAX_HEADER_LINE - len, "Connection: Keep-Alive\r\n");

    // Optional: Request Shoutcast metadata (track titles)
    if (request_meta) {
        len += snprintf(request + len, MAX_HEADER_LINE - len, "Icy-MetaData: 1\r\n");
    }

    // Optional: Send session cookies if available
    if (cookie && strlen(cookie) > 0) {
        len += snprintf(request + len, MAX_HEADER_LINE - len, "Cookie: %s\r\n", cookie);
    }

    // Every HTTP request must end with an empty line (\r\n)
    len += snprintf(request + len, MAX_HEADER_LINE - len, "\r\n");

    return len;
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