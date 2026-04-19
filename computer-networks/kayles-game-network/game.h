#ifndef GAME_H
#define GAME_H

#include <cstdint>
#include <ctime>
#include <vector>
#include <map>
#include <chrono>

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;


// ----------------------------
// Constants
// ----------------------------

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
 * Full game state used for synchronization.
 */
typedef struct {
    std::uint32_t game_id = 0;
    std::uint32_t player_a_id = 0;
    std::uint32_t player_b_id = 0;
    std::uint8_t status = 0;
    std::uint8_t max_pawn = 0;
    std::vector<std::uint8_t> pawn_row;
} GameState;

/** 
 * Represents full game state including timing and remaining pawns.
 */
typedef struct {
    TimePoint player_a_activity;             // Timestamp of last a activity
    TimePoint player_b_activity;             // Timestamp of last b activity
    TimePoint after_finish_activity;         // Timestamp of last activity including finish
    std::uint8_t pawns_left;                 // Number of pawns still available
    GameState game_state;                    // Logical game state (players, board, status)
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
 * Checks whether the game is still active based on player activity
 * and server-defined timeout.
 *
 * Handles different game states:
 * - WAITING_FOR_OPPONENT: if opponent does not join in time → game expires
 * - TURN_A / TURN_B: if active player is inactive → opponent wins
 * - WIN_A / WIN_B: after timeout → game can be cleaned up
 *
 * @param game             Game instance (modified if timeout occurs)
 * @param server_timeout   Maximum allowed inactivity time (in seconds)
 * @return true if game is still active, false if it should be terminated
 */
bool check_recent_activity(Game& game, time_t server_timeout);


/**
 * Updates the last activity timestamp for a player or finished game.
 *
 * - If the game is already finished (WIN_A / WIN_B), updates
 *   post-game activity timestamp.
 * - Otherwise updates activity timestamp for the corresponding player.
 *
 * @param game        Game instance (modified in-place)
 * @param player_id   Player identifier
 */
void update_activity_time(Game& game, uint32_t player_id);

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