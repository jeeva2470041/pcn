/*
 * File: udp_client.c
 * Description: UDP Client that sends data packets to the UDP server
 *              and waits for an acknowledgement (ACK).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_PORT 9090

int main(int argc, char *argv[]) {
    int sock_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_len;
    char buffer[BUFFER_SIZE];
    char input_pkt[BUFFER_SIZE];
    const char *server_ip = (argc > 1) ? argv[1] : DEFAULT_SERVER_IP;
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;
    int packet_seq = 0;

    printf("====================================================\n");
    printf("         UDP PACKET & ACKNOWLEDGEMENT CLIENT        \n");
    printf("====================================================\n\n");

    // 1. Create UDP Socket (SOCK_DGRAM)
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[1] socket() : UDP socket created successfully (fd=%d)\n", sock_fd);

    // Set 5-second receive timeout so it doesn't hang if ACK is lost
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // 2. Configure Server Address Structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("[-] inet_pton() failed: Invalid IP address");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("[2] Target   : Configured to send to Server %s:%d\n", server_ip, port);
    printf("----------------------------------------------------\n");
    printf("Type a message to send as a packet (or type 'exit' to quit):\n\n");

    // 3. Send Packets and Receive Acknowledgements
    addr_len = sizeof(server_addr);
    while (1) {
        printf("Enter Packet Data: ");
        if (!fgets(input_pkt, sizeof(input_pkt), stdin)) {
            break;
        }

        // Remove newline character
        input_pkt[strcspn(input_pkt, "\r\n")] = '\0';

        if (strlen(input_pkt) == 0) {
            continue;
        }

        // Send Packet to Server using sendto()
        ssize_t sent = sendto(sock_fd, input_pkt, strlen(input_pkt), 0,
                              (struct sockaddr *)&server_addr, addr_len);
        if (sent < 0) {
            perror("[-] sendto() failed");
            break;
        }

        // Check for exit
        if (strcasecmp(input_pkt, "exit") == 0 || strcasecmp(input_pkt, "quit") == 0) {
            printf("[*] Exiting UDP client session...\n");
            break;
        }

        packet_seq++;
        printf("[SEND]    UDP Packet #%d sent to server: \"%s\" (%zu bytes)\n",
               packet_seq, input_pkt, strlen(input_pkt));

        // Wait for Acknowledgement (ACK) using recvfrom()
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_recv = recvfrom(sock_fd, buffer, sizeof(buffer) - 1, 0,
                                      NULL, NULL);

        if (bytes_recv < 0) {
            printf("[-] Timeout: No ACK received from server within 5 seconds.\n\n");
        } else {
            buffer[bytes_recv] = '\0';
            printf("[RECEIVE] %s\n\n", buffer);
        }
    }

    // 4. Close client socket
    close(sock_fd);
    printf("[-] UDP Client terminated.\n");
    return 0;
}
