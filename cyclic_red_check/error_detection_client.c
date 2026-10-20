/*
 * File: error_detection_client.c
 * Assignment 6: Interactive TCP Error Detection Client for Text Strings
 * Description: Prompts user to type a text string via keyboard, converts to 8-bit words / binary,
 *              computes Checksum & CRC, and transmits over TCP with optional error injection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 2048
#define GENERATOR "10011" // G(x) = x^4 + x + 1

// Convert text string to binary bitstream
void text_to_bits(const char *text, char *bits_out) {
    bits_out[0] = '\0';
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)text[i];
        for (int b = 7; b >= 0; b--) {
            char bit = ((ch >> b) & 1) ? '1' : '0';
            strncat(bits_out, &bit, 1);
        }
    }
}

// Modulo-2 XOR binary division for CRC
void crc_compute(const char *data_bits, const char *gen, char *remainder) {
    int data_len = strlen(data_bits);
    int gen_len = strlen(gen);
    char augmented[BUFFER_SIZE];
    
    // Append (gen_len - 1) zeros
    strcpy(augmented, data_bits);
    for (int i = 0; i < gen_len - 1; i++) {
        augmented[data_len + i] = '0';
    }
    augmented[data_len + gen_len - 1] = '\0';

    int total_len = strlen(augmented);
    for (int i = 0; i <= total_len - gen_len; i++) {
        if (augmented[i] == '1') {
            for (int j = 0; j < gen_len; j++) {
                augmented[i + j] = (augmented[i + j] == gen[j]) ? '0' : '1';
            }
        }
    }
    strncpy(remainder, augmented + total_len - (gen_len - 1), gen_len - 1);
    remainder[gen_len - 1] = '\0';
}

// 8-bit Checksum Generator over string characters
unsigned char compute_checksum(const char *text) {
    unsigned int sum = 0;
    int len = strlen(text);
    printf("\n[*] [SENDER] Dividing string \"%s\" into %d 8-bit ASCII words:\n", text, len);
    for (int i = 0; i < len; i++) {
        unsigned char val = (unsigned char)text[i];
        sum += val;
        while (sum >> 8) {
            sum = (sum & 0xFF) + (sum >> 8); // Wrap end-around carry
        }
        printf("    -> Word[%d]: '%c' (0x%02X) | Running Sum: 0x%02X\n", i, text[i], val, sum & 0xFF);
    }
    unsigned char checksum = (~sum) & 0xFF;
    printf("    -> Sum: 0x%02X | Generated Checksum (~Sum): 0x%02X\n", sum & 0xFF, checksum);
    return checksum;
}

int send_frame_to_server(const char *payload, char *response_out) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Connect failed");
        return -1;
    }
    send(sock_fd, payload, strlen(payload), 0);
    bzero(response_out, BUFFER_SIZE);
    recv(sock_fd, response_out, BUFFER_SIZE - 1, 0);
    close(sock_fd);
    return 0;
}

int main() {
    int choice;
    char input_text[256];
    char response[BUFFER_SIZE];

    printf("====================================================\n");
    printf("   STRING ERROR DETECTION CLIENT (CHECKSUM & CRC)   \n");
    printf("====================================================\n");

    while (1) {
        printf("\nSelect Scheme to Test:\n");
        printf("  1. 8-bit Checksum (Error-Free Transmission)\n");
        printf("  2. 8-bit Checksum (With Injected Character/Bit Error)\n");
        printf("  3. Cyclic Redundancy Check CRC-4 (Error-Free Transmission)\n");
        printf("  4. Cyclic Redundancy Check CRC-4 (With Injected Bit Error)\n");
        printf("  5. Exit\n");
        printf("Enter Choice (1-5): ");
        if (scanf("%d", &choice) != 1) break;
        getchar(); // consume newline
        if (choice == 5) break;

        printf("Enter Text String to Transmit: ");
        if (!fgets(input_text, sizeof(input_text), stdin)) break;
        input_text[strcspn(input_text, "\r\n")] = 0;
        if (strlen(input_text) == 0) strcpy(input_text, "NETWORKS");

        if (choice == 1 || choice == 2) {
            unsigned char chk = compute_checksum(input_text);
            char tx_message[256];
            strcpy(tx_message, input_text);

            if (choice == 2) {
                printf("\n[!] [ERROR INJECTION] Manually corrupting character '%c' -> '%c'...\n",
                       tx_message[1], tx_message[1] ^ 0x08);
                tx_message[1] ^= 0x08; // Flip bit in character 1
                printf("    -> Corrupted Transmitted String: \"%s\"\n", tx_message);
            }

            char payload[BUFFER_SIZE];
            snprintf(payload, sizeof(payload), "CHECKSUM %s %02X", tx_message, chk);

            printf("\n[->] Transmitting Frame over TCP: \"%s\"\n", payload);
            send_frame_to_server(payload, response);
            printf("[<-] Server Verification Result: %s\n", response);

        } else if (choice == 3 || choice == 4) {
            char data_bits[BUFFER_SIZE];
            text_to_bits(input_text, data_bits);
            char remainder[16];
            crc_compute(data_bits, GENERATOR, remainder);

            printf("\n[*] [SENDER] Converting String \"%s\" to Binary (%d bits):\n", input_text, (int)strlen(data_bits));
            printf("    -> Binary Data Stream    : %s\n", data_bits);
            printf("    -> Generator Poly G(x)   : %s (x^4 + x + 1)\n", GENERATOR);
            printf("    -> Computed Remainder R  : %s\n", remainder);

            char codeword[BUFFER_SIZE];
            snprintf(codeword, sizeof(codeword), "%s%s", data_bits, remainder);
            printf("    -> Transmitted Codeword T: %s\n", codeword);

            if (choice == 4) {
                printf("\n[!] [ERROR INJECTION] Inverting Bit at index 6 ('%c' -> '%c')...\n",
                       codeword[6], (codeword[6] == '0') ? '1' : '0');
                codeword[6] = (codeword[6] == '0') ? '1' : '0';
                printf("    -> Corrupted Codeword T' : %s\n", codeword);
            }

            char payload[BUFFER_SIZE];
            snprintf(payload, sizeof(payload), "CRC %d %s", (int)strlen(input_text), codeword);

            printf("\n[->] Transmitting Codeword over TCP: \"CRC %d [Bitstream]\"\n", (int)strlen(input_text));
            send_frame_to_server(payload, response);
            printf("[<-] Server Verification Result: %s\n", response);
        }
    }

    printf("\n[*] Client terminated.\n");
    return 0;
}
