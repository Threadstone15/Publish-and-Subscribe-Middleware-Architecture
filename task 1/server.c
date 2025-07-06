#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("Failed to initialize Winsock. Error Code : %d\n", WSAGetLastError());
        return 1;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <PORT>\n", argv[0]);
        WSACleanup();
        return 1;
    }

    int server_fd, new_socket;
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE];
    int port = atoi(argv[1]);
    int addrlen = sizeof(address);

    // 1. Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed. Error: %d\n", WSAGetLastError());
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // 2. Bind socket to port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // Accept any IP
    address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("Bind failed. Error: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // 3. Listen
    if (listen(server_fd, 5) < 0) {
        printf("Listen failed. Error: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Get and display server IP address
    char hostname[256];
    struct hostent *host_info;
    
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        host_info = gethostbyname(hostname);
        if (host_info != NULL) {
            struct in_addr addr;
            addr.s_addr = *((unsigned long *)host_info->h_addr);
            printf("Server started on IP: %s, Port: %d\n", inet_ntoa(addr), port);
        } else {
            printf("Server listening on port %d...\n", port);
        }
    } else {
        printf("Server listening on port %d...\n", port);
    }

    // 4. Accept connection
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
        printf("Accept failed. Error: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("Client connected.\n");

    // 5. Receive messages
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = recv(new_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0) break;

        buffer[bytes_read] = '\0';
        printf("Client: %s", buffer);
    }

    closesocket(new_socket);
    closesocket(server_fd);
    WSACleanup();
    return 0;
}
