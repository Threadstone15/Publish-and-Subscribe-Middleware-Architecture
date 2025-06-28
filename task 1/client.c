#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#define BUFFER_SIZE 1024
#define CLIENT_TYPE_PUBLISHER 1
#define CLIENT_TYPE_SUBSCRIBER 2

// Function prototypes
int initialize_winsock(void);
SOCKET create_client_socket(void);
int connect_to_server(SOCKET sock, const char* server_ip, int port);
int send_client_type(SOCKET sock, const char* client_type);
void handle_publisher_mode(SOCKET sock);
void handle_subscriber_mode(SOCKET sock);
unsigned __stdcall receive_messages(void* socket);
void cleanup_client(SOCKET sock);

int main(int argc, char *argv[]) {
    if (!initialize_winsock()) {
        return 1;
    }

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <SERVER_IP> <PORT> <CLIENT_TYPE>\n", argv[0]);
        fprintf(stderr, "CLIENT_TYPE: PUBLISHER or SUBSCRIBER\n");
        fprintf(stderr, "Example: %s 192.168.1.100 5000 PUBLISHER\n", argv[0]);
        WSACleanup();
        return 1;
    }

    const char* server_ip = argv[1];
    int port = atoi(argv[2]);
    const char* client_type = argv[3];

    // Validate client type
    if (strcmp(client_type, "PUBLISHER") != 0 && strcmp(client_type, "SUBSCRIBER") != 0) {
        fprintf(stderr, "Error: CLIENT_TYPE must be either PUBLISHER or SUBSCRIBER\n");
        WSACleanup();
        return 1;
    }

    SOCKET sock = create_client_socket();
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    if (!connect_to_server(sock, server_ip, port)) {
        cleanup_client(sock);
        return 1;
    }

    printf("Connected to server at %s:%d as %s\n", server_ip, port, client_type);

    if (!send_client_type(sock, client_type)) {
        cleanup_client(sock);
        return 1;
    }

    // Receive welcome message
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Server: %s", buffer);
    }

    // Handle based on client type
    if (strcmp(client_type, "PUBLISHER") == 0) {
        handle_publisher_mode(sock);
    } else {
        handle_subscriber_mode(sock);
    }

    cleanup_client(sock);
    return 0;
}

int initialize_winsock(void) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("Failed to initialize Winsock. Error Code: %d\n", WSAGetLastError());
        return 0;
    }
    return 1;
}

SOCKET create_client_socket(void) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Socket creation error. Error: %d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }
    return sock;
}

int connect_to_server(SOCKET sock, const char* server_ip, int port) {
    struct sockaddr_in serv_addr;
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address / Address not supported\n");
        return 0;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed. Error: %d\n", WSAGetLastError());
        return 0;
    }

    return 1;
}

int send_client_type(SOCKET sock, const char* client_type) {
    char type_msg[BUFFER_SIZE];
    snprintf(type_msg, BUFFER_SIZE, "%s\n", client_type);
    
    if (send(sock, type_msg, strlen(type_msg), 0) == SOCKET_ERROR) {
        printf("Failed to send client type. Error: %d\n", WSAGetLastError());
        return 0;
    }
    return 1;
}

void handle_publisher_mode(SOCKET sock) {
    char buffer[BUFFER_SIZE];
    
    printf("\n=== PUBLISHER MODE ===\n");
    printf("Type messages to publish (type 'terminate' to exit):\n");

    while (1) {
        printf("Publish: ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            printf("Input error.\n");
            break;
        }

        if (send(sock, buffer, strlen(buffer), 0) == SOCKET_ERROR) {
            printf("Send failed. Error: %d\n", WSAGetLastError());
            break;
        }

        if (strncmp(buffer, "terminate", 9) == 0) {
            printf("Terminating connection...\n");
            break;
        }
    }
}

void handle_subscriber_mode(SOCKET sock) {
    char buffer[BUFFER_SIZE];
    
    printf("\n=== SUBSCRIBER MODE ===\n");
    printf("Listening for published messages (type 'terminate' to exit):\n");
    printf("You can also type messages, but they won't be broadcasted.\n");

    // Start thread to receive messages from server
    HANDLE receive_thread = (HANDLE)_beginthreadex(NULL, 0, receive_messages, (void*)sock, 0, NULL);
    if (receive_thread == NULL) {
        printf("Failed to create receive thread\n");
        return;
    }

    // Main thread handles user input
    while (1) {
        printf("Subscriber: ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            printf("Input error.\n");
            break;
        }

        if (send(sock, buffer, strlen(buffer), 0) == SOCKET_ERROR) {
            printf("Send failed. Error: %d\n", WSAGetLastError());
            break;
        }

        if (strncmp(buffer, "terminate", 9) == 0) {
            printf("Terminating connection...\n");
            break;
        }
    }

    // Wait for receive thread to finish
    WaitForSingleObject(receive_thread, 1000);
    CloseHandle(receive_thread);
}

unsigned __stdcall receive_messages(void* socket) {
    SOCKET sock = (SOCKET)socket;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while (1) {
        bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            break;  // Connection closed or error
        }
        
        buffer[bytes_received] = '\0';
        printf("\n>>> %s", buffer);
        printf("Subscriber: ");
        fflush(stdout);
    }

    return 0;
}

void cleanup_client(SOCKET sock) {
    closesocket(sock);
    WSACleanup();
}
