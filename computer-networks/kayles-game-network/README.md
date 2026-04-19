# 🎯 Kayles Network Game (UDP)

![Language](https://img.shields.io/badge/language-C%2FC%2B%2B-blue)
![Build](https://img.shields.io/badge/build-Makefile-green)
![Protocol](https://img.shields.io/badge/protocol-UDP-orange)
![Architecture](https://img.shields.io/badge/architecture-single--threaded-lightgrey)
![Status](https://img.shields.io/badge/status-in%20progress-yellow)

A **networked implementation of the Kayles game** using **UDP sockets**.
The project includes a **server handling multiple concurrent games** and a **client for player interaction**.

---

## 🧠 About the Game

**Kayles** is a simple two-player game:

* Remove:

  * 🔹 one pawn
  * 🔹 or two adjacent pawns
* 🎯 The player who removes the **last pawn wins**

### 🔧 Custom Feature

This implementation allows a **custom initial pawn layout**, defined at server startup.

---

## 🏗 Architecture

```text
Player → Client → Server → Game Logic
```

### Key Properties

* ♟ Multiple games handled simultaneously
* 👥 Players can join multiple games
* 🔁 Players can control both sides
* 🌐 Communication via **UDP (IPv4)**
* ⚙ Server = **event-driven state machine (single-threaded)**

---

## 📦 Project Structure

```
kayles/
├── kayles_server.cpp     # UDP server, game manager
├── kayles_client.cpp     # Client interface
├── game.cpp              # Core game logic
├── game.h
├── protocol.cpp          # Serialization/deserialization
├── protocol.h
├── common.cpp            # Utilities (parsing, bitmaps)
├── common.h
├── err.cpp               # Error handling
├── err.h
├── Makefile
├── test_kayels.py
├── test_kayels2.py
```

---

## 🚀 Features

* ✅ Custom binary application-layer protocol
* ✅ Safe parsing & strict validation
* ✅ Bit-level board representation (bitmap)
* ✅ Multiplayer support over UDP
* ✅ Timeout handling (client & server)

---

## 📡 Protocol Overview

### Client → Server

| Message          | Description             |
| ---------------- | ----------------------- |
| `MSG_JOIN`       | Join or create a game   |
| `MSG_MOVE_1`     | Remove 1 pawn           |
| `MSG_MOVE_2`     | Remove 2 adjacent pawns |
| `MSG_KEEP_ALIVE` | Maintain presence       |
| `MSG_GIVE_UP`    | Forfeit game            |

---

### Server → Client

* `MSG_GAME_STATE` → current game state
* `MSG_WRONG_MSG` → invalid message response

---

## 🎮 Game State

Each game includes:

* `game_id`
* `player_a_id`, `player_b_id`
* `status`:

  * `WAITING_FOR_OPPONENT`
  * `TURN_A`, `TURN_B`
  * `WIN_A`, `WIN_B`
* `max_pawn`
* `pawn_row` (bitmap representation)

---

## ⚙️ Build

```bash
make
```

### Output

* `kayles_server`
* `kayles_client`

### Clean

```bash
make clean
```

---

## ▶️ Usage

### Start Server

```bash
./kayles_server -r 1110111 -a 0.0.0.0 -p 12345 -t 10
```

### Run Client

```bash
./kayles_client -a 127.0.0.1 -p 12345 -m "1/123/0/3" -t 5
```

### Message Format

```
msg_type/player_id/game_id/pawn
```

---

## 🔄 Game Flow

### Start

1. Player A sends `MSG_JOIN`
2. Server creates game (`WAITING_FOR_OPPONENT`)
3. Player B joins → game starts (`TURN_B`)

---

### Gameplay

* Players send moves via client
* Server validates and updates state
* Opponent retrieves updates via `KEEP_ALIVE`

---

### End Conditions

* 🏁 Last pawn removed → winner
* 🏳 Player gives up
* ⏱ Timeout → inactive player loses

---

## ⏱ Timeouts

| Type          | Description                    |
| ------------- | ------------------------------ |
| Server (`-t`) | Removes inactive games/players |
| Client (`-t`) | Waits for server response      |

---


## 🛠 Technical Notes

* 🌐 All multi-byte values use **network byte order**
* 🚫 No threads, processes, `select()` or `poll()`
* ⚙ Server implemented as a **state machine**
* 🔒 Strict protocol validation

---

## 📚 Technologies

* C / C++
* POSIX sockets (UDP)
* Bit-level data structures
* Custom binary protocol

---

## 🧪 Testing

```bash
python3 test_kayels.py
python3 test_kayels2.py
```

---


