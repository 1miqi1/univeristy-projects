#pragma once

#include <cstddef>
#include <cstdint>

void handle_audio(const uint8_t* data, size_t len);

void handle_metadata(const uint8_t* data, size_t len);

int read_user_line(char* buffer, size_t size);