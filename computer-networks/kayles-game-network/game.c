#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "err.h"
#include "game.h"
#include "common.h"

#define BYTE 8

GameState* create_game(uint32_t game_id,  uint8_t max_pawns, uint8_t* pawn_row) {
    GameState *g = malloc(sizeof(GameState));
    if (!g) return NULL;

    g->game_id = game_id;
    g->player_a_id = 0;
    g->player_b_id = 0;
    g->status = 0;
    g->max_pawn = max_pawns;

    size_t bytes = (max_pawns / BYTE) + 1;
    g->pawn_row = calloc(bytes, sizeof(uint8_t));
    if (!g->pawn_row) {
        free(g);
        return NULL;
    }

    memcpy(g->pawn_row, pawn_row, sizeof(g->pawn_row));

    return g;
}

void destroy_game_state(GameState* g) {
    if (!g) return;
    free(g->pawn_row);
    free(g);
}

Game* create_full_game(uint32_t game_id, uint8_t max_pawns, uint8_t* pawn_row, uint8_t pawns_letf) {
    Game* game = malloc(sizeof(Game));
    if (!game) return NULL;

    // Set start time
    game->start = time(NULL);
    game->pawns_left = pawns_letf;
    // Set pawns left

    // Allocate and initialize GameState
    game->gs = *(create_game(game_id, max_pawns, pawn_row));
    if (!game->gs.pawn_row) {
        free(game);
        return NULL;
    }

    return game;
}

// Cleanup
void destroy_game(Game* game) {
    if (!game) return;

    free(game->gs.pawn_row);
    free(game);
}

bool check_my_game(Game* game, uint32_t player_id){
    return game->gs.player_a_id == player_id || game->gs.player_b_id == player_id;
}

bool check_my_turn(Game* game, uint32_t player_id){
    switch (game->gs.status)
    {
    case TURN_A:
        return game->gs.player_a_id == player_id;
    case TURN_B:
        return game->gs.player_b_id == player_id;
    default:
        return false;
    }
}


void join_game(Game* game, uint32_t player_id){
    if(!game->gs.player_a_id){
        game->gs.player_a_id = player_id;
    }else{
        game->gs.player_b_id = player_id;
        game->gs.status = TURN_A;
    }
}


void make_move_1(Game* game, uint8_t pawn){
    if(game->gs.max_pawn > pawn && bitset_get(game->gs.pawn_row, pawn)){
        bitset_set(game->gs.pawn_row, pawn, false);
        game->pawns_left -= 1;
        if(!game->pawns_left){
            if(game->gs.status == WIN_A){
                game->gs.status = WIN_A;
            }else{
                game->gs.status = WIN_B;
            }
        }else{
            if(game->gs.status == TURN_A){
                game->gs.status = TURN_B;
            }else{
                game->gs.status = WIN_B;
            }
        }
    }
}

void make_move_2(Game* game, uint8_t pawn){
    if(game->gs.max_pawn > pawn + 1 && bitset_get(game->gs.pawn_row, pawn) && bitset_get(game->gs.pawn_row, pawn + 1)){
        bitset_set(game->gs.pawn_row, pawn, false);
        bitset_set(game->gs.pawn_row, pawn + 1, false);
        game->pawns_left -= 2;
        if(!game->pawns_left){
            if(game->gs.status == WIN_A){
                game->gs.status = WIN_A;
            }else{
                game->gs.status = WIN_B;
            }
        }else{
            if(game->gs.status == TURN_A){
                game->gs.status = TURN_B;
            }else{
                game->gs.status = WIN_B;
            }
        }
    }
}

void give_up(Game* game){
    if(game->gs.status == TURN_A){
        game->gs.status = WIN_B;
    }else if(game->gs.status == TURN_B){
        game->gs.status = WIN_A;
    }
}


