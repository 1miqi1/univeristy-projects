#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

void handle_audio(const char* data, size_t len);

void handle_metadata(std::string& meta);
