#ifndef GAME_H
#define GAME_H

#include "protocol.h"

#define ILLEGAL MOVE -1

typedef struct {
    time_t start;
    uint8_t pawns_left;
    GameState gs;
}Game;


Game* create_full_game(uint32_t game_id, uint32_t a, uint32_t b, uint8_t max_pawns); 
void destroy_game(Game* game);
bool my_game(Game* game, uint32_t player_id);
bool my_turn(Game* game, uint32_t player_id);


#endif