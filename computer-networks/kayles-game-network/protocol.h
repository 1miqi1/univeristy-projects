#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <variant>
#include <span>

// ----------------------------
// Constants
// ----------------------------

/**
 * Size of client message prefix used in error reporting.
 */
constexpr std::size_t CLIENT_MESSAGE_PREFIX_SIZE = 12;

/**
 * Status value indicating an invalid message.
 */
constexpr std::uint8_t WRONG_MESSAGE_STATUS = 255;


/**
 * Max pawn row range
 */
constexpr std::uint8_t MAX_PAWN_RANGE = 255;

/**
 * Game id field index for error reporting.
 */
constexpr std::size_t GAME_ID_INDEX = 5;

// ----------------------------
// Enums
// ----------------------------

/**
 * Indexes of fields in a parsed client message.
 */
enum MsgFieldIndex : std::uint8_t {
    TYPE = 0,
    PLAYER_ID = 1,
    GAME_ID = 2,
    PAWN = 3
};

/**
 * Number of fields expected in each client message type.
 */
enum MsgNumFields : std::uint8_t {
    MSG_JOIN_FIELDS = 2,
    MSG_MOVE_1_FIELDS = 4,
    MSG_MOVE_2_FIELDS = 4,
    MSG_KEEP_ALIVE_FIELDS = 3,
    MSG_GIVE_UP_FIELDS = 3,
};

/**
 * Types of messages sent from client to server.
 */
enum MsgType : std::uint8_t {
    MSG_JOIN = 0,
    MSG_MOVE_1 = 1,
    MSG_MOVE_2 = 2,
    MSG_KEEP_ALIVE = 3,
    MSG_GIVE_UP = 4,
};

/**
 * Types of responses sent from server to client.
 */
enum ResponseType : std::uint8_t {
    MSG_CORRECT_MESSAGE = 0,
    MSG_WRONG_MESSAGE = 1,
};

/**
 * Expected lengths of each message type in wire format.
 */
enum MsgTypeLengths : std::size_t {
    MSG_JOIN_LENGTH = 5,
    MSG_MOVE_1_LENGTH = 10,
    MSG_MOVE_2_LENGTH = 10,
    MSG_KEEP_ALIVE_LENGTH = 9,
    MSG_GIVE_UP_LENGTH = 9,
    MSG_WRONG_MESSAGE_LENGTH = 14,
    GAME_STATE_HEADER_SIZE = 14,
};

/**
 * Result codes for parsing textual input.
 */
enum ParseStatus : int {
    PARSE_OK = 0,
    PARSE_ERR_INPUT = -1,
    PARSE_ERR_RESOURCE = -2
};

/**
 * Represents current state of the game match.
 */
enum GameStatus : std::uint8_t {
    WAITING_FOR_OPPONENT = 0,
    TURN_A = 1,
    TURN_B = 2,
    WIN_A = 3,
    WIN_B = 4
};

// ----------------------------
// Structures
// ----------------------------

/**
 * Result of a text parsing operation.
 */
struct ParseResult {
    ParseStatus status;
    const char* msg;
};

/**
 * Parsed client message.
 */
struct ClientMessage {
    std::uint8_t msg_type = 0;
    std::uint32_t player_id = 0;
    std::uint32_t game_id = 0;
    std::uint8_t pawn = 0;
};

/**
 * Full game state used for synchronization.
 */
struct GameState {
    std::uint32_t game_id = 0;
    std::uint32_t player_a_id = 0;
    std::uint32_t player_b_id = 0;
    std::uint8_t status = 0;
    std::uint8_t max_pawn = 0;
    std::vector<std::uint8_t> pawn_row;
};

/**
 * Error details for WRONG_MESSAGE responses.
 */
struct WrongMessage {
    std::uint8_t client_message_prefix[CLIENT_MESSAGE_PREFIX_SIZE] = {};
    std::uint8_t status = 0;
    std::uint8_t error_index = 0;
};

/**
 * Container for any server-to-client response.
 */
struct ServerResponse {
    ResponseType response_type = MSG_CORRECT_MESSAGE;
    std::variant<WrongMessage, GameState> response;
};

// ----------------------------
// Logic Functions
// ----------------------------

/**
 * Parses a client message from text form.
 *
 * @param text  Input text message
 * @param msg   Output parameter for parsed message
 * @return Parsing result with status and error description
 */
ParseResult parse_client_message(const std::string& text, ClientMessage& msg);

/**
 * Converts parsing result into an error string.
 *
 * @param err  Parsing result
 * @return Human-readable error description
 */
std::string parse_error_string(ParseResult err);

/**
 * Handles invalid game state by updating error index.
 *
 * @param error_index  Output parameter for error position
 */
void handle_invalid_game(std::uint8_t& error_index);

/**
 * Creates WrongMessage based on clients message
 *
 * @param wrong_message wrong message output
 * @param buf  client message
 * @param error_index  Parameter for error position
 */
void create_wrong_message(WrongMessage& wrong_message,
                          std::span<const std::uint8_t> buf,
                          std::uint8_t error_index);

/**
 * Returns expected binary size of a client message.
 *
 * @param msg  Client message
 * @return Size of serialized message in bytes
 */
std::size_t get_client_message_size(const ClientMessage& msg);

/**
 * Serializes a client message into the provided buffer.
 *
 * @param msg  Client message to serialize
 * @param buf  Output buffer
 * @return Number of bytes written, or 0 on failure
 */
std::size_t serialize_client_message(const ClientMessage& msg, std::span<std::uint8_t> buf);

/**
 * Serializes a game state into the provided buffer.
 *
 * @param state  Game state to serialize
 * @param buf    Output buffer
 * @return Number of bytes written, or 0 on failure
 */
std::size_t serialize(const GameState& state, std::span<std::uint8_t> buf);

/**
 * Serializes a WRONG_MESSAGE response into the provided buffer.
 *
 * @param wrong_message  Error response to serialize
 * @param buf            Output buffer
 * @return Number of bytes written, or 0 on failure
 */
std::size_t serialize(const WrongMessage& wrong_message, std::span<std::uint8_t> buf);

/**
 * Serializes a server response into the provided buffer.
 *
 * @param server_response  Server response to serialize
 * @param buf              Output buffer
 * @return Number of bytes written, or 0 on failure
 */
std::size_t serialize(const ServerResponse& server_response, std::span<std::uint8_t> buf);

/**
 * Deserializes a client message from the provided buffer.
 *
 * @param msg          Output parameter for deserialized message
 * @param buf          Input buffer
 * @param recieved_length
 * @param error_index  Output parameter for error position
 * @return true on success, false on failure
 */
bool deserialize_client_message(ClientMessage& msg,
                                std::span<const std::uint8_t> buf,
                                std::size_t recieved_length,
                                std::uint8_t& error_index);

/**
 * Deserializes a game state from the provided buffer.
 *
 * @param game_state  Output parameter for deserialized game state
 * @param buf         Input buffer
 * @return true on success, false on failure
 */
bool deserialize(GameState& game_state, std::span<const std::uint8_t> buf);

/**
 * Deserializes a WRONG_MESSAGE response from the provided buffer.
 *
 * @param wrong_message  Output parameter for deserialized error response
 * @param buf            Input buffer
 * @return true on success, false on failure
 */
bool deserialize(WrongMessage& wrong_message, std::span<const std::uint8_t> buf);

/**
 * Deserializes a server response from the provided buffer.
 *
 * @param msg  Output parameter for deserialized server response
 * @param buf  Input buffer
 * @return true on success, false on failure
 */
bool deserialize(ServerResponse& msg, std::span<const std::uint8_t> buf);

/**
 * Prints a client message 
 *
 * @param msg  Client message to print
 */
void print(const ClientMessage& msg);

/**
 * Prints a game state 
 *
 * @param game_state  Game state to print
 */
void print(const GameState& game_state);

/**
 * Prints a WRONG_MESSAGE response 
 *
 * @param wrong_message  Error response to print
 */
void print(const WrongMessage& wrong_message);

/**
 * Prints a server response 
 *
 * @param server_response  Server response to print
 */
void print(const ServerResponse& server_response);