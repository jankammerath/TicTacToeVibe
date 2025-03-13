# Tic-Tac-Toe Game Server
A simple HTTP-based Tic-Tac-Toe game server implemented in C using the libmicrohttpd library.

## Overview
This project implements a web-based Tic-Tac-Toe game where:

- The player plays as 'X'
- The computer plays as 'O'
- The game runs in a web browser
- The game logic is handled by a C-based HTTP server

## Features
- Simple, clean web interface
- RESTful API for game interactions
- Computer opponent making random moves
- Full game state tracking
- Win/loss/draw detection

## Requirements
- C compiler (gcc or clang)
- libmicrohttpd library
- Standard C libraries (stdio, stdlib, string, time)

## Building the Project
[Instructions for building the project would go here.]

## Running the Server
This will start the server on port 8888. Open your web browser and navigate to `http://localhost:8888` to play the game.

## API Endpoints
| Endpoint  | Method | Description                          |
|-----------|--------|--------------------------------------|
| `/`       | GET    | Returns the game interface (HTML)    |
| `/move`   | POST   | Makes a player move. Send JSON: `{"position": 0-8}` |
| `/reset`  | POST   | Resets the game to initial state     |

## Game Board Layout
The game board is represented as a 3x3 grid with positions numbered 0-8:

```
 0 | 1 | 2
-----------
 3 | 4 | 5
-----------
 6 | 7 | 8
```


## Response Format
Moves return JSON responses in the following format:

```json
{
  "board": [0, 1, 2, 0, 0, 0, 0, 0, 0],
  "message": "Player X's turn",
  "gameOver": false
}
```

Where:
- `board` is an array of 9 integers (0=empty, 1=X, 2=O)
- `message` indicates game status
- `gameOver` is true when the game has concluded

# Architecture
The server uses libmicrohttpd to handle HTTP requests and maintain game state. The application:
Initializes the game state

- Processes player moves
- Makes computer moves
- Checks for win/loss/draw conditions
- Returns updated game state to the client

# License
This project is open source and available under the MIT license.

# Acknowledgments
- Uses libmicrohttpd for HTTP server functionality
- Simple, educational implementation of HTTP-based game logic