/*
 * ============================================================================
 * File: client.c
 * Project: Multithreaded Private Chat & Support Desk Client
 * Description:
 *   - Multithreaded TCP Client using POSIX threads (pthread).
 *   - Receiver Thread handles asynchronous incoming messages without blocking.
 *   - Main Thread reads user input from stdin and transmits across the socket.
 *   - Interactive role selection:
 *       1. Customer / Client (Can request support & chat privately)
 *       2. Support Team Agent (Can list requests, accept, & handle clients)
 *   - Clean prompt interface with graceful disconnection handling.
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
#include <signal.h>
#include <stdbool.h>

#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_PORT 8080
#define BUFFER_SIZE 2048
#define NAME_LEN 64

// Client state
static int sock_fd = -1;
static volatile bool client_running = true;
static char client_name[NAME_LEN];
static int selected_role = 1; // 1 = Customer, 2 = Support Team

// Receiver Thread Function
void *receive_handler(void *arg) {
    (void)arg;
    char buffer[BUFFER_SIZE];

    while (client_running) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_read <= 0) {
            if (client_running) {
                printf("\n\n[ALERT] Connection terminated by the server.\n");
                client_running = false;
            }
            break;
        }

        buffer[bytes_read] = '\0';

        // Print message and refresh prompt
        printf("\r\033[K%s", buffer);
        if (buffer[strlen(buffer) - 1] != '\n' && buffer[strlen(buffer) - 1] != ' ') {
            printf("\n");
        }
        if (client_running) {
            printf("> ");
            fflush(stdout);
        }
    }

    return NULL;
}

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    (void)sig;
    printf("\n[SYSTEM] Exiting client...\n");
    client_running = false;
    if (sock_fd >= 0) {
        send(sock_fd, "/exit\n", 6, 0);
        close(sock_fd);
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    const char *server_ip = (argc > 1) ? argv[1] : DEFAULT_SERVER_IP;
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;
    struct sockaddr_in server_addr;
    pthread_t recv_tid;
    char input_line[BUFFER_SIZE];

    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    printf("=====================================================================\n");
    printf("   MULTITHREADED PRIVATE CHAT & SUPPORT DESK CLIENT (PCN LAB)        \n");
    printf("=====================================================================\n");

    // 1. Create client socket using socket()
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] Socket created (AF_INET, SOCK_STREAM)...\n");

    // 2. Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("[-] Invalid IP address or address not supported");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Connect to server using connect()
    printf("[SYSTEM] Connecting to server at %s:%d...\n", server_ip, port);
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] connect() failed. Is the server running?");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("[OK] Successfully connected to server!\n");
    printf("---------------------------------------------------------------------\n");

    // 4. Interactive user registration
    printf("Enter your Name / Handle: ");
    if (!fgets(client_name, sizeof(client_name), stdin)) {
        strcpy(client_name, "Anonymous");
    }
    while (strlen(client_name) > 0 && (client_name[strlen(client_name) - 1] == '\n' || client_name[strlen(client_name) - 1] == '\r')) {
        client_name[strlen(client_name) - 1] = '\0';
    }
    if (strlen(client_name) == 0) strcpy(client_name, "Anonymous");

    printf("\nChoose your Role:\n");
    printf("  [1] Customer / Client (Chat privately & request support)\n");
    printf("  [2] Support Team Agent (Handle client requests & private chat)\n");
    printf("Enter choice (1 or 2): ");

    char role_input[16];
    if (fgets(role_input, sizeof(role_input), stdin)) {
        selected_role = atoi(role_input);
        if (selected_role != 1 && selected_role != 2) selected_role = 1;
    } else {
        selected_role = 1;
    }

    // 5. Send registration packet to server
    char reg_packet[128];
    if (selected_role == 2) {
        snprintf(reg_packet, sizeof(reg_packet), "ROLE:SUPPORT:%s\n", client_name);
        printf("[OK] Registering as Support Team Agent '%s'...\n", client_name);
    } else {
        snprintf(reg_packet, sizeof(reg_packet), "ROLE:CUSTOMER:%s\n", client_name);
        printf("[OK] Registering as Customer / Client '%s'...\n", client_name);
    }
    send(sock_fd, reg_packet, strlen(reg_packet), 0);

    // 6. Spawn Receiver Thread for asynchronous reception
    if (pthread_create(&recv_tid, NULL, receive_handler, NULL) != 0) {
        perror("[-] Failed to create receiver thread");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    pthread_detach(recv_tid);

    printf("---------------------------------------------------------------------\n");
    printf("Type '/help' to view available commands, or start chatting below.\n");
    printf("---------------------------------------------------------------------\n");

    // 7. Main loop for sending input (Sender Thread)
    while (client_running) {
        printf("> ");
        fflush(stdout);

        if (!fgets(input_line, sizeof(input_line), stdin)) {
            break;
        }

        // Trim trailing newline
        while (strlen(input_line) > 0 && (input_line[strlen(input_line) - 1] == '\n' || input_line[strlen(input_line) - 1] == '\r')) {
            input_line[strlen(input_line) - 1] = '\0';
        }

        if (strlen(input_line) == 0) continue;

        // Check for exit command
        if (strcasecmp(input_line, "/exit") == 0 || strcasecmp(input_line, "/quit") == 0) {
            send(sock_fd, "/exit\n", 6, 0);
            printf("[SYSTEM] Disconnecting from server...\n");
            client_running = false;
            break;
        }

        // Send line to server
        char send_buf[BUFFER_SIZE + 2];
        snprintf(send_buf, sizeof(send_buf), "%s\n", input_line);
        if (send(sock_fd, send_buf, strlen(send_buf), 0) < 0) {
            perror("[-] send() failed");
            break;
        }

        // Slight pause to allow incoming prompt rendering if server echoes immediately
        usleep(15000);
    }

    client_running = false;
    if (sock_fd >= 0) close(sock_fd);
    printf("[SYSTEM] Session finished.\n");
    return 0;
}

