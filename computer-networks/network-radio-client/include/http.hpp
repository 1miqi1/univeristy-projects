#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct HttpCookie {
    std::string key;
    std::string value;
    std::string domain;
    std::string path;
};

/**
 * @brief Structure containing parsed data from an HTTP response.
 */
struct HttpResponse {
    int status_code = 0;                      ///< The HTTP status code (e.g., 200, 404)
    std::string location;                     ///< The redirect location, if provided
    std::vector<std::string> set_cookies;     ///< List of raw Set-Cookie headers from the server
    int icy_metaint = 0;                      ///< ICY metadata interval (bytes between metadata chunks)
    bool is_chunked = false;                  ///< True if Transfer-Encoding is chunked
};

/**
 * @brief Resets an HttpResponse object to its default state.
 * 
 * @param resp The HttpResponse object to initialize.
 */
void init_http_response(HttpResponse& resp);

void validate_http_response(const HttpResponse& resp);

void merge_cookie(std::vector<HttpCookie>& client_cookies, 
                  const std::string& set_cookie_header, 
                  const std::string& current_host);
/**
 * @brief Generates an HTTP GET request string.
 * 
 * @param request      Pointer to the buffer where the request will be written.
 * @param host         The target hostname.
 * @param path         The request path and query string.
 * @param request_meta Whether to request ICY metadata.
 * @param client_cookies Vector of cookies stored by the client.
 * @return size_t      The total number of bytes written to the buffer.
 */
size_t create_http_request(char *request,
                           const std::string& host,
                           const std::string& path,
                           bool request_meta,
                           const std::vector<HttpCookie>& client_cookies);

/**
 * @brief Parses a single line from an HTTP response header block.
 * 
 * @param line       The line of text to parse.
 * @param line_count The current line index (0 indicates the status line).
 * @param resp       The HttpResponse struct to populate.
 * @throws ProtocolError if the status line is malformed.
 */
void parse_http_response_line(const std::string &line,
                              int line_count,
                              HttpResponse &resp);