#pragma once

#include <stdexcept>
#include <string>
#include <system_error>

/**
 * @brief Base exception for the entire Radio project.
 * 
 * Catching this will safely catch any custom runtime error generated 
 * by the application's internal logic.
 */
class RadioError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Errors related to the ICY/HTTP protocol parsing.
 * 
 * Thrown when the application encounters malformed headers, invalid 
 * metadata lengths, or unexpected protocol formatting.
 */
class ProtocolError : public RadioError {
public:
    using RadioError::RadioError;
};

/**
 * @brief Errors specifically for unhandled HTTP status codes.
 * 
 * Thrown when the server returns a 4xx or 5xx HTTP error code, 
 * or when a redirect cannot be properly followed.
 */
class HttpException : public ProtocolError {
public:
    int status_code; ///< The HTTP status code returned by the server (e.g., 404, 403)

    /**
     * @brief Constructs an HttpException with a specific status code and message.
     * 
     * @param code The HTTP status code triggering the exception.
     * @param message A descriptive error message.
     */
    HttpException(int code, const std::string& message) 
        : ProtocolError(message), status_code(code) {}
};

/**
 * @brief Errors involving the system/network layer.
 * 
 * This covers low-level system failures (e.g., socket creation failed, 
 * poll errors, or recv/send failures) and automatically wraps standard 
 * system errnos.
 */
class NetworkError : public std::system_error {
public:
    /**
     * @brief Constructs a NetworkError from a system error number.
     * 
     * @param ev The system error value (typically errno).
     * @param msg A descriptive message regarding what operation failed.
     */
    NetworkError(int ev, const std::string& msg)
        : std::system_error(ev, std::generic_category(), msg) {}
};

/**
 * @brief Errors for invalid command-line arguments.
 * 
 * Thrown during application initialization if the provided CLI flags 
 * or parameters are missing, malformed, or out of bounds.
 */
class ArgumentError : public RadioError {
public:
    using RadioError::RadioError;
};

/**
 * @brief Base class for high-level connection issues.
 * 
 * Covers failures related to DNS resolution, socket binding, 
 * TLS handshakes, or connection timeouts.
 */
class ConnectionException : public RadioError {
public:
    using RadioError::RadioError;
};

/**
 * @brief Thrown when the server unexpectedly drops an active connection.
 * 
 * Used to signal a graceful fallback or reconnection attempt when 
 * receiving EOF, ECONNRESET, or zero-byte returns from the server.
 */
class ServerDisconnectedException : public ConnectionException {
public:
    /**
     * @brief Constructs a ServerDisconnectedException with a default message.
     */
    ServerDisconnectedException() : ConnectionException("Server closed the connection") {}
};

/**
 * @brief Catch-all exception for malformed usage.
 * 
 * Thrown for improper URL structures, invalid URI schemes, or logic requests 
 * that violate the client's current state. Classifying them under one unified 
 * exception keeps the hierarchy clean.
 */
class InvalidRequestException : public RadioError {
public:
    using RadioError::RadioError;
};