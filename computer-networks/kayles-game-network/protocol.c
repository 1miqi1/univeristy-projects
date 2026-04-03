#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>

#include "protocol.h"
#include "common.h"


int parse_client_message(const char* string, ClientMessage* msg) {
    if (!string || !msg) return -1;

    char* copy = strdup(string);
    if (!copy) return -1;

    char* token = NULL;
    uint32_t table[MAX_MESSAGE_LENGTH] = {0};
    int i = 0;

    token = strtok(copy, "/");
    while (token != NULL && i < MAX_MESSAGE_LENGTH) {
        uint32_t value;
        if (validate_number(token, &value) != 0) {
            free(copy);
            return -1;
        }
        table[i++] = value;
        token = strtok(NULL, "/");
    }
    free(copy);

    // Must have at least msg_type + player_id
    if (i < 2) return -1;

    msg->msg_type   = (uint8_t)table[0];
    msg->player_id  = table[1];

    switch (msg->msg_type) {
        case MSG_JOIN:
            if (i != 2) return -1;
            break;

        case MSG_MOVE_1:
        case MSG_MOVE_2:
            if (i != 4 || table[3] > MAX_UINT8) return -1;
            msg->game_id = table[2];
            msg->pawn    = (uint8_t)table[3];
            break;

        case MSG_KEEP_ALIVE:
        case MSG_GIVE_UP:
            if (i < 3) return -1;
            msg->game_id = table[2];
            break;

        default:
            return -1; // unknown message type
    }

    return 0;
}


void print_client_message(ClientMessage* msg){
    if (!msg) return;

    printf("ClientMessage {\n");
    printf("  msg_type  : %u\n", msg->msg_type);
    printf("  player_id : %u\n", msg->player_id);
    printf("  game_id   : %u\n", msg->game_id);
    printf("  pawn      : %u\n", msg->pawn);
    printf("}\n");
}