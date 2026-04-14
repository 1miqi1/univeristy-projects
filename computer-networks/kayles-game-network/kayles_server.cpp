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

constexpr std::size_t BUFFER_SIZE = 1024;
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
    ServerInput args;
    char *host = NULL;
    std::set<char> flags;

    if (argc < int(INPUT_LENGTH)) {
        fatal("usage: %s -r <pawn row> -a <address> -p <port> -t <client_timeout>\n", argv[0]);
    }

    // ----------------------------
    // Parse port first
    // ----------------------------
    for (int i = 1; i + 1 < argc; i += 2) {
        if (std::strlen(argv[i]) < 2 || argv[i][0] != '-') {
            fatal("wrong input: %s", argv[i]);
        }
        if (argv[i][1] == 'p') {
            args.port = read_port(argv[i + 1]);
            flags.insert('p');
        }
    }

    if (!flags.count('p')) {
        fatal("missing required option: -p");
    }

    // ----------------------------
    // Parse remaining arguments
    // ----------------------------
    for (int i = 1; i + 1 < argc; i += 2) {
        if (flags.count(argv[i][1])) {
            continue;
        }

        flags.insert(argv[i][1]);
        switch (argv[i][1]) {
            case 'p':
                break;

            case 'a': {
                host = argv[i + 1];
                args.server_address = get_server_address(host, args.port);
                break;
            }

            case 'r': {
                std::size_t len = std::strlen(argv[i + 1]);
                if (len > MAX_PAWN_ROW_LENGTH || len == 0) {
                    fatal("pawn row length out of range (1-%zu): %s", MAX_PAWN_ROW_LENGTH, argv[i + 1]);
                }

                args.max_pawn = len - 1;

                std::size_t bytes = (args.max_pawn) / BYTE + 1;
                args.pawn_row.assign(bytes, 0);
                args.pawns_left = 0;

                for (std::size_t j = 0; j < len; j++) {
                    if (argv[i + 1][j] == '1') {
                        bitset_set(args.pawn_row, j, true);
                        args.pawns_left += 1;
                    } else if (argv[i + 1][j] != '0') {
                        fatal("wrong format of pawn row: %s", argv[i + 1]);
                    }
                }

                if (argv[i + 1][0] != '1' || argv[i + 1][len - 1] != '1') {
                    fatal("First and last pawn must be 1: %s", argv[i + 1]);
                }
                break;
            }

            case 't': {
                std::uint32_t value;
                if (!validate_number(argv[i + 1], value)) {
                    fatal("wrong format of client timeout: %s", argv[i + 1]);
                }

                if (value == 0 || value > MAX_TIME) {
                    fatal("client timeout out of range (1-%u): %s", MAX_TIME, argv[i + 1]);
                }

                args.server_timeout = value;
                break;
            }

            default:
                fatal("unknown option: %s", argv[i]);
        }
    }

    if (!(flags.count('p') && flags.count('a') && flags.count('t') && flags.count('r'))) {
        fatal("not all options provided");
    }

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

    uint32_t waiting_game_id = 0;
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
            create_wrong_message(wrong_message,
                                 std::span<const std::uint8_t>(buffer, BUFFER_SIZE),
                                 error_index);
            server_response.response_type = MSG_WRONG_MESSAGE;
            server_response.response = wrong_message;
        }
        else if (client_message.msg_type != MSG_JOIN &&
                 !check_game(games, client_message.game_id, client_message.player_id)) {
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
                        if (current_id == MAX_GAME_ID) {
                            continue;
                        }

                        games[current_id] = create_full_game(current_id, args.max_pawn,
                                                             args.pawn_row, args.pawns_left);
                        waiting_game_id = current_id;
                        current_id++;
                    } catch (const std::bad_alloc&) {
                        continue;
                    }
                }

                std::cout << "target game" << waiting_game_id << "\n";
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