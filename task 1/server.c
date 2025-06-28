#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 50
#define CLIENT_TYPE_PUBLISHER 1
#define CLIENT_TYPE_SUBSCRIBER 2

// Client structure
typedef struct {
    SOCKET socket;
    int client_id;
    int client_type;  // 1 = Publisher, 2 = Subscriber
    struct sockaddr_in address;
    char ip_str[INET_ADDRSTRLEN];
} client_info_t;

// Global variables
client_info_t clients[MAX_CLIENTS];
int client_count = 0;
CRITICAL_SECTION client_mutex;

// Function prototypes
int initialize_winsock(void);
SOCKET create_server_socket(int port);
void display_server_info(int port);
unsigned __stdcall handle_client(void* client_socket);
void broadcast_to_subscribers(const char* message, int sender_id);
void remove_client(int client_id);
int get_client_index(int client_id);
void cleanup_server(SOCKET server_fd);

int main(int argc, char *argv[]) {
    if (!initialize_winsock()) {
        return 1;
    }
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <PORT>\n", argv[0]);
        WSACleanup();
        return 1;
    }

    int port = atoi(argv[1]);
    SOCKET server_fd = create_server_socket(port);
    if (server_fd == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    display_server_info(port);
    
    // Initialize critical section for thread safety
    InitializeCriticalSection(&client_mutex);

    printf("Waiting for client connections...\n");

    while (1) {
        struct sockaddr_in client_addr;
        int addrlen = sizeof(client_addr);
        
        SOCKET client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_socket == INVALID_SOCKET) {
            printf("Accept failed. Error: %d\n", WSAGetLastError());
            continue;
        }

        // Create thread to handle client
        HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, handle_client, (void*)client_socket, 0, NULL);
        if (thread == NULL) {
            printf("Failed to create thread for client\n");
            closesocket(client_socket);
        } else {
            CloseHandle(thread);
        }
    }

    cleanup_server(server_fd);
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

SOCKET create_server_socket(int port) {
    SOCKET server_fd;
    struct sockaddr_in address;

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        printf("Socket creation failed. Error: %d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }

    // Allow socket reuse
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        printf("Setsockopt failed\n");
    }

    // Bind socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("Bind failed. Error: %d\n", WSAGetLastError());
        closesocket(server_fd);
        return INVALID_SOCKET;
    }

    // Listen
    if (listen(server_fd, 5) == SOCKET_ERROR) {
        printf("Listen failed. Error: %d\n", WSAGetLastError());
        closesocket(server_fd);
        return INVALID_SOCKET;
    }

    return server_fd;
}

void display_server_info(int port) {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct hostent* host_info = gethostbyname(hostname);
        if (host_info != NULL) {
            struct in_addr addr;
            addr.s_addr = *((unsigned long*)host_info->h_addr);
            printf("=== Pub-Sub Server Started ===\n");
            printf("Server IP: %s\n", inet_ntoa(addr));
            printf("Server Port: %d\n", port);
            printf("==============================\n");
        } else {
            printf("Server listening on port %d\n", port);
        }
    } else {
        printf("Server listening on port %d\n", port);
    }
}

unsigned __stdcall handle_client(void* client_socket) {
    SOCKET sock = (SOCKET)client_socket;
    char buffer[BUFFER_SIZE];
    char client_type_str[20];
    int client_id;
    
    // Get client address info
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    getpeername(sock, (struct sockaddr*)&client_addr, &addr_len);
    char* client_ip = inet_ntoa(client_addr.sin_addr);

    // Receive client type (PUBLISHER or SUBSCRIBER)
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        closesocket(sock);
        return 0;
    }
    
    buffer[bytes_received] = '\0';
    
    // Remove newline if present
    char* newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';
    newline = strchr(buffer, '\r');
    if (newline) *newline = '\0';

    // Determine client type
    int client_type;
    if (strncmp(buffer, "PUBLISHER", 9) == 0) {
        client_type = CLIENT_TYPE_PUBLISHER;
        strcpy(client_type_str, "PUBLISHER");
    } else if (strncmp(buffer, "SUBSCRIBER", 10) == 0) {
        client_type = CLIENT_TYPE_SUBSCRIBER;
        strcpy(client_type_str, "SUBSCRIBER");
    } else {
        printf("Invalid client type received: %s\n", buffer);
        closesocket(sock);
        return 0;
    }

    // Add client to the list
    EnterCriticalSection(&client_mutex);
    if (client_count < MAX_CLIENTS) {
        client_id = client_count;
        clients[client_count].socket = sock;
        clients[client_count].client_id = client_count;
        clients[client_count].client_type = client_type;
        clients[client_count].address = client_addr;
        strcpy(clients[client_count].ip_str, client_ip);
        client_count++;
        printf("[%s] Client #%d connected from %s\n", client_type_str, client_id, client_ip);
    } else {
        printf("Maximum clients reached. Connection rejected.\n");
        LeaveCriticalSection(&client_mutex);
        closesocket(sock);
        return 0;
    }
    LeaveCriticalSection(&client_mutex);

    // Send welcome message
    char welcome_msg[BUFFER_SIZE];
    snprintf(welcome_msg, BUFFER_SIZE, "Connected as %s (ID: %d)\n", client_type_str, client_id);
    send(sock, welcome_msg, strlen(welcome_msg), 0);

    // Handle client messages
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            break;  // Client disconnected
        }
        
        buffer[bytes_received] = '\0';
        
        // Check for termination
        if (strncmp(buffer, "terminate", 9) == 0) {
            printf("[%s] Client #%d terminating connection\n", client_type_str, client_id);
            break;
        }
        
        // Display message on server
        printf("[%s #%d]: %s", client_type_str, client_id, buffer);
        
        // If it's a publisher, broadcast to all subscribers
        if (client_type == CLIENT_TYPE_PUBLISHER) {
            char broadcast_msg[BUFFER_SIZE + 50];
            snprintf(broadcast_msg, sizeof(broadcast_msg), "[Publisher #%d]: %s", client_id, buffer);
            broadcast_to_subscribers(broadcast_msg, client_id);
        }
    }

    // Clean up
    printf("[%s] Client #%d from %s disconnected\n", client_type_str, client_id, client_ip);
    remove_client(client_id);
    closesocket(sock);
    return 0;
}

void broadcast_to_subscribers(const char* message, int sender_id) {
    EnterCriticalSection(&client_mutex);
    
    for (int i = 0; i < client_count; i++) {
        if (clients[i].client_type == CLIENT_TYPE_SUBSCRIBER && 
            clients[i].client_id != sender_id) {
            
            int result = send(clients[i].socket, message, strlen(message), 0);
            if (result == SOCKET_ERROR) {
                printf("Failed to send message to subscriber #%d\n", clients[i].client_id);
            }
        }
    }
    
    LeaveCriticalSection(&client_mutex);
}

void remove_client(int client_id) {
    EnterCriticalSection(&client_mutex);
    
    int index = get_client_index(client_id);
    if (index != -1) {
        // Shift remaining clients
        for (int i = index; i < client_count - 1; i++) {
            clients[i] = clients[i + 1];
        }
        client_count--;
    }
    
    LeaveCriticalSection(&client_mutex);
}

int get_client_index(int client_id) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].client_id == client_id) {
            return i;
        }
    }
    return -1;
}

void cleanup_server(SOCKET server_fd) {
    DeleteCriticalSection(&client_mutex);
    closesocket(server_fd);
    WSACleanup();
}
