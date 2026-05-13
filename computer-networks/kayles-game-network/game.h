#ifndef GAME_H
#define GAME_H

#include <cstdint>
#include <ctime>
#include <vector>
#include <map>
#include <chrono>
#include <set>

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using ScheduledCheck = std::pair<TimePoint, std::uint32_t>;


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
    TimePoint next_check_time;                 // Timestamp when should it be next checked
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

/**
 * Schedules or reschedules a game in the timeout set.
 *
 * If the game already has a previously scheduled timeout check,
 * that entry is removed before inserting the new one. This guarantees
 * that the timeout set contains at most one active entry per game.
 *
 * @param games           Map of active games
 * @param timeouts        Ordered set of scheduled timeout checks
 * @param game_id         Identifier of the game to schedule
 * @param server_timeout  Timeout for inactivity
 */
void schedule_game(std::map<std::uint32_t, Game>& games,
                   std::set<ScheduledCheck>& timeouts,
                   std::uint32_t game_id,
                   std::time_t server_timeout);

/**
 * Verifies whether a game is still active and updates its state on timeout.
 *
 * The function applies timeout rules depending on the current state:
 *  - WAITING_FOR_OPPONENT: the game is removed if player A timed out
 *  - TURN_A / TURN_B: if one or both players timed out, the game is
 *    transitioned to the appropriate WIN_* state
 *  - WIN_A / WIN_B: the finished game is kept only for a limited
 *    retention period, after which it is removed
 *
 * If a timeout occurs during an active game, the winner is determined
 * according to the last player who remained active.
 *
 * @param game            Game to verify and possibly update
 * @param server_timeout  Timeout for inactivity
 * @return true if the game should remain on the server, false if it should be removed
 */
bool check_recent_activity(Game& game, std::time_t server_timeout);

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
 * Computes the next timeout check moment for a game.
 *
 * The returned time is the earliest point when the game may need
 * a state update or removal due to inactivity, depending on its state.
 *
 * @param game            Game to evaluate
 * @param server_timeout  Timeout duration
 * @return Next scheduled check time
 */
TimePoint compute_next_check_time(const Game& game, std::time_t server_timeout);

#endif