#pragma once

#include <cstddef>
#include <cstdint>

void handle_audio(const char* buffer, size_t len);

void handle_metadata(const char* buffer, size_t len);

int read_user_line(char* buffer, size_t size);