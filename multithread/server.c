/*
 * ============================================================================
 * File: server.c
 * Project: Multithreaded Private Chat & Support Desk Server
 * Description:
 *   - Multithreaded TCP Server using POSIX threads (pthread).
 *   - Concurrent client handling (each client assigned a worker thread).
 *   - Thread-safe client registry protected by mutex locks.
 *   - Comprehensive real-time server logging with timestamps and Thread IDs.
 *   - Role-based routing: Customer (Client) vs Support Team Agent.
 *   - Private 1-on-1 chat channels isolated between client and support agent.
 *   - Direct private chat capability between Server and any Client.
 *   - Interactive Server Console thread for live monitoring and administration.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>
#include <stdbool.h>

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 2048
#define MAX_CLIENTS 50
#define NAME_LEN 64
#define ISSUE_LEN 256

// Role enumeration
typedef enum {
    ROLE_UNREGISTERED = 0,
    ROLE_CUSTOMER,
    ROLE_SUPPORT
} client_role_t;

// Session status enumeration
typedef enum {
    STATUS_IDLE = 0,
    STATUS_WAITING_SUPPORT,
    STATUS_IN_CHAT
} client_status_t;

// Structure representing a connected client
typedef struct {
    int id;
    int socket_fd;
    struct sockaddr_in address;
    pthread_t thread_id;
    char name[NAME_LEN];
    client_role_t role;
    client_status_t status;
    int partner_id;              // ID of paired partner (-1 if none)
    char pending_issue[ISSUE_LEN]; // Issue description
    time_t connect_time;
    int messages_sent;
    int messages_received;
    bool active;
} client_t;

// Global server state
static client_t *clients[MAX_CLIENTS];
static int next_client_id = 1;
static int active_client_count = 0;
static int server_socket_fd = -1;
static volatile bool server_running = true;

// Mutex locks
static pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

// Function Prototypes
void server_log(const char *tag, const char *format, ...);
void *client_handler_thread(void *arg);
void *server_console_thread(void *arg);
void handle_client_disconnect(client_t *cli);
client_t *find_client_by_id_locked(int id);
void send_raw(int socket_fd, const char *msg);
void send_to_client(client_t *cli, const char *format, ...);
void broadcast_to_role(client_role_t role, const char *format, ...);
const char *role_str(client_role_t r);
const char *status_str(client_status_t s);

/*
 * Thread-safe logging function with timestamp, thread ID, and formatted tag
 */
void server_log(const char *tag, const char *format, ...) {
    va_list args;
    char timestamp[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    pthread_mutex_lock(&log_lock);
    printf("[%s] [TID:%14lu] [%-13s] ", timestamp, (unsigned long)pthread_self(), tag);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
    pthread_mutex_unlock(&log_lock);
}

const char *role_str(client_role_t r) {
    switch (r) {
        case ROLE_CUSTOMER: return "CUSTOMER";
        case ROLE_SUPPORT:  return "SUPPORT";
        default:            return "UNREGISTERED";
    }
}

const char *status_str(client_status_t s) {
    switch (s) {
        case STATUS_WAITING_SUPPORT: return "WAITING_SUPPORT";
        case STATUS_IN_CHAT:         return "IN_PRIVATE_CHAT";
        default:                     return "IDLE";
    }
}

void send_raw(int socket_fd, const char *msg) {
    if (socket_fd >= 0) {
        send(socket_fd, msg, strlen(msg), 0);
    }
}

void send_to_client(client_t *cli, const char *format, ...) {
    if (!cli || cli->socket_fd < 0) return;
    char buffer[BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    send_raw(cli->socket_fd, buffer);
}

// Broadcasts a message to all connected clients of a specific role
void broadcast_to_role(client_role_t role, const char *format, ...) {
    char buffer[BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->active && clients[i]->role == role) {
            send_raw(clients[i]->socket_fd, buffer);
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

// Find client by ID (Caller MUST hold clients_lock)
client_t *find_client_by_id_locked(int id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->active && clients[i]->id == id) {
            return clients[i];
        }
    }
    return NULL;
}

// Add client to registry
bool register_client(client_t *cli) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            cli->id = next_client_id++;
            clients[i] = cli;
            active_client_count++;
            pthread_mutex_unlock(&clients_lock);
            return true;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return false;
}

// Remove client from registry
void unregister_client(client_t *cli) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == cli) {
            clients[i] = NULL;
            active_client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

// Disconnect cleanup logic
void handle_client_disconnect(client_t *cli) {
    if (!cli) return;

    pthread_mutex_lock(&clients_lock);
    int partner_id = cli->partner_id;
    client_t *partner = (partner_id > 0) ? find_client_by_id_locked(partner_id) : NULL;

    if (partner) {
        partner->partner_id = -1;
        partner->status = STATUS_IDLE;
        send_to_client(partner, "\n[SYSTEM] Your private chat partner '%s' has disconnected. Returning to IDLE status.\n> ", cli->name);
        server_log("SESSION_END", "Private session between Client #%d ('%s') and Client #%d ('%s') ended due to disconnect.",
                   cli->id, cli->name, partner->id, partner->name);
    }
    pthread_mutex_unlock(&clients_lock);

    server_log("DISCONNECT", "Client #%d ('%s', %s) disconnected. Active clients remaining: %d.",
               cli->id, cli->name[0] ? cli->name : "Anonymous", role_str(cli->role), active_client_count - 1);

    close(cli->socket_fd);
    cli->socket_fd = -1;
    cli->active = false;
    unregister_client(cli);
    free(cli);
}

/*
 * Worker thread for each connected client
 */
void *client_handler_thread(void *arg) {
    client_t *cli = (client_t *)arg;
    char buffer[BUFFER_SIZE];
    char ip_str[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &(cli->address.sin_addr), ip_str, sizeof(ip_str));
    int port = ntohs(cli->address.sin_port);

    server_log("CONNECT", "New incoming connection from %s:%d assigned Client ID #%d.", ip_str, port, cli->id);

    // Initial greeting and registration prompt
    send_to_client(cli, 
        "===========================================================\n"
        "  WELCOME TO THE MULTITHREADED PRIVATE SUPPORT SERVER      \n"
        "===========================================================\n"
        "Assigned Client ID: #%d\n"
        "Please complete registration in your client interface.\n", cli->id);

    // Main message processing loop
    while (server_running && cli->active) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = recv(cli->socket_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received <= 0) {
            // Client closed connection or error occurred
            break;
        }

        buffer[bytes_received] = '\0';

        // Trim trailing newline or carriage return
        while (strlen(buffer) > 0 && (buffer[strlen(buffer) - 1] == '\n' || buffer[strlen(buffer) - 1] == '\r')) {
            buffer[strlen(buffer) - 1] = '\0';
        }

        if (strlen(buffer) == 0) continue;

        cli->messages_sent++;

        // 1. REGISTRATION HANDSHAKE: "ROLE:<CUSTOMER|SUPPORT>:<NAME>"
        if (strncmp(buffer, "ROLE:", 5) == 0) {
            char role_token[32];
            char name_token[NAME_LEN];
            memset(role_token, 0, sizeof(role_token));
            memset(name_token, 0, sizeof(name_token));

            char *p1 = buffer + 5;
            char *p2 = strchr(p1, ':');
            if (p2) {
                size_t rlen = p2 - p1;
                strncpy(role_token, p1, (rlen < sizeof(role_token)-1) ? rlen : sizeof(role_token)-1);
                strncpy(name_token, p2 + 1, sizeof(name_token) - 1);
            } else {
                strncpy(role_token, p1, sizeof(role_token) - 1);
                snprintf(name_token, sizeof(name_token), "User_%d", cli->id);
            }

            pthread_mutex_lock(&clients_lock);
            strncpy(cli->name, name_token, sizeof(cli->name) - 1);
            if (strcasecmp(role_token, "SUPPORT") == 0) {
                cli->role = ROLE_SUPPORT;
            } else {
                cli->role = ROLE_CUSTOMER;
            }
            pthread_mutex_unlock(&clients_lock);

            server_log("REGISTER", "Client #%d registered as %s with Name '%s'.", cli->id, role_str(cli->role), cli->name);

            if (cli->role == ROLE_SUPPORT) {
                send_to_client(cli, 
                    "[SYSTEM] Welcome Agent '%s'! You are registered as SUPPORT TEAM.\n"
                    "Available Commands:\n"
                    "  /list                - List active customers & pending support requests\n"
                    "  /accept <client_id>  - Accept and start private 1-on-1 chat with client\n"
                    "  /close               - Close active private session with current client\n"
                    "  /help                - Show commands\n"
                    "  /exit                - Disconnect\n", cli->name);
            } else {
                send_to_client(cli, 
                    "[SYSTEM] Welcome '%s'! You are registered as CUSTOMER/CLIENT.\n"
                    "Available Commands:\n"
                    "  /request <issue>     - Request support team assistance\n"
                    "  /server <message>    - Chat directly and privately with Server\n"
                    "  /status              - Check current session status\n"
                    "  /close               - Close private chat with support agent\n"
                    "  /help                - Show commands\n"
                    "  /exit                - Disconnect\n", cli->name);
            }
            continue;
        }

        // 2. DISCONNECT COMMAND: "/exit" or "/quit"
        if (strcasecmp(buffer, "/exit") == 0 || strcasecmp(buffer, "/quit") == 0) {
            send_to_client(cli, "[SYSTEM] Goodbye! Disconnecting...\n");
            break;
        }

        // 3. HELP COMMAND: "/help"
        if (strcasecmp(buffer, "/help") == 0) {
            if (cli->role == ROLE_SUPPORT) {
                send_to_client(cli,
                    "--- SUPPORT AGENT COMMANDS ---\n"
                    "  /list               : View all clients and pending support tickets\n"
                    "  /accept <client_id> : Start private 1-on-1 session with client\n"
                    "  /close              : End private session with current client\n"
                    "  /server <msg>       : Message the server operator\n"
                    "  /exit               : Disconnect from server\n"
                    "  (Any plain text while in session is sent PRIVATELY to your client)\n");
            } else {
                send_to_client(cli,
                    "--- CUSTOMER COMMANDS ---\n"
                    "  /request <issue>    : Request a support team agent\n"
                    "  /server <msg>       : Chat directly and privately with Server\n"
                    "  /status             : View your current session status\n"
                    "  /close              : Leave active private chat\n"
                    "  /exit               : Disconnect from server\n"
                    "  (Any plain text while paired is sent PRIVATELY to your support agent)\n");
            }
            continue;
        }

        // 4. STATUS COMMAND: "/status"
        if (strcasecmp(buffer, "/status") == 0) {
            pthread_mutex_lock(&clients_lock);
            char partner_info[128] = "None";
            if (cli->partner_id > 0) {
                client_t *partner = find_client_by_id_locked(cli->partner_id);
                if (partner) {
                    snprintf(partner_info, sizeof(partner_info), "#%d ('%s')", partner->id, partner->name);
                }
            }
            send_to_client(cli, 
                "--- STATUS REPORT ---\n"
                "  Client ID: #%d\n"
                "  Name     : %s\n"
                "  Role     : %s\n"
                "  Status   : %s\n"
                "  Partner  : %s\n"
                "  Messages : %d sent\n",
                cli->id, cli->name, role_str(cli->role), status_str(cli->status), partner_info, cli->messages_sent);
            pthread_mutex_unlock(&clients_lock);
            continue;
        }

        // 5. CUSTOMER REQUEST SUPPORT: "/request <issue description>"
        if (strncasecmp(buffer, "/request", 8) == 0) {
            if (cli->role != ROLE_CUSTOMER) {
                send_to_client(cli, "[-] Only customers can request support.\n");
                continue;
            }

            char *issue = buffer + 8;
            while (*issue == ' ') issue++;
            if (strlen(issue) == 0) {
                issue = "General Inquiry / Support Request";
            }

            pthread_mutex_lock(&clients_lock);
            cli->status = STATUS_WAITING_SUPPORT;
            strncpy(cli->pending_issue, issue, sizeof(cli->pending_issue) - 1);
            pthread_mutex_unlock(&clients_lock);

            server_log("SUPPORT_REQ", "Client #%d ('%s') submitted support request: \"%s\".",
                       cli->id, cli->name, cli->pending_issue);

            send_to_client(cli, "[SYSTEM] Support request registered! Waiting for a Support Agent to accept...\n");

            // Alert all support agents
            broadcast_to_role(ROLE_SUPPORT,
                "\n[ALERT] New support request from Client #%d ('%s'): \"%s\"\n"
                "        Type '/accept %d' to start private 1-on-1 session.\n> ",
                cli->id, cli->name, cli->pending_issue, cli->id);
            continue;
        }

        // 6. SUPPORT AGENT LIST REQUESTS: "/list"
        if (strcasecmp(buffer, "/list") == 0) {
            if (cli->role != ROLE_SUPPORT) {
                send_to_client(cli, "[-] Only support team members can view the client request list.\n");
                continue;
            }

            char list_buf[BUFFER_SIZE];
            int offset = snprintf(list_buf, sizeof(list_buf),
                "\n==================== ACTIVE CLIENT REGISTRY ====================\n"
                "%-5s %-16s %-12s %-16s %-20s\n"
                "----------------------------------------------------------------\n",
                "ID", "NAME", "ROLE", "STATUS", "ISSUE / PARTNER");

            pthread_mutex_lock(&clients_lock);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] && clients[i]->active) {
                    char extra[32] = "-";
                    if (clients[i]->status == STATUS_WAITING_SUPPORT) {
                        strncpy(extra, clients[i]->pending_issue, sizeof(extra)-1);
                    } else if (clients[i]->partner_id > 0) {
                        snprintf(extra, sizeof(extra), "Paired with #%d", clients[i]->partner_id);
                    }
                    offset += snprintf(list_buf + offset, sizeof(list_buf) - offset,
                        "%-5d %-16s %-12s %-16s %-20s\n",
                        clients[i]->id, clients[i]->name, role_str(clients[i]->role),
                        status_str(clients[i]->status), extra);
                }
            }
            pthread_mutex_unlock(&clients_lock);

            offset += snprintf(list_buf + offset, sizeof(list_buf) - offset,
                "================================================================\n");
            send_raw(cli->socket_fd, list_buf);
            continue;
        }

        // 7. SUPPORT AGENT ACCEPT CLIENT: "/accept <client_id>"
        if (strncasecmp(buffer, "/accept", 7) == 0) {
            if (cli->role != ROLE_SUPPORT) {
                send_to_client(cli, "[-] Only support agents can accept client requests.\n");
                continue;
            }

            if (cli->status == STATUS_IN_CHAT) {
                send_to_client(cli, "[-] You are already in a private chat with Client #%d. Type '/close' first.\n", cli->partner_id);
                continue;
            }

            char *id_str = buffer + 7;
            while (*id_str == ' ') id_str++;
            int target_id = atoi(id_str);

            if (target_id <= 0) {
                send_to_client(cli, "[-] Usage: /accept <client_id>\n");
                continue;
            }

            pthread_mutex_lock(&clients_lock);
            client_t *target = find_client_by_id_locked(target_id);

            if (!target) {
                pthread_mutex_unlock(&clients_lock);
                send_to_client(cli, "[-] Client ID #%d not found or inactive.\n", target_id);
                continue;
            }

            if (target->role != ROLE_CUSTOMER) {
                pthread_mutex_unlock(&clients_lock);
                send_to_client(cli, "[-] Target #%d is not a customer.\n", target_id);
                continue;
            }

            if (target->status == STATUS_IN_CHAT) {
                pthread_mutex_unlock(&clients_lock);
                send_to_client(cli, "[-] Client #%d is already in a private chat with another agent.\n", target_id);
                continue;
            }

            // Establish private 1-on-1 session
            cli->partner_id = target->id;
            cli->status = STATUS_IN_CHAT;

            target->partner_id = cli->id;
            target->status = STATUS_IN_CHAT;

            server_log("SESSION_START", "Private 1-on-1 session started: Support Agent #%d ('%s') <==> Client #%d ('%s').",
                       cli->id, cli->name, target->id, target->name);

            // Notify both parties
            send_to_client(cli, 
                "\n[SYSTEM] Private session connected with Client #%d ('%s')!\n"
                "[SYSTEM] Client Issue: \"%s\"\n"
                "[SYSTEM] Any message you type now is sent PRIVATELY to %s. Type '/close' to end.\n> ",
                target->id, target->name, target->pending_issue, target->name);

            send_to_client(target, 
                "\n[SYSTEM] Support Agent '%s' has joined your private chat!\n"
                "[SYSTEM] Any message you type now is sent PRIVATELY to %s. Type '/close' to end.\n> ",
                cli->name, cli->name);

            pthread_mutex_unlock(&clients_lock);
            continue;
        }

        // 8. CLOSE PRIVATE SESSION: "/close"
        if (strcasecmp(buffer, "/close") == 0) {
            pthread_mutex_lock(&clients_lock);
            if (cli->status != STATUS_IN_CHAT || cli->partner_id <= 0) {
                pthread_mutex_unlock(&clients_lock);
                send_to_client(cli, "[-] You do not have an active private chat session.\n");
                continue;
            }

            client_t *partner = find_client_by_id_locked(cli->partner_id);
            int old_partner_id = cli->partner_id;

            cli->partner_id = -1;
            cli->status = STATUS_IDLE;

            if (partner) {
                partner->partner_id = -1;
                partner->status = STATUS_IDLE;
                send_to_client(partner, "\n[SYSTEM] Private session closed by '%s'. Returning to IDLE.\n> ", cli->name);
            }

            send_to_client(cli, "\n[SYSTEM] Private session closed. Returning to IDLE.\n> ");

            server_log("SESSION_END", "Private session closed between Client #%d ('%s') and Client #%d.",
                       cli->id, cli->name, old_partner_id);

            pthread_mutex_unlock(&clients_lock);
            continue;
        }

        // 9. DIRECT PRIVATE CHAT WITH SERVER: "/server <message>"
        if (strncasecmp(buffer, "/server", 7) == 0) {
            char *msg = buffer + 7;
            while (*msg == ' ') msg++;

            server_log("SERVER_CHAT", "Private message from Client #%d ('%s', %s) -> SERVER: \"%s\"",
                       cli->id, cli->name, role_str(cli->role), msg);

            // Server automated acknowledgement
            send_to_client(cli, "[SERVER DIRECT]: Received your message: \"%s\". Server operator logged this request.\n", msg);
            continue;
        }

        // 10. ACTIVE PRIVATE CHAT MESSAGE ROUTING
        pthread_mutex_lock(&clients_lock);
        if (cli->status == STATUS_IN_CHAT && cli->partner_id > 0) {
            client_t *partner = find_client_by_id_locked(cli->partner_id);
            if (partner) {
                partner->messages_received++;

                // Log the private message on the Server
                server_log("PRIVATE_MSG", "[Client #%d '%s' -> Partner #%d '%s']: \"%s\"",
                           cli->id, cli->name, partner->id, partner->name, buffer);

                // Relay strictly to the private partner
                if (cli->role == ROLE_SUPPORT) {
                    send_to_client(partner, "\n[Support Agent %s]: %s\n> ", cli->name, buffer);
                } else {
                    send_to_client(partner, "\n[%s]: %s\n> ", cli->name, buffer);
                }
                pthread_mutex_unlock(&clients_lock);
                continue;
            }
        }
        pthread_mutex_unlock(&clients_lock);

        // 11. UNPAIRED MESSAGES: Defaults to Private Server Chat
        server_log("DIRECT_CHAT", "Unpaired message from Client #%d ('%s'): \"%s\"", cli->id, cli->name, buffer);
        send_to_client(cli, 
            "[SERVER] You are not currently in a private session with an agent.\n"
            "         - Type '/request <issue>' to request a support agent.\n"
            "         - Type '/server <msg>' to chat with the server.\n"
            "         - Type '/help' for command options.\n");
    }

    // Clean disconnect
    handle_client_disconnect(cli);
    return NULL;
}

/*
 * Interactive Server Console Thread:
 * Allows server administrator to monitor logs, list clients, send direct private messages,
 * and broadcast announcements.
 */
void *server_console_thread(void *arg) {
    (void)arg;
    char line[BUFFER_SIZE];

    printf("\n[SERVER CONSOLE] Active. Type '/help' for admin commands.\n");

    while (server_running) {
        if (!fgets(line, sizeof(line), stdin)) break;

        // Trim newline
        while (strlen(line) > 0 && (line[strlen(line) - 1] == '\n' || line[strlen(line) - 1] == '\r')) {
            line[strlen(line) - 1] = '\0';
        }

        if (strlen(line) == 0) continue;

        if (strcasecmp(line, "/help") == 0) {
            pthread_mutex_lock(&log_lock);
            printf("\n--- SERVER ADMIN COMMANDS ---\n"
                   "  /clients               : List all connected clients with details\n"
                   "  /msg <id> <message>    : Send a private message directly from Server to Client\n"
                   "  /broadcast <message>   : Broadcast an announcement to all clients\n"
                   "  /kick <id>             : Disconnect a client\n"
                   "  /quit                  : Shutdown server\n\n");
            pthread_mutex_unlock(&log_lock);
        }
        else if (strcasecmp(line, "/clients") == 0 || strcasecmp(line, "/list") == 0) {
            pthread_mutex_lock(&clients_lock);
            pthread_mutex_lock(&log_lock);
            printf("\n======================= CONNECTED CLIENTS (%d) =======================\n", active_client_count);
            printf("%-4s %-16s %-10s %-16s %-16s %-8s\n", "ID", "NAME", "ROLE", "STATUS", "IP ADDRESS", "PARTNER");
            printf("-----------------------------------------------------------------------\n");
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] && clients[i]->active) {
                    char ip_buf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(clients[i]->address.sin_addr), ip_buf, sizeof(ip_buf));
                    char partner_str[16] = "None";
                    if (clients[i]->partner_id > 0) {
                        snprintf(partner_str, sizeof(partner_str), "#%d", clients[i]->partner_id);
                    }
                    printf("%-4d %-16s %-10s %-16s %-16s %-8s\n",
                           clients[i]->id, clients[i]->name, role_str(clients[i]->role),
                           status_str(clients[i]->status), ip_buf, partner_str);
                }
            }
            printf("=======================================================================\n\n");
            pthread_mutex_unlock(&log_lock);
            pthread_mutex_unlock(&clients_lock);
        }
        else if (strncasecmp(line, "/msg", 4) == 0) {
            char *p = line + 4;
            while (*p == ' ') p++;
            int target_id = atoi(p);
            while (*p && *p != ' ') p++;
            while (*p == ' ') p++;

            if (target_id <= 0 || strlen(p) == 0) {
                printf("[-] Usage: /msg <client_id> <message>\n");
                continue;
            }

            pthread_mutex_lock(&clients_lock);
            client_t *target = find_client_by_id_locked(target_id);
            if (target) {
                send_to_client(target, "\n[PRIVATE FROM SERVER OPERATOR]: %s\n> ", p);
                server_log("SERVER_MSG", "Sent private message to Client #%d ('%s'): \"%s\"", target->id, target->name, p);
            } else {
                printf("[-] Client #%d not found.\n", target_id);
            }
            pthread_mutex_unlock(&clients_lock);
        }
        else if (strncasecmp(line, "/broadcast", 10) == 0) {
            char *msg = line + 10;
            while (*msg == ' ') msg++;
            if (strlen(msg) == 0) {
                printf("[-] Usage: /broadcast <message>\n");
                continue;
            }

            pthread_mutex_lock(&clients_lock);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] && clients[i]->active) {
                    send_to_client(clients[i], "\n[SERVER BROADCAST ANNOUNCEMENT]: %s\n> ", msg);
                }
            }
            pthread_mutex_unlock(&clients_lock);
            server_log("BROADCAST", "Admin broadcast: \"%s\"", msg);
        }
        else if (strncasecmp(line, "/kick", 5) == 0) {
            char *p = line + 5;
            while (*p == ' ') p++;
            int target_id = atoi(p);

            pthread_mutex_lock(&clients_lock);
            client_t *target = find_client_by_id_locked(target_id);
            if (target) {
                send_to_client(target, "\n[SYSTEM] You have been disconnected by the Server Administrator.\n");
                close(target->socket_fd);
                target->active = false;
                server_log("ADMIN_KICK", "Kicked Client #%d ('%s').", target->id, target->name);
            } else {
                printf("[-] Client #%d not found.\n", target_id);
            }
            pthread_mutex_unlock(&clients_lock);
        }
        else if (strcasecmp(line, "/quit") == 0) {
            server_log("SHUTDOWN", "Shutdown command received from server console.");
            server_running = false;
            if (server_socket_fd >= 0) close(server_socket_fd);
            exit(0);
        }
        else {
            printf("[-] Unknown command '%s'. Type '/help' for admin commands.\n", line);
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    struct sockaddr_in server_addr;
    int opt = 1;
    pthread_t console_tid;

    // Ignore broken pipe signals when clients abruptly disconnect
    signal(SIGPIPE, SIG_IGN);

    printf("=====================================================================\n");
    printf("   MULTITHREADED PRIVATE CHAT & SUPPORT DESK SERVER (PCN LAB)        \n");
    printf("=====================================================================\n");

    // 1. Create Server Socket using socket()
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd < 0) {
        perror("[-] socket() creation failed");
        exit(EXIT_FAILURE);
    }
    server_log("INIT", "TCP socket created successfully (fd=%d).", server_socket_fd);

    // 2. Set socket options using setsockopt()
    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[-] setsockopt(SO_REUSEADDR) failed");
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Configure server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // 4. Bind socket to address and port using bind()
    if (bind(server_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] bind() failed");
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }
    server_log("INIT", "Server socket successfully bound to port %d.", port);

    // 5. Listen for incoming connections using listen()
    if (listen(server_socket_fd, 10) < 0) {
        perror("[-] listen() failed");
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }
    server_log("LISTEN", "Server listening for client connections on 0.0.0.0:%d (Backlog: 10).", port);

    // 6. Spawn Server Console thread
    if (pthread_create(&console_tid, NULL, server_console_thread, NULL) != 0) {
        perror("[-] Warning: Failed to spawn server console thread");
    } else {
        pthread_detach(console_tid);
    }

    server_log("READY", "Server is live! Logging all incoming threads, connections, and private messages.");
    printf("---------------------------------------------------------------------\n");

    // 7. Main connection acceptance loop
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_socket_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (!server_running) break;
            perror("[-] accept() failed");
            continue;
        }

        // Allocate client structure
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        if (!cli) {
            perror("[-] malloc() failed for client");
            close(client_fd);
            continue;
        }

        memset(cli, 0, sizeof(client_t));
        cli->socket_fd = client_fd;
        cli->address = client_addr;
        cli->role = ROLE_UNREGISTERED;
        cli->status = STATUS_IDLE;
        cli->partner_id = -1;
        cli->connect_time = time(NULL);
        cli->active = true;

        if (!register_client(cli)) {
            send_raw(client_fd, "[-] Server client limit reached. Try again later.\n");
            close(client_fd);
            free(cli);
            continue;
        }

        // Spawn dedicated POSIX worker thread for this client
        if (pthread_create(&(cli->thread_id), NULL, client_handler_thread, (void *)cli) != 0) {
            perror("[-] pthread_create() failed for client");
            unregister_client(cli);
            close(client_fd);
            free(cli);
            continue;
        }

        // Detach thread to allow automatic resource cleanup upon exit
        pthread_detach(cli->thread_id);
    }

    if (server_socket_fd >= 0) close(server_socket_fd);
    server_log("SHUTDOWN", "Server terminated gracefully.");
    return 0;
}

