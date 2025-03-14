#include <microhttpd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

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

// Post data processor callback
static enum MHD_Result post_iterator(void *cls, 
                                  enum MHD_ValueKind kind, 
                                  const char *key,
                                  const char *filename, 
                                  const char *content_type,
                                  const char *transfer_encoding, 
                                  const char *data,
                                  uint64_t off, 
                                  size_t size)
{
    printf("Received POST field: %s=%.*s\n", key, (int)size, data);
    // We don't actually use this for JSON parsing
    // Our manual parsing happens directly in the request handler
    return MHD_YES;
}

// Update the request_completed function to clean up the connection info
static void request_completed(void *cls, struct MHD_Connection *connection,
    void **con_cls, enum MHD_RequestTerminationCode toe)
{
    printf("Request completed with code: %d\n", toe);
    if (*con_cls != NULL) {
        struct ConnectionInfo *con_info = *con_cls;
        free(con_info);  // Free the connection info
        *con_cls = NULL;
    }
}

// Then create a struct to track POST state separately
#define MAX_SESSIONS 100

// Game session structure
struct GameSession {
    struct GameState game_state;
    char session_id[37];  // 36 chars for UUID + null terminator
    time_t last_access;
    bool in_use;
};

// Sessions manager
static struct {
    struct GameSession sessions[MAX_SESSIONS];
    int count;
} sessions_manager;

// Initialize sessions manager
void init_sessions_manager() {
    memset(&sessions_manager, 0, sizeof(sessions_manager));
}

// Generate a simple session ID (not cryptographically secure, but sufficient for demo)
void generate_session_id(char *buffer) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            buffer[i] = '-';
        } else {
            buffer[i] = chars[rand() % (sizeof(chars) - 1)];
        }
    }
    buffer[36] = '\0';
}

// Create a new session
struct GameSession* create_session() {
    // Find an empty slot
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!sessions_manager.sessions[i].in_use) {
            struct GameSession* session = &sessions_manager.sessions[i];
            init_game(&session->game_state);
            generate_session_id(session->session_id);
            session->last_access = time(NULL);
            session->in_use = true;
            sessions_manager.count++;
            printf("Created new session: %s\n", session->session_id);
            return session;
        }
    }
    printf("Failed to create session - max sessions reached\n");
    return NULL;
}

// Find session by ID
struct GameSession* find_session(const char* session_id) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions_manager.sessions[i].in_use && 
            strcmp(sessions_manager.sessions[i].session_id, session_id) == 0) {
            sessions_manager.sessions[i].last_access = time(NULL);
            return &sessions_manager.sessions[i];
        }
    }
    return NULL;
}

// Update connection info to store session data
struct ConnectionInfo {
    struct GameSession *session;
    int position_received;
    int position;
};

// Update the request handler to use sessions
static enum MHD_Result request_handler(void *cls,
                                      struct MHD_Connection *connection,
                                      const char *url,
                                      const char *method,
                                      const char *version,
                                      const char *upload_data,
                                      size_t *upload_data_size,
                                      void **con_cls)
{
    // First call, initialize connection info
    if (*con_cls == NULL) {
        struct ConnectionInfo *con_info = malloc(sizeof(struct ConnectionInfo));
        if (!con_info) return MHD_NO;
        
        con_info->position_received = 0;
        con_info->position = -1;
        
        *con_cls = con_info;
        
        // For POST requests, we need to return and wait for the next call with data
        if (strcmp(method, "POST") == 0) {
            return MHD_YES;
        }
    }

    struct ConnectionInfo *con_info = *con_cls;
    struct MHD_Response *response;
    enum MHD_Result ret;
    
    // Get session ID from cookies
    const char *cookie_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Cookie");
    char session_id[37] = {0};
    struct GameSession *session = NULL;
    
    // Extract session ID from cookie if it exists
    if (cookie_header) {
        const char *session_cookie = strstr(cookie_header, "session=");
        if (session_cookie) {
            session_cookie += 8; // Skip "session="
            int i = 0;
            while (i < 36 && session_cookie[i] && session_cookie[i] != ';') {
                session_id[i] = session_cookie[i];
                i++;
            }
            session_id[i] = '\0';
            
            // Find the session
            session = find_session(session_id);
        }
    }

    printf("Request: %s %s (Session: %s)\n", method, url, session ? session_id : "None");

    // Serve the main page and create a new session if needed
    if (strcmp(method, "GET") == 0 && strcmp(url, "/") == 0) {
        if (!session) {
            // Create a new session for this user
            session = create_session();
            if (!session) {
                response = MHD_create_response_from_buffer(
                    strlen("Server busy, try again later"),
                    (void*)"Server busy, try again later",
                    MHD_RESPMEM_PERSISTENT
                );
                ret = MHD_queue_response(connection, MHD_HTTP_SERVICE_UNAVAILABLE, response);
                return ret;
            }
        }
        
        // Set session for this connection
        con_info->session = session;
        
        // Create response
        response = MHD_create_response_from_buffer(strlen(html_page),
                                                 (void *)html_page,
                                                 MHD_RESPMEM_PERSISTENT);
        if (!response) {
            printf("Failed to create GET response\n");
            return MHD_NO;
        }
        
        // Set session cookie
        char cookie[100];
        snprintf(cookie, sizeof(cookie), "session=%s; Path=/; SameSite=Strict", session->session_id);
        MHD_add_response_header(response, "Set-Cookie", cookie);
        MHD_add_response_header(response, "Content-Type", "text/html");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        return ret;
    }
    
    // For other requests, require a valid session
    if (!session) {
        response = MHD_create_response_from_buffer(
            strlen("Invalid session"),
            (void*)"Invalid session",
            MHD_RESPMEM_PERSISTENT
        );
        ret = MHD_queue_response(connection, MHD_HTTP_UNAUTHORIZED, response);
        return ret;
    }
    
    // Store session in connection info
    con_info->session = session;
    
    // Handle player move
    if (strcmp(method, "POST") == 0 && strcmp(url, "/move") == 0) {
        // First pass with data
        if (*upload_data_size != 0) {
            // Process data - parse position
            const char *json = (const char *)upload_data;
            const char *pos_marker = strstr(json, "position");
            
            if (pos_marker) {
                pos_marker = strchr(pos_marker, ':');
                if (pos_marker) {
                    // Parse the position number
                    while (*++pos_marker == ' ');
                    con_info->position = atoi(pos_marker);
                    con_info->position_received = 1;
                    
                    printf("Parsed position: %d for session %s\n", 
                           con_info->position, session->session_id);
                }
            }
            
            *upload_data_size = 0;
            return MHD_YES;  // We have processed the data, but need another call to finalize
        }
        
        // Data fully received, process the move
        int position = con_info->position;
        struct GameState *game_state = &session->game_state;
        
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
        if (game_state->board[position] != 0 || game_state->game_over) {
            const char *error_msg = game_state->board[position] != 0 ? 
                                  "Position already taken" : "Game is over";
            response = MHD_create_response_from_buffer(strlen(error_msg), 
                                                     (void *)error_msg,
                                                     MHD_RESPMEM_PERSISTENT);
            MHD_add_response_header(response, "Content-Type", "text/plain");
            ret = MHD_queue_response(connection, 
                                   game_state->board[position] != 0 ? 
                                   MHD_HTTP_CONFLICT : MHD_HTTP_FORBIDDEN,
                                   response);
            return ret;
        }
        
        // Valid move, process it
        game_state->board[position] = 1; // X for player
        int winner = check_winner(game_state->board);
        
        if (winner == 0) {
            computer_move(game_state);
            winner = check_winner(game_state->board);
        }
        
        game_state->game_over = (winner != 0);
        
        // Create the response
        char response_str[256];
        int len = snprintf(response_str, sizeof(response_str),
                          "{\"board\":[%d,%d,%d,%d,%d,%d,%d,%d,%d],"
                          "\"message\":\"%s\","
                          "\"gameOver\":%s}",
                          game_state->board[0], game_state->board[1], game_state->board[2],
                          game_state->board[3], game_state->board[4], game_state->board[5],
                          game_state->board[6], game_state->board[7], game_state->board[8],
                          winner == 1 ? "You win!" : 
                          winner == 2 ? "Computer wins!" : 
                          winner == 3 ? "Draw!" : "Computer\'s turn",
                          game_state->game_over ? "true" : "false");
        
        // Use MUST_COPY for stack-allocated response string
        response = MHD_create_response_from_buffer(strlen(response_str),
                                                 response_str,
                                                 MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header(response, "Content-Type", "application/json");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        
        printf("Move processed successfully for session %s\n", session->session_id);
        return ret;
    }
    
    // Handle reset
    if (strcmp(method, "POST") == 0 && strcmp(url, "/reset") == 0) {
        init_game(&session->game_state);
        char response_str[128];
        snprintf(response_str, sizeof(response_str),
                "{\"board\":[%d,%d,%d,%d,%d,%d,%d,%d,%d]}",
                0, 0, 0, 0, 0, 0, 0, 0, 0);
        
        response = MHD_create_response_from_buffer(strlen(response_str),
                                                 (void *)response_str,
                                                 MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header(response, "Content-Type", "application/json");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        printf("Game reset for session %s\n", session->session_id);
        return ret;
    }

    response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    return ret;
}

// Add a cleanup function for idle sessions
void cleanup_sessions() {
    time_t now = time(NULL);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions_manager.sessions[i].in_use && 
            difftime(now, sessions_manager.sessions[i].last_access) > 1800) {  // 30 minutes
            printf("Cleaning up idle session: %s\n", sessions_manager.sessions[i].session_id);
            sessions_manager.sessions[i].in_use = false;
            sessions_manager.count--;
        }
    }
}

// Update the main function
int main(void)
{
    srand(time(NULL));
    struct MHD_Daemon *daemon;
    const int port = 8888;
    
    // Initialize sessions manager
    init_sessions_manager();

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
    
    // Run a periodic cleanup every 5 minutes
    time_t last_cleanup = time(NULL);
    
    while (1) {
        // Check for keyboard input with timeout
        fd_set fds;
        struct timeval tv;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        tv.tv_sec = 60;  // Check every minute
        tv.tv_usec = 0;
        
        int result = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        
        if (result > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
            getchar();  // Consume the Enter key
            break;
        }
        
        // Periodically clean up idle sessions
        time_t now = time(NULL);
        if (difftime(now, last_cleanup) > 300) {  // 5 minutes
            printf("Running session cleanup...\n");
            cleanup_sessions();
            last_cleanup = now;
        }
    }
    
    MHD_stop_daemon(daemon);
    return 0;
}