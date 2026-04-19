#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <cinttypes>
#include <climits>
#include <netdb.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <map>
#include <vector>
#include <set>
#include <queue>
#include <ctime>

#include "err.h"
#include "common.h"
#include "protocol.h"
#include "game.h"

constexpr std::size_t BUFFER_SIZE = 100;
constexpr int MAX_TIME = 99;
constexpr std::size_t INPUT_LENGTH = 9;
constexpr std::uint8_t BYTE = 8;
constexpr std::uint32_t MAX_GAME_ID = UINT32_MAX;
constexpr std::size_t MAX_PAWN_ROW_LENGTH = 256;

/**
 * Structure holding parsed server input parameters.
 */
typedef struct {
    std::vector<std::uint8_t> pawn_row;   /**< Encoded pawn row as a bit vector */
    std::uint8_t max_pawn = 0;            /**< Highest valid pawn index */
    std::uint8_t pawns_left = 0;          /**< Number of pawns still present */
    struct sockaddr_in server_address = {}; /**< Resolved server address */
    std::uint16_t port = 0;               /**< Server port number */
    std::time_t server_timeout = 0;       /**< Timeout for inactive games */
} ServerInput;

/**
 * Parses command-line arguments into a ServerInput structure.
 *
 * Expected format:
 *   -r <pawn row> -a <address> -p <port> -t <timeout>
 *
 * @param argc  Number of command-line arguments
 * @param argv  Command-line arguments
 * @return Parsed ServerInput structure
 * @throws fatal() on invalid input
 */
ServerInput parse_server_input(int argc, char *argv[]) {
    if (argc < int(INPUT_LENGTH) || argc % 2 == 0) {
        fatal("usage: %s -r <pawn row> -a <address> -p <port> -t <server_timeout>\n", argv[0]);
    }

    ServerInput args;

    // ----------------------------
    // Check for all options
    // ----------------------------
    const char* port_str = nullptr;
    const char* addr_str = nullptr;
    const char* pawn_row_str = nullptr;
    const char* timeout_str = nullptr;

    for (int i = 1; i < argc; i += 2) {
        if (std::strlen(argv[i]) != 2 || argv[i][0] != '-') {
            fatal("wrong option format: %s", argv[i]);
        }

        char opt = argv[i][1];

        switch (opt) {
            case 'p': port_str = argv[i + 1]; break;
            case 'a': addr_str = argv[i + 1]; break;
            case 'r': pawn_row_str = argv[i + 1]; break;
            case 't': timeout_str = argv[i + 1]; break;
            default:
                fatal("unknown option: %s", argv[i]);
        }
    }

    if (!port_str) fatal("missing required option: -p");
    if (!addr_str) fatal("missing required option: -a");
    if (!pawn_row_str) fatal("missing required option: -r");
    if (!timeout_str) fatal("missing required option: -t");

    // ----------------------------
    // Validate option inputs
    // ----------------------------

    // ----------------------------
    // Validate and convert port
    // ----------------------------
    args.port = read_port(port_str);

    // ----------------------------
    // Resolve server address
    // ----------------------------
    args.server_address = get_server_address(addr_str, args.port);


    // ----------------------------
    // Parse and validate pawn_row
    // ----------------------------
    std::size_t len = std::strlen(pawn_row_str);
    if (len > MAX_PAWN_ROW_LENGTH || len == 0) {
        fatal("pawn row length out of range (1-%zu): %s", MAX_PAWN_ROW_LENGTH, pawn_row_str);
    }

    args.max_pawn = len - 1;

    std::size_t bytes = (args.max_pawn) / BYTE + 1;
    args.pawn_row.assign(bytes, 0);
    args.pawns_left = 0;

    for (std::size_t j = 0; j < len; j++) {
        if (pawn_row_str[j] == '1') {
            bitset_set(args.pawn_row, j, true);
            args.pawns_left += 1;
        } else if (pawn_row_str[j] != '0') {
            fatal("wrong format of pawn row: %s", pawn_row_str);
        }
    }

    if (pawn_row_str[0] != '1' || pawn_row_str[len - 1] != '1') {
        fatal("First and last pawn must be 1: %s", pawn_row_str);
    }

    // ----------------------------
    // Validate and convert timeout
    // ----------------------------
    uint32_t value = 0;
    if (!validate_number(timeout_str, value)) {
        fatal("wrong format of client timeout: %s", timeout_str);
    }
    if (value == 0 || value > MAX_TIME) {
        fatal("server timeout out of range (1-%u): %s", MAX_TIME, timeout_str);
    }
    args.server_timeout = value;

    return args;
}

/**
 * Removes inactive games from the server state.
 *
 * A game is erased if it has not had recent activity
 * within the configured timeout period.
 *
 * @param games           Map of active games
 * @param server_timeout  Timeout for inactivity
 */
void cleanup(std::map<std::uint32_t, Game>& games, std::time_t server_timeout) {
    auto it = games.begin();
    while (it != games.end()) {
        if (!check_recent_activity(it->second, server_timeout)) {
            it = games.erase(it);
        } else {
            it++;
        }
    }
}

/**
 * Checks whether a player belongs to the specified game.
 *
 * @param games      Map of active games
 * @param game_id    Identifier of the game
 * @param player_id  Identifier of the player
 * @return true if the player belongs to the game, false otherwise
 */
bool check_game(std::map<std::uint32_t, Game>& games, std::uint32_t game_id, std::uint32_t player_id) {
    auto it = games.find(game_id);
    if (it != games.end()) {
        return check_my_game(it->second, player_id);
    }
    return false;
}

/**
 * Entry point of the server application.
 *
 * Responsibilities:
 *  - Parse input arguments
 *  - Create and bind UDP socket
 *  - Receive and validate client messages
 *  - Update game state
 *  - Serialize and send server responses
 *
 * @param argc  Argument count
 * @param argv  Argument vector
 * @return 0 on success
 */
int main(int argc, char *argv[]) {
    // ----------------------------
    // Argument validation and input parsing
    // ----------------------------
    ServerInput args = parse_server_input(argc, argv);

    // ----------------------------
    // Server state initialization
    // ----------------------------
    std::map<std::uint32_t, Game> games;
    WrongMessage wrong_message;
    ClientMessage client_message;
    ServerResponse server_response;

    bool games_avaliable = true;
    std::uint32_t waiting_game_id = 0;
    std::uint32_t current_id = 0;

    // ----------------------------
    // Socket creation
    // ----------------------------
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    // ----------------------------
    // Socket binding
    // ----------------------------
    if (bind(socket_fd, (struct sockaddr *) &args.server_address, (socklen_t) sizeof(args.server_address)) < 0) {
        syserr("bind");
    }

    // ----------------------------
    // Buffer initialization
    // ----------------------------
    std::uint8_t buffer[BUFFER_SIZE];
    ssize_t received_length;

    do {
        // ----------------------------
        // Prepare buffer and client address
        // ----------------------------
        std::memset(buffer, 0, sizeof(buffer));
        struct sockaddr_in client_address;
        socklen_t address_length = (socklen_t) sizeof(client_address);

        // ----------------------------
        // Receive message from client
        // ----------------------------
        received_length = recvfrom(socket_fd, buffer, BUFFER_SIZE, 0,
                                   (struct sockaddr *) &client_address, &address_length);
        if (received_length < 0) {
            syserr("recvfrom");
        }

        std::uint8_t error_index = 0;

        // ----------------------------
        // Remove inactive games
        // ----------------------------
        cleanup(games, args.server_timeout);

        // ----------------------------
        // Message validation
        // ----------------------------
        if (!deserialize_client_message(client_message, buffer, received_length, error_index)) {
            // ----------------------------
            // Wrong client message
            // ----------------------------
            create_wrong_message(wrong_message,
                                std::span<const std::uint8_t>(buffer, BUFFER_SIZE),
                                error_index);
            server_response.response_type = MSG_WRONG_MESSAGE;
            server_response.response = wrong_message;
        }
        else if (client_message.msg_type != MSG_JOIN &&
                !check_game(games, client_message.game_id, client_message.player_id)) {
            // ----------------------------
            // No such game
            // ----------------------------
            handle_invalid_game(error_index);
            create_wrong_message(wrong_message,
                                std::span<const std::uint8_t>(buffer, BUFFER_SIZE),
                                error_index);
            server_response.response_type = MSG_WRONG_MESSAGE;
            server_response.response = wrong_message;
        }
        else {
            // ----------------------------
            // Valid message handling
            // ----------------------------
            server_response.response_type = MSG_CORRECT_MESSAGE;
            std::uint32_t target_game = 0;

            if (client_message.msg_type == MSG_JOIN) {
                auto it = games.find(waiting_game_id);

                if (it == games.end() || !(it->second.game_state.status == WAITING_FOR_OPPONENT)) {
                    try {
                        if (!games_avaliable){
                            continue;
                        }
                        if (current_id == MAX_GAME_ID) {
                            games_avaliable = false;
                        }

                        games.emplace(current_id, create_full_game(current_id, args.max_pawn,
                                                            args.pawn_row, args.pawns_left));
                        waiting_game_id = current_id;
                        current_id++;
                    } catch (const std::bad_alloc&) {
                        games_avaliable = true;
                        continue;
                    }
                }

                target_game = waiting_game_id;
                join_game(games[target_game], client_message.player_id);
                server_response.response = games[target_game].game_state;
            }
            else {
                target_game = client_message.game_id;

                // ----------------------------
                // Process player move
                // ----------------------------
                if (check_my_turn(games[target_game], client_message.player_id)) {
                    switch (client_message.msg_type) {
                        case MSG_MOVE_1:
                            make_move_1(games[target_game], client_message.pawn);
                            break;
                        case MSG_MOVE_2:
                            make_move_2(games[target_game], client_message.pawn);
                            break;
                        case MSG_GIVE_UP:
                            give_up(games[target_game]);
                            break;
                        default:
                            break;
                    }
                }

                server_response.response = games[target_game].game_state;
            }

            // ----------------------------
            // Update player activity time
            // ----------------------------
            update_activity_time(games[target_game], client_message.player_id);
        }

        // ----------------------------
        // Serialize response
        // ----------------------------
        std::memset(buffer, 0, sizeof(buffer));
        std::size_t send_length = serialize(server_response, std::span<std::uint8_t>(buffer, BUFFER_SIZE));

        // ----------------------------
        // Send response to client
        // ----------------------------
        ssize_t sent_length = sendto(socket_fd, buffer, send_length, 0,
                                    (struct sockaddr *) &client_address, address_length);
        if (sent_length < 0) {
            syserr("sendto");
        }
        else if ((std::size_t) sent_length != send_length) {
            fatal("incomplete sending");
        }

    } while (true);

    // ----------------------------
    // Cleanup
    // ----------------------------
    close(socket_fd);
    return 0;
}