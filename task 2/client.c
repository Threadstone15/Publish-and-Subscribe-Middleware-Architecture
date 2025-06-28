#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024

typedef enum {
    CLIENT_PUBLISHER = 1,
    CLIENT_SUBSCRIBER = 2
} ClientType;

// Global variables
SOCKET client_socket;
ClientType client_type;
volatile int running = 1;

// Function prototypes
void initialize_client();
void cleanup_client();
SOCKET connect_to_server(const char* server_ip, int port);
ClientType parse_client_type(const char* type_str);
const char* client_type_to_string(ClientType type);
void send_client_type();
unsigned __stdcall receive_messages(void* arg);
void handle_user_input();
void print_usage(const char* program_name);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* server_ip = argv[1];
    int port = atoi(argv[2]);
    client_type = parse_client_type(argv[3]);
    
    if (client_type == 0) {
        fprintf(stderr, "Error: Client type must be either 'PUBLISHER' or 'SUBSCRIBER'\n");
        print_usage(argv[0]);
        return 1;
    }
    
    initialize_client();
    
    client_socket = connect_to_server(server_ip, port);
    
    printf("Connected to server at %s:%d as %s\n", 
           server_ip, port, client_type_to_string(client_type));
    
    // Send client type to server
    send_client_type();
    
    // Create thread to receive messages (for subscribers)
    HANDLE receive_thread = NULL;
    if (client_type == CLIENT_SUBSCRIBER) {
        receive_thread = (HANDLE)_beginthreadex(NULL, 0, receive_messages, NULL, 0, NULL);
        if (receive_thread == NULL) {
            printf("Failed to create receive thread\n");
            cleanup_client();
            return 1;
        }
        printf("Listening for messages from publishers...\n");
        printf("Type 'terminate' to exit.\n");
    } else {
        printf("You can now send messages. Type 'terminate' to exit.\n");
    }
    
    // Handle user input
    handle_user_input();
    
    // Cleanup
    running = 0;
    if (receive_thread) {
        WaitForSingleObject(receive_thread, 1000);
        CloseHandle(receive_thread);
    }
    
    cleanup_client();
    return 0;
}

void initialize_client() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("Failed to initialize Winsock. Error Code : %d\n", WSAGetLastError());
        exit(1);
    }
    
    printf("=== Publisher-Subscriber Client ===\n");
}

void cleanup_client() {
    if (client_socket != INVALID_SOCKET) {
        closesocket(client_socket);
    }
    WSACleanup();
}

SOCKET connect_to_server(const char* server_ip, int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Socket creation failed. Error: %d\n", WSAGetLastError());
        cleanup_client();
        exit(1);
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid address / Address not supported\n");
        cleanup_client();
        exit(1);
    }
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Connection failed. Error: %d\n", WSAGetLastError());
        cleanup_client();
        exit(1);
    }
    
    return sock;
}

ClientType parse_client_type(const char* type_str) {
    if (strcmp(type_str, "PUBLISHER") == 0) {
        return CLIENT_PUBLISHER;
    } else if (strcmp(type_str, "SUBSCRIBER") == 0) {
        return CLIENT_SUBSCRIBER;
    }
    return 0;
}

const char* client_type_to_string(ClientType type) {
    switch (type) {
        case CLIENT_PUBLISHER: return "PUBLISHER";
        case CLIENT_SUBSCRIBER: return "SUBSCRIBER";
        default: return "UNKNOWN";
    }
}

void send_client_type() {
    const char* type_str = client_type_to_string(client_type);
    char message[64];
    snprintf(message, sizeof(message), "%s\n", type_str);
    
    int send_result = send(client_socket, message, strlen(message), 0);
    if (send_result == SOCKET_ERROR) {
        printf("Failed to send client type. Error: %d\n", WSAGetLastError());
        cleanup_client();
        exit(1);
    }
}

unsigned __stdcall receive_messages(void* arg) {
    char buffer[BUFFER_SIZE];
    
    while (running) {
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            if (running) {
                printf("\nServer disconnected.\n");
            }
            break;
        }
        
        buffer[bytes_received] = '\0';
        printf("\n[Received] %s", buffer);
        
        // Re-prompt for user input if we're still running
        if (running) {
            printf("You: ");
            fflush(stdout);
        }
    }
    
    return 0;
}

void handle_user_input() {
    char buffer[BUFFER_SIZE];
    
    while (running) {
        printf("You: ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            printf("Input error.\n");
            break;
        }
        
        // Check for terminate command
        if (strncmp(buffer, "terminate", 9) == 0) {
            printf("Terminating connection...\n");
            send(client_socket, buffer, strlen(buffer), 0);
            break;
        }
        
        // Send message to server
        int send_result = send(client_socket, buffer, strlen(buffer), 0);
        if (send_result == SOCKET_ERROR) {
            printf("Failed to send message. Error: %d\n", WSAGetLastError());
            break;
        }
        
        // For publishers, show confirmation
        if (client_type == CLIENT_PUBLISHER) {
            printf("Message sent to all subscribers.\n");
        }
    }
}

void print_usage(const char* program_name) {
    printf("Usage: %s <SERVER_IP> <PORT> <CLIENT_TYPE>\n", program_name);
    printf("CLIENT_TYPE must be either 'PUBLISHER' or 'SUBSCRIBER'\n");
    printf("Examples:\n");
    printf("  %s 192.168.10.2 5000 PUBLISHER\n", program_name);
    printf("  %s 192.168.10.2 5000 SUBSCRIBER\n", program_name);
}
