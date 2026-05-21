#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * @brief Writes audio data to standard output safely, handling interruptions.
 * 
 * @param data Pointer to the audio data buffer.
 * @param len  Length of the data to write.
 * @throws NetworkError if a fatal pipe/write error occurs.
 */
void handle_audio(const char* data, size_t len);

/**
 * @brief Processes and writes stream metadata to standard error.
 * 
 * @param meta The metadata string to process and print. Modifies the string in place 
 *             to remove ICY padding and append a newline.
 * @throws NetworkError if a fatal pipe/write error occurs.
 */
void handle_metadata(const std::string& meta);