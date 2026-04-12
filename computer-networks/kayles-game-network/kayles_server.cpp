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

using namespace std;

#include "err.h"
#include "common.h"
#include "protocol.h"
#include "game.h"

#define BUFFER_SIZE 12
#define MAX_TIME  99
#define INPUT_LENGTH 9 
#define MAX_GAME_ID UINT32_MAX // Fixed bitshift precedence bug
#define MAX_PAWN_ROW_LENGTH  256

typedef struct {
    vector<uint8_t> pawn_row;
    uint8_t max_pawn = 0;
    uint8_t pawns_left = 0;
    struct sockaddr_in server_address = {};
    uint16_t port = 0;
    uint32_t server_timeout = 0;
} ServerInput;

ServerInput parse_server_input(int argc, char *argv[]) {
    ServerInput args;
    char *host = NULL;
    bool port_found = false;

    if (argc != INPUT_LENGTH) {
        fatal("usage: %s -r <pawn row> -a <address> -p <port> -t <client_timeout>\n", argv[0]);
    }

    // 1. Find the port first
    for (int i = 1; i < INPUT_LENGTH; i += 2) {
        if (std::strlen(argv[i]) < 2 || argv[i][0] != '-') {
            fatal("wrong input: %s", argv[i]);
        }
        if (argv[i][1] == 'p') {
            args.port = read_port(argv[i + 1]);
            port_found = true;
            break;
        }
    }

    if (!port_found) {
        fatal("missing required option: -p");
    }

    // 2. Parse remaining arguments
    for (int i = 1; i < INPUT_LENGTH; i += 2) {
        switch (argv[i][1]) {
            case 'p':
                break; // Handled above

            case 'a': {
                host = argv[i + 1];
                args.server_address = get_server_address(host, args.port);
                break;
            }

            case 'r': {
                size_t len = strlen(argv[i+1]);
                if(len > MAX_PAWN_ROW_LENGTH || len == 0) {
                    fatal("pawn row length out of range (1-%u): %s", MAX_PAWN_ROW_LENGTH, argv[i+1]);
                }
                
                args.max_pawn = len; // Removed the -1, assuming length represents total pawns

                // Allocate exactly the number of bytes needed for bits, initialized to 0
                size_t bytes = (len + 7) / 8;
                args.pawn_row.assign(bytes, 0); 
                args.pawns_left = 0;

                for(size_t j = 0; j < len; j++) {
                    if(argv[i+1][j] == '1') {
                        // Pack bits safely: j is the correct index, NOT i!
                        args.pawn_row[j / 8] |= (1 << (j % 8)); 
                        args.pawns_left += 1;
                    } else if(argv[i+1][j] != '0') {
                        // vector cleans up its own memory on fatal exit
                        fatal("wrong format of pawn row: %s", argv[i+1]);
                    }
                }
                if(args.pawns_left == 0) {
                    fatal("there should be at least one '1' in row: %s", argv[i+1]);
                }
                break;
            }

            case 't': {
                uint32_t value;
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
    return args;
}


int main(int argc, char *argv[]) {
    // Pass argc and argv to the updated parser
    ServerInput args = parse_server_input(argc, argv); 

    map<uint32_t, Game> games; // Use uint32_t to match your game ID limits
    WrongMessage wrong_message;
    ClientMessage client_message;
    ServerResponse server_response;
    
    bool has_waiting_game = false; // Renamed from empty_game for clarity
    size_t current_id = 0;

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    if (bind(socket_fd, (struct sockaddr *) &args.server_address, (socklen_t) sizeof(args.server_address)) < 0) {
        syserr("bind");
    }

    cout << "listening on port" << args.port << "\n";
    cout << "HELLO\n";

    uint8_t buffer[BUFFER_SIZE]; // Declare once outside the loop to avoid redeclaration issues
    ssize_t received_length;

    do {
        cout << "HELLO";
        memset(buffer, 0, sizeof(buffer)); 
        struct sockaddr_in client_address;
        socklen_t address_length = (socklen_t) sizeof(client_address);

        received_length = recvfrom(socket_fd, buffer, BUFFER_SIZE, 0,
                                   (struct sockaddr *) &client_address, &address_length);
        if (received_length < 0) {
            syserr("recvfrom");
        }

        
        uint8_t error_index = 0;
        cout << "HELLO\n";
        
        // 1. Message Validation
        if (!deserialize_client_message(client_message, buffer, received_length, error_index)) {
            cout << "Deserialized" << error_index << "\n";
            create_wrong_message(wrong_message,
                     std::span<const std::uint8_t>(buffer, BUFFER_SIZE),
                     error_index);
            server_response.response_type = MSG_WRONG_MESSAGE;
            server_response.response = wrong_message;
        } 
        else if (!check_my_game(games, client_message.game_id, client_message.player_id)) {
            cout << "wrong game\n";
            handle_invalid_game(error_index);
            create_wrong_message(wrong_message,
                     std::span<const std::uint8_t>(buffer, BUFFER_SIZE),
                     error_index);
            server_response.response_type = MSG_WRONG_MESSAGE; 
            server_response.response = wrong_message;
        }
        else {
            print(client_message);
            if (client_message.msg_type == MSG_JOIN) {
                if (!has_waiting_game) {
                    if (current_id == MAX_GAME_ID) {
                        continue; // Reached maximum game limit
                    }
                    current_id++;
                    games[current_id] = create_full_game(client_message.game_id, args.max_pawn, args.pawn_row, args.pawns_left);
                    has_waiting_game = true;
                } else {
                    // A second player joined, the game is no longer waiting
                    has_waiting_game = false; 
                }
                join_game(games[current_id], client_message.player_id);
                server_response.response = games[current_id].game_state;
            } 
            else {
                // Must use the game_id from the client, NOT the current_id of the server!
                uint32_t target_game = client_message.game_id; 
                
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
            server_response.response_type = MSG_CORRECT_MESSAGE;
        }

        // 3. Serialize and Send
        memset(buffer, 0, sizeof(buffer));
        size_t send_length = serialize(server_response, std::span<std::uint8_t>(buffer, BUFFER_SIZE));

        ssize_t sent_length = sendto(socket_fd, buffer, send_length, 0,
                                     (struct sockaddr *) &client_address, address_length);
        if (sent_length < 0) {
            syserr("sendto");
        } 
        else if ((size_t)sent_length != send_length) { // Check against what we tried to SEND, not what we received
            fatal("incomplete sending");
        }

    } while (true);

    close(socket_fd);
    return 0;
}