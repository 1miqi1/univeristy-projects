#include <iostream>
#include <cstdint>
#include <ctime>
#include <vector>
#include <map>
#include <chrono>

#include "common.h"
#include "err.h"
#include "game.h"

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

constexpr std::size_t BYTE_SIZE = 8;

// Creates basic logical game state.
// Initializes players as not joined yet and sets initial board layout.
GameState create_game_state(std::uint32_t game_id,
                      std::uint8_t max_pawn,
                      const std::vector<std::uint8_t>& pawn_row) {
    GameState game_state{};
    game_state.game_id = game_id;
    game_state.player_a_id = 0;
    game_state.player_b_id = 0;
    game_state.status = 0;
    game_state.max_pawn = max_pawn;
    game_state.pawn_row = pawn_row;
    return game_state;
}

// Creates full game object with start time, remaining pawns counter,
// and initialized logical state.
Game create_full_game(std::uint32_t game_id,
                      std::uint8_t max_pawn,
                      const std::vector<std::uint8_t>& pawn_row,
                      std::uint8_t pawns_left) {
    Game game{};
    game.player_a_activity = Clock::now();
    game.player_b_activity = Clock::now();
    game.next_check_time = Clock::now();
    game.pawns_left = pawns_left;
    game.game_state = create_game_state(game_id, max_pawn, pawn_row);
    return game;
}

// Checks whether given player belongs to this game as player A or B.
bool check_my_game(const Game& game, std::uint32_t player_id){
    return game.game_state.player_a_id == player_id ||
           game.game_state.player_b_id == player_id;
}

// Checks whether it is currently the given player's turn.
// Only TURN_A and TURN_B are valid active-turn states.
bool check_my_turn(const Game& game, std::uint32_t player_id) {
    switch (game.game_state.status) {
        case TURN_A:
            return game.game_state.player_a_id == player_id;
        case TURN_B:
            return game.game_state.player_b_id == player_id;
        default:
            return false;
    }
}


// Adds player to the game.
// First joining player becomes A.
// Second joining player becomes B and starts the game with player B's turn.
bool join_game(Game& game, std::uint32_t player_id) {
    if (game.game_state.player_a_id == 0) {
        game.game_state.player_a_id = player_id;
        return false;
    } else {
        game.game_state.player_b_id = player_id;
        game.game_state.status = TURN_B;
        return true;
    }
}

// Removes one pawn if the selected pawn exists and is still available.
// After a valid move, updates remaining pawns and either:
// - declares winner if no pawns are left,
// - or switches turn to the other player.
bool make_move_1(Game& game, std::uint8_t pawn) {
    std::uint8_t bit = 0;

    // Reject move if pawn index is outside allowed game range.
    if (pawn > game.game_state.max_pawn) {
        return false;
    }

    // Read pawn availability from bitset.
    bitset_get(game.game_state.pawn_row, pawn, bit);
    if (!bit) {
        return false;
    }

    // Clear selected pawn from board.
    bitset_set(game.game_state.pawn_row, pawn, false);
    game.pawns_left -= 1;

    // If last pawn was removed, current player wins.
    // Otherwise pass turn to the other player.
    if (game.pawns_left == 0) {
        if (game.game_state.status == TURN_A) {
            game.game_state.status = WIN_A;
        } else {
            game.game_state.status = WIN_B;
        }
        game.after_finish_activity = Clock::now();
    } else {
        if (game.game_state.status == TURN_A) {
            game.game_state.status = TURN_B;
        } else {
            game.game_state.status = TURN_A;
        }
    }

    return true;
}

// Removes two adjacent pawns if both exist and are available.
// 'pawn' is the first pawn, the second one is 'pawn + 1'.
// After a valid move, updates remaining pawns and game status.
bool make_move_2(Game& game, std::uint8_t pawn) {
    std::uint8_t bit1 = 0;
    std::uint8_t bit2 = 0;

    // Reject move if second pawn would go outside allowed range.
    if (pawn >= game.game_state.max_pawn) {
        return false;
    }

    // Read availability of both adjacent pawns.
    bitset_get(game.game_state.pawn_row, pawn, bit1);
    bitset_get(game.game_state.pawn_row, pawn + 1, bit2);

    // Both pawns must be present to perform this move.
    if (bit1 + bit2 < 2) {
        return false;
    }

    // Clear both pawns from board.
    bitset_set(game.game_state.pawn_row, pawn, false);
    bitset_set(game.game_state.pawn_row, pawn + 1, false);
    game.pawns_left -= 2;

    // If last pawns were removed, current player wins.
    // Otherwise pass turn to the other player.
    if (game.pawns_left == 0) {
        if (game.game_state.status == TURN_A) {
            game.game_state.status = WIN_A;
        } else {
            game.game_state.status = WIN_B;
        }
    } else {
        if (game.game_state.status == TURN_A) {
            game.game_state.status = TURN_B;
        } else {
            game.game_state.status = TURN_A;
        }
    }

    return true;
}

// Handles resignation.
// If current player gives up, the other player wins immediately.
void give_up(Game& game) {
    if (game.game_state.status == TURN_A) {
        game.game_state.status = WIN_B;
    } else if (game.game_state.status == TURN_B) {
        game.game_state.status = WIN_A;
    }
}


// Computes time when server should check the state of the game
TimePoint compute_next_check_time(const Game& game, std::time_t server_timeout) {
    auto timeout = std::chrono::seconds(server_timeout);

    if (game.game_state.status == WAITING_FOR_OPPONENT) {
        return game.player_a_activity + timeout;
    }

    if (game.game_state.status == TURN_A || game.game_state.status == TURN_B) {
        return std::min(game.player_a_activity + timeout,
                        game.player_b_activity + timeout);
    }

    if (game.game_state.status == WIN_A || game.game_state.status == WIN_B) {
        return game.after_finish_activity + timeout;
    }

    return Clock::now() + timeout;
}

// Schedules or reschedules a game in the timeout set.
void schedule_game(std::map<std::uint32_t, Game>& games,
                   std::set<ScheduledCheck>& timeouts,
                   std::uint32_t game_id,
                   std::time_t server_timeout) {
    auto it = games.find(game_id);
    if (it == games.end()) {
        return;
    }

    timeouts.erase({it->second.next_check_time, game_id});
    it->second.next_check_time = compute_next_check_time(it->second, server_timeout);
    timeouts.insert({it->second.next_check_time, game_id});
}

//Verifies whether a game is still active and updates its state on timeout.
bool check_recent_activity(Game& game, std::time_t server_timeout) {
    auto now = Clock::now();
    auto timeout = std::chrono::seconds(server_timeout);

    if (game.game_state.status == WAITING_FOR_OPPONENT) {
        if (now - game.player_a_activity >= timeout) {
            return false;
        }
    }
    else if (game.game_state.status == TURN_A || game.game_state.status == TURN_B) {
        bool a_timed_out = (now - game.player_a_activity >= timeout);
        bool b_timed_out = (now - game.player_b_activity >= timeout);

        if (a_timed_out && b_timed_out) {
            if (game.player_a_activity > game.player_b_activity) {
                game.game_state.status = WIN_A;
                game.after_finish_activity = game.player_b_activity + timeout;
            } else {
                game.game_state.status = WIN_B;
                game.after_finish_activity = game.player_a_activity + timeout;
            }
        }
        else if (a_timed_out) {
            game.game_state.status = WIN_B;
            game.after_finish_activity = game.player_a_activity + timeout;
        }
        else if (b_timed_out) {
            game.game_state.status = WIN_A;
            game.after_finish_activity = game.player_b_activity + timeout;
        }
    }

    if (game.game_state.status == WIN_A || game.game_state.status == WIN_B) {
        if (now - game.after_finish_activity >= timeout) {
            return false;
        }
    }

    return true;
}

// Updates last time of activity
void update_activity_time(Game& game, uint32_t player_id) {
    auto now = Clock::now();

    if (game.game_state.status == WIN_A || game.game_state.status == WIN_B) {
        game.after_finish_activity = now;
    }

    if (game.game_state.player_a_id == player_id) {
        game.player_a_activity = now;
    }

    if (game.game_state.player_b_id == player_id) {
        game.player_b_activity = now;
    }
}

