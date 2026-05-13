#pragma once

#include <cstdint>
#include <string>

struct HttpResponse {
    int status_code = 0;
    std::string location;
    std::string cookie;
    int icy_metaint = 0;
    bool is_chunked = false;
};

void init_http_response(HttpResponse *resp);

int create_http_request(char *request,
                      const std::string host,
                      const std::string path,
                      bool request_meta,
                      const std::string cookie);

bool parse_http_response(const char* line,
                         int bytes,
                         int line_count,
                         HttpResponse *resp);