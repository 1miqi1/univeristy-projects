#pragma once

#include <cstdint>
#include <string>

struct HttpResponse {
    int status_code = 0;
    char location[1024];
    char cookie[1024];
    int icy_metaint = 0;
    bool is_chunked = false;
};

void init_http_response(HttpResponse *resp);

int send_http_request(char *request,
                      const char *host,
                      const char *path,
                      bool request_meta,
                      const char *cookie);

bool parse_http_response(const char* line,
                         int bytes,
                         int line_count,
                         HttpResponse *resp);