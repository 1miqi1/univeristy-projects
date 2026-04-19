#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <ranges>
#include <span>
#include <algorithm>

#include <arpa/inet.h>

#include "protocol.h"
#include "common.h"
#include "err.h"

constexpr std::size_t MAX_MESSAGE_SIZE = 4;
constexpr std::size_t MIN_MESSAGE_SIZE = 2;
constexpr std::size_t BYTE_SIZE = 8;
constexpr std::size_t MAX_MESSAGE_TYPE = 4;
constexpr std::size_t MAX_CLIENT_INPUT_MESSAGE_SIZE = 100;

static const std::size_t message_sizes[] = {
    MSG_JOIN_LENGTH,
    MSG_MOVE_1_LENGTH,
    MSG_MOVE_2_LENGTH,
    MSG_KEEP_ALIVE_LENGTH,
    MSG_GIVE_UP_LENGTH
};


/**
 * --- PARSING LOGIC ---
 */

ParseResult parse_client_message(const std::string& text, ClientMessage& msg) {
    if (text.empty()) {
        return {PARSE_ERR_INPUT, "Empty string"};
    }

    if (text.size() > MAX_CLIENT_INPUT_MESSAGE_SIZE) return {PARSE_ERR_INPUT, "Text too long"};

    std::vector<std::uint32_t> text_split;

    for (auto part : text | std::views::split('/')) {
        if (text_split.size() >= MAX_MESSAGE_SIZE) {
            return {PARSE_ERR_INPUT, "Too many fields"};
        }
        std::uint32_t value = 0;
        std::string token(part.begin(), part.end());

        if (!validate_number(token, value)) {
            return {PARSE_ERR_INPUT, "Fields must be non-negative integers"};
        }

        text_split.push_back(value);
    }

    const std::size_t count = text_split.size();


    if (count < MIN_MESSAGE_SIZE) {
        return {PARSE_ERR_INPUT, "Message too short: requires type/player_id"};
    }

    if(text_split[TYPE] > MAX_MESSAGE_SIZE){
        return {PARSE_ERR_INPUT, "Message type out of range"};
    }

    msg.msg_type = static_cast<std::uint8_t>(text_split[TYPE]);
    
    msg.player_id = text_split[PLAYER_ID];
    if(msg.player_id == 0){
        return {PARSE_ERR_INPUT, "Player index can't be 0"};
    }

    msg.game_id = 0;
    msg.pawn = 0;

    switch (msg.msg_type) {
        case MSG_JOIN:
            if (count != MSG_JOIN_FIELDS) {
                return {PARSE_ERR_INPUT, "JOIN requires exactly: 0/player_id"};
            }
            break;

        case MSG_MOVE_1:
        case MSG_MOVE_2:
            if (count != MSG_MOVE_2_FIELDS) {
                return {PARSE_ERR_INPUT, "MOVE requires: type/player_id/game_id/pawn"};
            }
            if (text_split[PAWN] > MAX_PAWN_RANGE) { // Replaced MAX_UINT8 for standard clarity
                return {PARSE_ERR_INPUT, "Pawn index out of range (0-255)"};
            }
            msg.game_id = text_split[GAME_ID];
            msg.pawn = static_cast<std::uint8_t>(text_split[PAWN]);
            break;

        case MSG_KEEP_ALIVE:
        case MSG_GIVE_UP:
            if (count != MSG_KEEP_ALIVE_FIELDS) {
                return {PARSE_ERR_INPUT, "Utility msg requires: type/player_id/game_id"};
            }
            msg.game_id = text_split[GAME_ID];
            break;

        default:
            return {PARSE_ERR_INPUT, "Unknown message type (0-4)"};
    }

    return {PARSE_OK, "Success"};
}

std::string parse_error_string(ParseResult res) {
    return res.msg;
}

/**
 * --- HELPER FUNCTIONS ---
 */

std::size_t get_client_message_size(const ClientMessage& msg) {
    if (msg.msg_type >= std::size(message_sizes)) {
        return 0;
    }
    return message_sizes[msg.msg_type];
}

void handle_invalid_game(std::uint8_t& error_index) {
    error_index = GAME_ID_INDEX;
}

void create_wrong_message(WrongMessage& wrong_message,
                          std::span<const std::uint8_t> buf,
                          std::uint8_t error_index) {
    const std::size_t size = std::min(buf.size(), CLIENT_MESSAGE_PREFIX_SIZE);
    std::memcpy(wrong_message.client_message_prefix, buf.data(), size);

    if (size < CLIENT_MESSAGE_PREFIX_SIZE) {
        std::memset(wrong_message.client_message_prefix + size,
                    0,
                    CLIENT_MESSAGE_PREFIX_SIZE - size);
    }

    wrong_message.status = WRONG_MESSAGE_STATUS;
    wrong_message.error_index = error_index;
}

/**
 * --- SERIALIZATION (Writing to span) ---
 */

std::size_t serialize_client_message(const ClientMessage& msg, std::span<std::uint8_t> buf) {
    const std::size_t size = get_client_message_size(msg);
    if (size == 0 || buf.size() < size) {
        return 0;
    }

    std::size_t offset = 0;
    buf[offset++] = msg.msg_type;

    const std::uint32_t net_player_id = htonl(msg.player_id);
    std::memcpy(buf.data() + offset, &net_player_id, sizeof(net_player_id));
    offset += sizeof(net_player_id);

    if (msg.msg_type == MSG_MOVE_1 || msg.msg_type == MSG_MOVE_2 ||
        msg.msg_type == MSG_KEEP_ALIVE || msg.msg_type == MSG_GIVE_UP) {
        const std::uint32_t net_game_id = htonl(msg.game_id);
        std::memcpy(buf.data() + offset, &net_game_id, sizeof(net_game_id));
        offset += sizeof(net_game_id);
    }

    if (msg.msg_type == MSG_MOVE_1 || msg.msg_type == MSG_MOVE_2) {
        buf[offset++] = msg.pawn;
    }

    return offset;
}

std::size_t serialize(const GameState& game_state, std::span<std::uint8_t> buf) {
    const std::size_t pawn_bytes = (game_state.max_pawn / BYTE_SIZE) + 1;
    const std::size_t total_size = GAME_STATE_HEADER_SIZE + pawn_bytes;

    if (buf.size() < total_size) {
        return 0;
    }

    std::size_t offset = 0;

    const std::uint32_t net_game_id = htonl(game_state.game_id);
    std::memcpy(buf.data() + offset, &net_game_id, sizeof(net_game_id));
    offset += sizeof(net_game_id);

    const std::uint32_t net_player_a_id = htonl(game_state.player_a_id);
    std::memcpy(buf.data() + offset, &net_player_a_id, sizeof(net_player_a_id));
    offset += sizeof(net_player_a_id);

    const std::uint32_t net_player_b_id = htonl(game_state.player_b_id);
    std::memcpy(buf.data() + offset, &net_player_b_id, sizeof(net_player_b_id));
    offset += sizeof(net_player_b_id);

    buf[offset++] = game_state.status;
    buf[offset++] = game_state.max_pawn;

    std::memcpy(buf.data() + offset, game_state.pawn_row.data(), pawn_bytes);
    offset += pawn_bytes;

    return offset;
}

std::size_t serialize(const WrongMessage& wrong_message, std::span<std::uint8_t> buf) {
    const std::size_t total_size = MSG_WRONG_MESSAGE_LENGTH;
    if (buf.size() < total_size) {
        return 0;
    }

    std::size_t offset = 0;
    std::memcpy(buf.data(), wrong_message.client_message_prefix, CLIENT_MESSAGE_PREFIX_SIZE);
    offset += CLIENT_MESSAGE_PREFIX_SIZE;

    buf[offset++] = wrong_message.status;
    buf[offset++] = wrong_message.error_index;

    return offset;
}

std::size_t serialize(const ServerResponse& server_response, std::span<std::uint8_t> buf) {
    return std::visit([buf](const auto& response) -> std::size_t {
        return serialize(response, buf);
    }, server_response.response);
}

/**
 * --- DESERIALIZATION (Reading from span) ---
 */

bool deserialize_client_message(ClientMessage& msg,
                                std::span<const std::uint8_t> buf,
                                std::size_t recieved_length,
                                std::uint8_t& error_index) {
    error_index = 0;
    if (!recieved_length) return false;

    std::size_t offset = 0;
    msg.msg_type = buf[offset++];

    switch (msg.msg_type) {
        case MSG_JOIN: {
            if (recieved_length != MSG_JOIN_LENGTH) {
                error_index = static_cast<std::uint8_t>(std::min( recieved_length, (std::size_t)MSG_JOIN_LENGTH));
                return false;
            }

            std::uint32_t net_player_id = 0;
            std::memcpy(&net_player_id, buf.data() + offset, sizeof(net_player_id));
            msg.player_id = ntohl(net_player_id);
            msg.game_id = 0;
            msg.pawn = 0;

            if (msg.player_id == 0) {
                error_index = 1;
                return false;
            }
            break;
        }

        case MSG_MOVE_1:
        case MSG_MOVE_2: {
            const std::size_t expected = (msg.msg_type == MSG_MOVE_1) ? MSG_MOVE_1_LENGTH : MSG_MOVE_2_LENGTH;
            if (recieved_length != expected) {
                error_index = static_cast<std::uint8_t>(std::min(recieved_length, expected));
                return false;
            }

            std::uint32_t net_player_id = 0;
            std::uint32_t net_game_id = 0;

            std::memcpy(&net_player_id, buf.data() + offset, sizeof(net_player_id));
            offset += sizeof(net_player_id);
            std::memcpy(&net_game_id, buf.data() + offset, sizeof(net_game_id));
            offset += sizeof(net_game_id);

            msg.player_id = ntohl(net_player_id);
            msg.game_id = ntohl(net_game_id);
            msg.pawn = buf[offset];
            if (msg.player_id == 0) {
                error_index = 1;
                return false;
            }
            break;
        }

        case MSG_KEEP_ALIVE:
        case MSG_GIVE_UP: {
            const std::size_t expected = (msg.msg_type == MSG_KEEP_ALIVE) ? MSG_KEEP_ALIVE_LENGTH : MSG_GIVE_UP_LENGTH;
            if (recieved_length != expected) {
                error_index = static_cast<std::uint8_t>(std::min(recieved_length, expected));
                return false;
            }

            std::uint32_t net_player_id = 0;
            std::uint32_t net_game_id = 0;

            std::memcpy(&net_player_id, buf.data() + offset, sizeof(net_player_id));
            offset += sizeof(net_player_id);
            std::memcpy(&net_game_id, buf.data() + offset, sizeof(net_game_id));
            offset += sizeof(net_game_id);

            msg.player_id = ntohl(net_player_id);
            msg.game_id = ntohl(net_game_id);
            msg.pawn = 0;
            if (msg.player_id == 0) {
                error_index = 1;
                return false;
            }
            break;
        }

        default:
            return false;
    }
    return true;
}

bool deserialize(GameState& game_state, std::span<const std::uint8_t> buf) {
    if (buf.size() < GAME_STATE_HEADER_SIZE) return false;

    std::size_t offset = 0;
    std::uint32_t net_game_id, net_player_a_id, net_player_b_id;

    std::memcpy(&net_game_id, buf.data() + offset, sizeof(net_game_id));
    game_state.game_id = ntohl(net_game_id);
    offset += sizeof(net_game_id);

    std::memcpy(&net_player_a_id, buf.data() + offset, sizeof(net_player_a_id));
    game_state.player_a_id = ntohl(net_player_a_id);
    offset += sizeof(net_player_a_id);

    std::memcpy(&net_player_b_id, buf.data() + offset, sizeof(net_player_b_id));
    game_state.player_b_id = ntohl(net_player_b_id);
    offset += sizeof(net_player_b_id);

    game_state.status = buf[offset++];
    game_state.max_pawn = buf[offset++];

    const std::size_t pawn_bytes = (game_state.max_pawn / BYTE_SIZE) + 1;
    if (buf.size() < offset + pawn_bytes) return false;

    game_state.pawn_row.resize(pawn_bytes);
    std::memcpy(game_state.pawn_row.data(), buf.data() + offset, pawn_bytes);

    return true;
}

bool deserialize(WrongMessage& wrong_message, std::span<const std::uint8_t> buf) {
    if (buf.size() < CLIENT_MESSAGE_PREFIX_SIZE + 2) return false;

    std::memcpy(wrong_message.client_message_prefix, buf.data(), CLIENT_MESSAGE_PREFIX_SIZE);
    wrong_message.status = buf[CLIENT_MESSAGE_PREFIX_SIZE];
    wrong_message.error_index = buf[CLIENT_MESSAGE_PREFIX_SIZE + 1];

    return true;
}

bool deserialize(ServerResponse& msg, std::span<const std::uint8_t> buf) {
    if (buf.size() < CLIENT_MESSAGE_PREFIX_SIZE + 1){
        return false;
    }

    if (buf[CLIENT_MESSAGE_PREFIX_SIZE] == WRONG_MESSAGE_STATUS) {
        WrongMessage wm;
        if (!deserialize(wm, buf)){
            return false;
        }
        msg.response_type = MSG_WRONG_MESSAGE;
        msg.response = wm;
    } else {
        GameState gs;
        if (!deserialize(gs, buf)){
            return false;
        }
        msg.response_type = MSG_CORRECT_MESSAGE;
        msg.response = gs;
    }
    return true;
}

/**
 * --- PRINTING LOGIC ---
 */

void print(const ClientMessage& msg) {
    std::cout << "ClientMessage {\n"
              << "  msg_type  : " << static_cast<unsigned>(msg.msg_type) << "\n"
              << "  player_id : " << msg.player_id << "\n"
              << "  game_id   : " << msg.game_id << "\n"
              << "  pawn      : " << static_cast<unsigned>(msg.pawn) << "\n"
              << "}\n";
}

void print(const WrongMessage& wm) {
    std::cout << "WrongMessage {\n  client message prefix : ";
    for (std::size_t i = 0; i < CLIENT_MESSAGE_PREFIX_SIZE; ++i)
        std::cout << static_cast<unsigned>(wm.client_message_prefix[i]) << " ";
    std::cout << "\n  status                : " << static_cast<unsigned>(wm.status) 
              << "\n  error index           : " << static_cast<unsigned>(wm.error_index) << "\n}\n";
}

void print(const GameState& gs) {
    std::cout << "GameState {\n  game id      : " << gs.game_id 
              << "\n  player A id  : " << gs.player_a_id 
              << "\n  player B id  : " << gs.player_b_id 
              << "\n  status       : " << static_cast<unsigned>(gs.status) 
              << "\n  max pawn     : " << static_cast<unsigned>(gs.max_pawn) 
              << "\n  pawn row     : ";
    for (std::size_t pawn = 0; pawn <= gs.max_pawn; ++pawn) {
        uint8_t bit;
        bitset_get(gs.pawn_row, pawn, bit);
        std::cout << static_cast<unsigned>(bit);
    }
    std::cout << "\n}\n";
}

void print(const ServerResponse& server_response) {
    std::visit([](const auto& response) { print(response); }, server_response.response);
}