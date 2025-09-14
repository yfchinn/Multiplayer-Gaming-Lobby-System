# Multiplayer-Gaming-Lobby-System
A multiplayer gaming platform supporting user registration, login, and room management. Players can create/join public or private rooms, manage invitations, and upload/share games.

## Features

- **User Registration & Login:** Secure authentication for players.
- **Lobby System:** Create, join, and manage public or private game rooms.
- **Invitation Management:** Send and accept invitations to private rooms.
- **Game Upload/Share:** Players can upload custom games to the lobby and share them with others.
- **Multiple Game Support:** Example games (`game1.cpp`, `game2.cpp`) included.

## Repository Structure

- `lobbyServer.cpp`: Main server logic for lobby and room management.
- `client.cpp`: Client-side application for user interaction and gameplay.
- `game1.cpp`, `game2.cpp`: Example game implementations.
- `player_data.txt`: Stores player information (e.g., registration, stats).
- `game_info.txt`: Stores metadata about uploaded/shared games.

## Adding Games

- To add your own game, implement the game logic (see `game1.cpp` or `game2.cpp` as templates) and update the server and client code if needed.
