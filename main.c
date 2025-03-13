#include <microhttpd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// HTML content with embedded JavaScript for Tic-Tac-Toe
static const char *html_page = 
    "<!DOCTYPE html>"
    "<html>"
    "<head><title>Tic-Tac-Toe</title>"
    "<style>"
    "  table { border-collapse: collapse; margin: 20px auto; }"
    "  td { width: 50px; height: 50px; border: 1px solid black; text-align: center; font-size: 24px; cursor: pointer; }"
    "  .message { text-align: center; margin: 20px; font-size: 18px; }"
    "</style>"
    "</head>"
    "<body>"
    "<h1 style='text-align: center;'>Tic-Tac-Toe</h1>"
    "<div id='message' class='message'>Your turn (X)</div>"
    "<table id='board'>"
    "<tr><td onclick='makeMove(0)'></td><td onclick='makeMove(1)'></td><td onclick='makeMove(2)'></td></tr>"
    "<tr><td onclick='makeMove(3)'></td><td onclick='makeMove(4)'></td><td onclick='makeMove(5)'></td></tr>"
    "<tr><td onclick='makeMove(6)'></td><td onclick='makeMove(7)'></td><td onclick='makeMove(8)'></td></tr>"
    "</table>"
    "<div style='text-align: center;'><button onclick='resetGame()'>Reset Game</button></div>"
    "<script>"
    "async function makeMove(position) {"
    "  const response = await fetch('/move', {"
    "    method: 'POST',"
    "    headers: { 'Content-Type': 'application/json' },"
    "    body: JSON.stringify({ position: position })"
    "  });"
    "  const data = await response.json();"
    "  updateBoard(data.board);"
    "  document.getElementById('message').textContent = data.message;"
    "  if (data.gameOver) {"
    "    document.querySelectorAll('td').forEach(cell => cell.onclick = null);"
    "  }"
    "}"
    "function updateBoard(board) {"
    "  const cells = document.querySelectorAll('td');"
    "  for (let i = 0; i < 9; i++) {"
    "    cells[i].textContent = board[i] === 0 ? '' : (board[i] === 1 ? 'X' : 'O');"
    "  }"
    "}"
    "function resetGame() {"
    "  fetch('/reset', { method: 'POST' })"
    "    .then(response => response.json())"
    "    .then(data => {"
    "      updateBoard(data.board);"
    "      document.getElementById('message').textContent = 'Your turn (X)';"
    "      document.querySelectorAll('td').forEach(cell => cell.onclick = () => makeMove(cell.cellIndex + (cell.parentNode.rowIndex * 3)));"
    "    });"
    "}"
    "</script>"
    "</body>"
    "</html>";

// Game state
struct GameState {
    int board[9]; // 0 = empty, 1 = X, 2 = O
    int game_over;
    int last_position; // Store the last parsed position
};

// Initialize or reset game state
void init_game(struct GameState *state) {
    memset(state->board, 0, sizeof(state->board));
    state->game_over = 0;
    state->last_position = -1;
}

// Check for winner
int check_winner(int *board) {
    // Check rows, columns, and diagonals
    int wins[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8}, // rows
        {0,3,6}, {1,4,7}, {2,5,8}, // columns
        {0,4,8}, {2,4,6}           // diagonals
    };
    
    for (int i = 0; i < 8; i++) {
        if (board[wins[i][0]] != 0 &&
            board[wins[i][0]] == board[wins[i][1]] &&
            board[wins[i][1]] == board[wins[i][2]]) {
            return board[wins[i][0]];
        }
    }
    
    // Check for draw
    int full = 1;
    for (int i = 0; i < 9; i++) {
        if (board[i] == 0) {
            full = 0;
            break;
        }
    }
    return full ? 3 : 0; // 3 = draw, 0 = ongoing
}

// Computer's move (simple random move)
void computer_move(struct GameState *state) {
    if (state->game_over) return;
    
    int empty[9];
    int count = 0;
    for (int i = 0; i < 9; i++) {
        if (state->board[i] == 0) {
            empty[count++] = i;
        }
    }
    if (count > 0) {
        int move = empty[rand() % count];
        state->board[move] = 2; // O for computer
    }
}

// Callback for when a request is completed
static void request_completed(void *cls, struct MHD_Connection *connection,
    void **con_cls, enum MHD_RequestTerminationCode toe)
{
    printf("Request completed with code: %d\n", toe);
    if (*con_cls != NULL) {
        // No need to free game_state here; it's static
        *con_cls = NULL;
    }
}

static enum MHD_Result request_handler(void *cls,
                                      struct MHD_Connection *connection,
                                      const char *url,
                                      const char *method,
                                      const char *version,
                                      const char *upload_data,
                                      size_t *upload_data_size,
                                      void **con_cls)
{
    static struct GameState game_state;
    
    if (*con_cls == NULL) {
        init_game(&game_state);
        *con_cls = &game_state;
        printf("Initialized new game state\n");
    }

    struct MHD_Response *response;
    enum MHD_Result ret;

    printf("Request: %s %s\n", method, url);

    // Serve the main page
    if (strcmp(method, "GET") == 0 && strcmp(url, "/") == 0) {
        response = MHD_create_response_from_buffer(strlen(html_page),
                                                 (void *)html_page,
                                                 MHD_RESPMEM_PERSISTENT);
        if (!response) {
            printf("Failed to create GET response\n");
            return MHD_NO;
        }
        MHD_add_response_header(response, "Content-Type", "text/html");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        return ret;
    }
    
    // Handle player move
    if (strcmp(method, "POST") == 0 && strcmp(url, "/move") == 0) {
        // First call: wait for data
        if (*con_cls == NULL) {
            struct MHD_PostProcessor *pp = MHD_create_post_processor(
                connection, 1024, &post_iterator, NULL);
            
            if (pp == NULL) {
                return MHD_NO;
            }
            
            *con_cls = pp;
            return MHD_YES;
        }
        
        if (*upload_data_size != 0) {
            // We're still receiving data
            printf("Received POST data: %.*s\n", (int)*upload_data_size, upload_data);
            
            // Process the data separately
            int position = -1;
            const char *json = (const char *)upload_data;
            const char *pos_marker = strstr(json, "position");
            
            if (pos_marker) {
                pos_marker = strchr(pos_marker, ':');
                if (pos_marker) {
                    // Skip whitespace after colon
                    while (*++pos_marker == ' ');
                    // Parse the position number
                    position = atoi(pos_marker);
                    printf("Parsed position: %d\n", position);
                    
                    // Store the position for later use
                    game_state.last_position = position;
                }
            }
            
            *upload_data_size = 0;
            return MHD_YES;
        }
        
        // Data fully received, process the move
        int position = game_state.last_position;
        
        if (position < 0 || position > 8) {
            printf("Invalid position format: %d\n", position);
            const char *error_msg = "Invalid position";
            response = MHD_create_response_from_buffer(strlen(error_msg), 
                                                      (void *)error_msg, 
                                                      MHD_RESPMEM_PERSISTENT);
            MHD_add_response_header(response, "Content-Type", "text/plain");
            ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
            return ret;
        }
        
        // Continue with move processing
        if (game_state.board[position] != 0 || game_state.game_over) {
            printf("Invalid move: position=%d\n", position);
            const char *error_msg = game_state.board[position] != 0 ? 
                                 "Position already taken" : "Game is over";
            response = MHD_create_response_from_buffer(strlen(error_msg), 
                                                      (void *)error_msg,
                                                      MHD_RESPMEM_PERSISTENT);
            MHD_add_response_header(response, "Content-Type", "text/plain");
            ret = MHD_queue_response(connection, 
                                  game_state.board[position] != 0 ? MHD_HTTP_CONFLICT : MHD_HTTP_FORBIDDEN,
                                  response);
            return ret;
        }
        
        // Valid move, process it
        game_state.board[position] = 1; // X for player
        int winner = check_winner(game_state.board);
        
        if (winner == 0) {
            computer_move(&game_state);
            winner = check_winner(game_state.board);
        }
        
        game_state.game_over = (winner != 0);
        
        // Create the response
        char response_str[256];
        int len = snprintf(response_str, sizeof(response_str),
                          "{\"board\":[%d,%d,%d,%d,%d,%d,%d,%d,%d],"
                          "\"message\":\"%s\","
                          "\"gameOver\":%s}",
                          game_state.board[0], game_state.board[1], game_state.board[2],
                          game_state.board[3], game_state.board[4], game_state.board[5],
                          game_state.board[6], game_state.board[7], game_state.board[8],
                          winner == 1 ? "You win!" : 
                          winner == 2 ? "Computer wins!" : 
                          winner == 3 ? "Draw!" : "Computer\'s turn",
                          game_state.game_over ? "true" : "false");
        
        // Use MUST_COPY for stack-allocated response string
        response = MHD_create_response_from_buffer(strlen(response_str),
                                                 response_str,
                                                 MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header(response, "Content-Type", "application/json");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        
        printf("Move processed successfully\n");
        return ret;
    }
    
    // Handle reset
    if (strcmp(method, "POST") == 0 && strcmp(url, "/reset") == 0) {
        init_game(&game_state);
        char response_str[128];
        snprintf(response_str, sizeof(response_str),
                "{\"board\":[%d,%d,%d,%d,%d,%d,%d,%d,%d]}",
                game_state.board[0], game_state.board[1], game_state.board[2],
                game_state.board[3], game_state.board[4], game_state.board[5],
                game_state.board[6], game_state.board[7], game_state.board[8]);
        
        response = MHD_create_response_from_buffer(strlen(response_str),
                                                 (void *)response_str,
                                                 MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header(response, "Content-Type", "application/json");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        return ret;
    }

    response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    return ret;
}

int main(void)
{
    srand(time(NULL));
    struct MHD_Daemon *daemon;
    const int port = 8888;

    daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG,
                             port,
                             NULL, NULL,
                             &request_handler,
                             NULL,
                             MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL,
                             MHD_OPTION_END);

    if (daemon == NULL) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }

    printf("Server running on port %d\n", port);
    printf("Press Enter to stop the server...\n");
    getchar();
    MHD_stop_daemon(daemon);
    return 0;
}