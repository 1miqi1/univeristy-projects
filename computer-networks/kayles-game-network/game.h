#ifndef GAME_H
#define GAME_H

#include <cstdint>
#include <ctime>
#include <vector>
#include <map>

#include "protocol.h"

// Represents full game state including timing and remaining pawns.
typedef struct {
    std::time_t start;        // Timestamp when the game started
    std::uint8_t pawns_left;  // Number of pawns still available
    GameState game_state;     // Logical game state (players, board, status)
} Game;

/**
 * Creates initial game state.
 *
 * @param game_id     Unique identifier of the game
 * @param max_pawns   Maximum number of pawns in the game
 * @param pawn_row    Bitset representing initial pawn positions
 * @return Initialized GameState structure
 */
GameState create_game_state(std::uint32_t game_id,
                      std::uint8_t max_pawns,
                      const std::vector<std::uint8_t>& pawn_row);

/**
 * Creates full game structure including timing and initial state.
 *
 * @param game_id     Unique identifier of the game
 * @param max_pawns   Maximum number of pawns
 * @param pawn_row    Bitset representing initial pawn positions
 * @param pawns_left  Number of pawns initially available
 * @return Fully initialized Game structure
 */
Game create_full_game(std::uint32_t game_id,
                      std::uint8_t max_pawn,
                      const std::vector<std::uint8_t>& pawn_row,
                      std::uint8_t pawns_left);

/**
 * Checks if a given player participates in the game.
 *
 * @param games       All games
 * @param player_id   Player identifier
 * @param game_id     Game identifier
 * @return true if player is part of the game, false otherwise
 */
bool check_my_game(const std::map<std::uint32_t, Game>& games, std::uint32_t player_id, std::uint32_t game_id);

/**
 * Checks if it is the given player's turn.
 *
 * @param game        Game instance
 * @param player_id   Player identifier
 * @return true if it is player's turn, false otherwise
 */
bool check_my_turn(const Game &game, std::uint32_t player_id);

/**
 * Adds a player to the game.
 * First player joins as A, second as B.
 *
 * @param game        Game instance
 * @param player_id   Player identifier
 * @return true if second player joined
 */
bool join_game(Game &game, std::uint32_t player_id);

/**
 * Performs move removing a single pawn.
 *
 * @param game        Game instance
 * @param pawn        Index of pawn to remove
 * @return true if move was successful, false otherwise
 */
bool make_move_1(Game &game, std::uint8_t pawn);

/**
 * Performs move removing two adjacent pawns.
 *
 * @param game        Game instance
 * @param pawn        Index of first pawn (second is pawn + 1)
 * @return true if move was successful, false otherwise
 */
bool make_move_2(Game &game, std::uint8_t pawn);

/**
 * Player resigns the game.
 * Opponent is declared the winner.
 *
 * @param game        Game instance
 */
void give_up(Game &game);

#endif