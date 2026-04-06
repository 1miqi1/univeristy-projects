#pragma once
#include <stdint.h>
#include <stddef.h>

// ----------------------------
// Constants
// ----------------------------
#define MAX_MESSAGE_LENGTH 4   // maximum number of tokens in a client message
#define MAX_UINT8        255   // maximum value for a byte


typedef enum {
    MSG_JOIN = 0,
    MSG_MOVE_1 = 1,
    MSG_MOVE_2 = 2,
    MSG_KEEP_ALIVE = 3,
    MSG_GIVE_UP = 4,
} MsgType;

// ----------------------------
// Parsing errors
// ----------------------------
typedef enum {
    PARSE_OK = 0,
    PARSE_ERR_NULL_ARGUMENT,
    PARSE_ERR_MEMORY,
    PARSE_ERR_INVALID_NUMBER,
    PARSE_ERR_TOO_FEW_FIELDS,
    PARSE_ERR_INVALID_MESSAGE,
    PARSE_ERR_UNKNOWN_TYPE
} ParseResult;

// ----------------------------
// Game status
// ----------------------------
typedef enum {
    WAITING_FOR_OPPONENT = 0,
    TURN_A = 1,
    TURN_B = 2,
    WIN_A = 3,
    WIN_B = 4
} GameStatus;

// ----------------------------
// Client message structure
// ----------------------------
typedef struct {
    uint8_t msg_type;     // message type
    uint32_t player_id;   // player sending the message
    uint32_t game_id;     // game id (if applicable)
    uint8_t pawn;         // pawn number (if applicable)
} ClientMessage;

// ----------------------------
// Game state structure (server -> client)
// ----------------------------
typedef struct {
    uint32_t game_id;       // unique id for the game
    uint32_t player_a_id;   // id of player A
    uint32_t player_b_id;   // id of player B (0 if none)
    uint8_t status;         // current game status (GameStatus)
    uint8_t max_pawn;       // max pawn number
    uint8_t pawn_row[33];   // bitmap of pawns (floor(256/8)+1)
} GameState;

// ----------------------------
// Helper functions
// ----------------------------


// Serialize ClientMessage into buffer
// Returns number of bytes written or -1 on error
size_t serialize_client_message(const ClientMessage *msg, uint8_t *buf);

// Deserialize ClientMessage
int deserialize_client_message(ClientMessage *msg, const uint8_t *buf);

// Parse error to string
const char* parse_error_string(ParseResult err);

// Parse ClientMessage from buffer
// Returns 0 on success, -1 on error
ParseResult parse_client_message(const char *string, ClientMessage *msg);

size_t get_client_message_size(const ClientMessage *msg);

// Prints ClientMessage contents
void print_client_message(const ClientMessage *msg);

// Serialize GameState into buffer
// Returns number of bytes written
size_t serialize_game_state(const GameState *state, uint8_t *buf);
