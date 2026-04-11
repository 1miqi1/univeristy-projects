#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <variant>

// ----------------------------
// Constants
// ----------------------------

// Maximum length of a message (in bytes or fields depending on context)
constexpr std::size_t MAX_MESSAGE_LENGTH = 4;

// Game id index in cleint message
constexpr std::size_t GAME_ID_INDEX = 5;

// Maximum value for uint8_t (used as validation boundary)
constexpr std::uint8_t MAX_UINT8 = 255;

// Maximum number of pawns supported in a row
constexpr std::size_t MAX_PAWN_ROW_LENGTH = 256;

// Size of client message prefix used in error reporting
constexpr std::size_t CLIENT_MESSAGE_PREFIX_SIZE = 12;

// Status value indicating an invalid message
constexpr std::uint8_t WRONG_MESSAGE_STATUS = 255;


// ----------------------------
// Message types
// ----------------------------

// Types of messages sent from client to server
enum MsgType : std::uint8_t {
    MSG_JOIN = 0,        // Join a game
    MSG_MOVE_1 = 1,      // Remove one pawn
    MSG_MOVE_2 = 2,      // Remove two adjacent pawns
    MSG_KEEP_ALIVE = 3,  // Keep connection alive
    MSG_GIVE_UP = 4,     // Resign from the game
};

// Types of responses sent from server to client
enum ResponseType : std::uint8_t {
    MSG_CORRECT_MESSAGE = 0, // Valid response (game state)
    MSG_WRONG_MESSAGE = 1,   // Error response
};

// Expected lengths of each message type (used for validation)
enum MsgTypeLengths : std::size_t {
    MSG_JOIN_LENGTH       = 5,
    MSG_MOVE_1_LENGTH     = 10,
    MSG_MOVE_2_LENGTH     = 10,
    MSG_KEEP_ALIVE_LENGTH = 9,
    MSG_GIVE_UP_LENGTH    = 9,
    MSG_WRONG_MESSAGE_LENGTH = 14,
};

// ----------------------------
// Parsing errors
// ----------------------------

// Result codes for parsing operations
enum ParseStatus : int {
    PARSE_OK = 0,           // Parsing successful
    PARSE_ERR_INPUT = -1,   // Invalid input format
    PARSE_ERR_RESOURCE = -2 // Resource or memory error
};

// ----------------------------
// Parsing error result
// ----------------------------

// Represents parsing result with status and optional message
struct ParseResult {
    ParseStatus status; // Result code
    const char* msg;    // Error description (if any)
};

// ----------------------------
// Game status
// ----------------------------

// Represents current state of the game
enum GameStatus : std::uint8_t {
    WAITING_FOR_OPPONENT = 0, // Game not started yet
    TURN_A = 1,               // Player A turn
    TURN_B = 2,               // Player B turn
    WIN_A = 3,                // Player A won
    WIN_B = 4                 // Player B won
};

// ----------------------------
// Client message structure
// ----------------------------

// Represents a parsed message sent by client
struct ClientMessage {
    std::uint8_t msg_type = 0;   // Type of message (MsgType)
    std::uint32_t player_id = 0; // Sender player ID
    std::uint32_t game_id = 0;   // Target game ID
    std::uint8_t pawn = 0;       // Pawn index (if applicable)
};

// ----------------------------
// Game state structure (server -> client)
// ----------------------------

// Represents full game state sent from server to client
struct GameState {
    std::uint32_t game_id = 0;
    std::uint32_t player_a_id = 0;
    std::uint32_t player_b_id = 0;
    std::uint8_t status = 0;       // GameStatus
    std::uint8_t max_pawn = 0;     // Maximum pawn index
    std::vector<std::uint8_t> pawn_row; // Bitset representing pawn positions
};

// ----------------------------
// Packed error message (wire format)
// ----------------------------

// Represents an error response sent to client
struct WrongMessage {
    std::uint8_t client_message_prefix[CLIENT_MESSAGE_PREFIX_SIZE] = {}; // Partial original message
    std::uint8_t status = 0;       // Error status
    std::uint8_t error_index = 0;  // Position where error occurred
};

// Represents full server response (either success or error)
struct ServerResponse {
    ResponseType response_type = MSG_CORRECT_MESSAGE;
    std::variant<WrongMessage, GameState> response; // Either error or game state
};

// ----------------------------
// Helper functions
// ----------------------------

/**
 * Parses textual client message into structured form.
 *
 * @param text Input message string
 * @param msg  Output parsed message
 * @return ParseResult containing status and error info
 */
ParseResult parse_client_message(const std::string& text, ClientMessage& msg);

/**
 * Converts parsing error into human-readable string.
 *
 * @param err Parsing result
 * @return Error description string
 */
std::string parse_error_string(ParseResult err);

/**
 * Prints client message (for debugging/logging).
 *
 * @param msg Client message to print
 */
void print_client_message(const ClientMessage& msg);

/**
 * Serializes client message into byte buffer.
 *
 * @param msg Input message
 * @param buf Output buffer
 * @return Number of bytes written
 */
std::size_t serialize_client_message(const ClientMessage& msg,
                                     std::vector<std::uint8_t>& buf);

/**
 * Returns expected serialized size of client message.
 *
 * @param msg Input message
 * @return Size in bytes
 */
std::size_t get_client_message_size(const ClientMessage& msg);

/**
 * Deserializes client message from buffer.
 *
 * @param msg          Output message
 * @param buf          Input buffer
 * @param error_index  Output error position
 * @return true if successful, false otherwise
 */
bool deserialize_client_message(ClientMessage& msg,
                                const std::vector<std::uint8_t>& buf,
                                std::uint8_t& error_index);

/**
 * Handles invalid game state by updating error index.
 *
 * @param error_index Output error position
 */
void handle_invalid_game(std::uint8_t& error_index);

/**
 * Serializes game state into byte buffer.
 *
 * @param state Input game state
 * @param buf   Output buffer
 * @return Number of bytes written
 */
std::size_t serialize_game_state(const GameState& state,
                                 std::vector<std::uint8_t>& buf);

/**
 * Serializes error message into byte buffer.
 *
 * @param wrong_message Input error message
 * @param buf           Output buffer
 * @return Number of bytes written
 */
std::size_t serialize_wrong_message(const WrongMessage& wrong_message,
                                    std::vector<std::uint8_t>& buf);

/**
 * Serializes full server response into byte buffer.
 *
 * @param server_response Input response
 * @param buf             Output buffer
 * @return Number of bytes written
 */
std::size_t serialize_server_response(const ServerResponse& server_response,
                                      std::vector<std::uint8_t>& buf);

/**
 * Deserializes error message from buffer.
 *
 * @param wrong_message Output error message
 * @param buf           Input buffer
 * @return true if successful, false otherwise
 */
bool deserialize_wrong_message(WrongMessage& wrong_message,
                              const std::vector<std::uint8_t>& buf);

/**
 * Deserializes game state from buffer.
 *
 * @param game_state Output game state
 * @param buf        Input buffer
 * @return true if successful, false otherwise
 */
bool deserialize_game_state(GameState& game_state,
                           const std::vector<std::uint8_t>& buf);

/**
 * Deserializes server response from buffer.
 *
 * @param msg Output response
 * @param buf Input buffer
 * @return true if successful, false otherwise
 */
bool deserialize_server_response(ServerResponse& msg,
                                 const std::vector<std::uint8_t>& buf);

/**
 * Prints the contents of a GameState structure.
 *
 * Outputs all fields of the game state, including game identifiers,
 * player identifiers, status, maximum pawn value, and pawn row data.
 *
 * @param game_state GameState structure to print
 */
void print_game_state(const GameState& game_state);

/**
 * Prints the contents of a WrongMessage structure.
 *
 * Outputs the client message prefix (as byte values),
 * along with the status and error index.
 *
 * @param wrong_message WrongMessage structure to print
 */
void print_wrong_message(const WrongMessage& wrong_message);