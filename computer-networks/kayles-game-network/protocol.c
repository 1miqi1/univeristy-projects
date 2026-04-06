#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "protocol.h"
#include "common.h"

const char* parse_error_string(ParseResult err) {
    switch (err) {
        case PARSE_OK:
            return "OK";
        case PARSE_ERR_NULL_ARGUMENT:
            return "Null argument";
        case PARSE_ERR_MEMORY:
            return "Memory allocation failed";
        case PARSE_ERR_INVALID_NUMBER:
            return "Invalid number in message";
        case PARSE_ERR_TOO_FEW_FIELDS:
            return "Too few fields in message";
        case PARSE_ERR_INVALID_MESSAGE:
            return "Invalid message format";
        case PARSE_ERR_UNKNOWN_TYPE:
            return "Unknown message type";
        default:
            return "Unknown parse error";
    }
}

ParseResult parse_client_message(const char* string, ClientMessage* msg) {
    if (!string || !msg) 
        return (ParseResult){PARSE_ERR_SYSTEM, "Internal error: null argument"};

    memset(msg, 0, sizeof(*msg));
    char* copy = strdup(string);
    if (!copy) return (ParseResult){PARSE_ERR_SYSTEM, "Memory allocation failed"};

    // Buffer to hold tokens (max 5 expected based on MSG_MOVE_2 + 1 safety)
    uint32_t table[6]; 
    int count = 0;
    char* token = strtok(copy, "/");

    while (token) {
        if (count >= 5) {
            free(copy);
            return (ParseResult){PARSE_ERR_COUNT, "Too many fields provided"};
        }
        
        uint32_t value;
        if (validate_number(token, &value) != 0) {
            free(copy);
            return (ParseResult){PARSE_ERR_SYNTAX, "Fields must be non-negative integers"};
        }
        
        table[count++] = value;
        token = strtok(NULL, "/");
    }
    free(copy);

    if (count < 2) 
        return (ParseResult){PARSE_ERR_COUNT, "Message too short: requires at least type/player_id"};

    msg->msg_type  = (uint8_t)table[0];
    msg->player_id = table[1];

    // Specific validation for each type
    switch (msg->msg_type) {
        case MSG_JOIN:
            if (count != 2) return (ParseResult){PARSE_ERR_COUNT, "MSG_JOIN (0) requires exactly: 0/player_id"};
            break;

        case MSG_MOVE_1:
        case MSG_MOVE_2:
            if (count != 4) return (ParseResult){PARSE_ERR_COUNT, "Move messages (1,2) require: type/player_id/game_id/pawn"};
            if (table[3] > MAX_UINT8) return (ParseResult){PARSE_ERR_RANGE, "Pawn index must be 0-255"};
            msg->game_id = table[2];
            msg->pawn    = (uint8_t)table[3];
            break;

        case MSG_KEEP_ALIVE:
        case MSG_GIVE_UP:
            if (count != 3) return (ParseResult){PARSE_ERR_COUNT, "Utility messages (3,4) require: type/player_id/game_id"};
            msg->game_id = table[2];
            break;

        default:
            return (ParseResult){PARSE_ERR_TYPE, "Unknown message type (must be 0-4)"};
    }

    return (ParseResult){PARSE_OK, "Success"};
}

void print_client_message(const ClientMessage* msg) {

    if (!msg)
        return;

    printf("ClientMessage {\n");
    printf("  msg_type  : %u\n", msg->msg_type);
    printf("  player_id : %u\n", msg->player_id);
    printf("  game_id   : %u\n", msg->game_id);
    printf("  pawn      : %u\n", msg->pawn);
    printf("}\n");
}

size_t get_client_message_size(const ClientMessage *msg) {
    if (!msg) return 0;

    switch (msg->msg_type) {
        case MSG_JOIN:       return 1 + 4;
        case MSG_MOVE_1:
        case MSG_MOVE_2:     return 1 + 4 + 4 + 1;
        case MSG_KEEP_ALIVE:
        case MSG_GIVE_UP:    return 1 + 4 + 4;
        default:             return 0; // unknown type
    }
}

size_t serialize_client_message(const ClientMessage *msg, uint8_t *buf){
    if(!msg || !buf){
        return 0;
    }

    size_t offset = 0;
    buf[offset++] = msg->msg_type;

    uint32_t net_player_id = htonl(msg->player_id);
    memcpy(buf + offset, &net_player_id, sizeof(net_player_id));
    offset += sizeof(net_player_id);

    if(msg->msg_type != MSG_JOIN){
        uint32_t net_game_id = htonl(msg->game_id);
        memcpy(buf + offset, &net_game_id, sizeof(net_game_id));
        offset += sizeof(net_game_id);

        if(msg->msg_type == MSG_MOVE_1 || msg->msg_type == MSG_MOVE_2){
            buf[offset++] = msg->pawn;
        }
    }

    return offset;
}
