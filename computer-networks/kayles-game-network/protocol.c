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


ParseResult parse_client_message(const char* string, ClientMessage* msg) {
    if (!string || !msg) 
        return (ParseResult){PARSE_ERR_INPUT, "Null arguments"};

    ParseResult result = {PARSE_OK, "Success"};
    memset(msg, 0, sizeof(*msg));
    
    char* copy = strdup(string);
    if (!copy) return (ParseResult){PARSE_ERR_RESOURCE, "Memory allocation failed"};

    uint32_t table[6]; 
    int count = 0;
    char* token = strtok(copy, "/");

    while (token) {
        if (count >= 5) {
            result = (ParseResult){PARSE_ERR_INPUT, "Too many fields provided"};
            goto cleanup;
        }
        
        uint32_t value;
        // Re-using our logic from validate_number
        if (validate_number(token, &value) != 0) {
            result = (ParseResult){PARSE_ERR_INPUT, "Fields must be non-negative integers"};
            goto cleanup;
        }
        
        table[count++] = value;
        token = strtok(NULL, "/");
    }

    if (count < 2) {
        result = (ParseResult){PARSE_ERR_INPUT, "Message too short: requires type/player_id"};
        goto cleanup;
    }

    msg->msg_type  = (uint8_t)table[0];
    msg->player_id = table[1];

    switch (msg->msg_type) {
        case MSG_JOIN:
            if (count != 2) result = (ParseResult){PARSE_ERR_INPUT, "JOIN requires exactly: 0/player_id"};
            break;
        case MSG_MOVE_1:
        case MSG_MOVE_2:
            if (count != 4) result = (ParseResult){PARSE_ERR_INPUT, "MOVE requires: type/player_id/game_id/pawn"};
            else if (table[3] > MAX_UINT8) result = (ParseResult){PARSE_ERR_INPUT, "Pawn index out of range (0-255)"};
            else { msg->game_id = table[2]; msg->pawn = (uint8_t)table[3]; }
            break;
        case MSG_KEEP_ALIVE:
        case MSG_GIVE_UP:
            if (count != 3) result = (ParseResult){PARSE_ERR_INPUT, "Utility msg requires: type/player_id/game_id"};
            else msg->game_id = table[2];
            break;
        default:
            result = (ParseResult){PARSE_ERR_INPUT, "Unknown message type (0-4)"};
    }

cleanup:
    free(copy);
    return result;
}

const char* parse_error_string(ParseResult res) {
    return res.msg;
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

int deserialize_client_message(ClientMessage *msg, const uint8_t *buf, size_t length, uint8_t *error_index) {
    if (!msg || !buf || !error_index) return -1;

    size_t offset = 0;
    *error_index = 0;

    if (length < 1) {  // must have at least msg_type
        *error_index = 0;
        return -1;
    }

    msg->msg_type = buf[offset++];
    switch (msg->msg_type) {
        case MSG_JOIN:
            if (length != MSG_JOIN_LENGHT) {  // 1 byte type + 4 byte player_id
                *error_index = (length < MSG_JOIN_LENGHT) ? length : MSG_JOIN_LENGHT;
                return -1;
            }
            msg->player_id = ntohl(*(uint32_t*)(buf + offset));
            if (msg->player_id == 0) {  // invalid
                *error_index = 1; // first invalid field
                return -1;
            }
            msg->game_id = 0;
            msg->pawn = 0;
            break;

        case MSG_MOVE_1:
        case MSG_MOVE_2:
            if (length != MSG_MOVE_2_LENGHT) { // type + player + game + pawn
                *error_index = (length < MSG_MOVE_2_LENGHT) ? length : MSG_MOVE_2_LENGHT;
                return -1;
            }
            msg->player_id = ntohl(*(uint32_t*)(buf + offset)); offset += 4;
            msg->game_id   = ntohl(*(uint32_t*)(buf + offset)); offset += 4;
            msg->pawn      = buf[offset];

            break;

        case MSG_KEEP_ALIVE:
        case MSG_GIVE_UP:
            if (length != MSG_GIVE_UP_LENGHT) { // type + player + game
                *error_index = (length < MSG_GIVE_UP_LENGHT) ? length : MSG_GIVE_UP_LENGHT;
                return -1;
            }
            msg->player_id = ntohl(*(uint32_t*)(buf + offset)); offset += 4;
            msg->game_id   = ntohl(*(uint32_t*)(buf + offset)); offset += 4;
            msg->pawn = 0;
            break;

        default:
            *error_index = 0;
            return -1; // unknown msg_type
    }

    return 0; // success
}

void handle_invalid_game(uint8_t *error_index){
    *error_index = 5;
}


