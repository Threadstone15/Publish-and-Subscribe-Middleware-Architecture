#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 50

typedef enum {
    CLIENT_UNKNOWN = 0,
    CLIENT_PUBLISHER = 1,
    CLIENT_SUBSCRIBER = 2
} ClientType;

typedef struct {
    SOCKET socket;
    struct sockaddr_in address;
    ClientType type;
    int id;
    char ip_str[INET_ADDRSTRLEN];
} Client;

// Global variables
Client clients[MAX_CLIENTS];
int client_count = 0;
CRITICAL_SECTION clients_mutex;

// Function prototypes
void initialize_server();
void cleanup_server();
SOCKET create_server_socket(int port);
void display_server_info(int port);
unsigned __stdcall handle_client(void* arg);
void broadcast_to_subscribers(const char* message, int sender_id);
void remove_client(int client_id);
int add_client(SOCKET client_socket, struct sockaddr_in client_addr);
void print_client_info(Client* client, const char* action);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <PORT>\n", argv[0]);
        return 1;
    }

    initialize_server();
    
    int port = atoi(argv[1]);
    SOCKET server_socket = create_server_socket(port);
    
    display_server_info(port);
    
    // Accept connections loop
    while (1) {
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        
        SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket == INVALID_SOCKET) {
            printf("Accept failed. Error: %d\n", WSAGetLastError());
            continue;
        }
        
        int client_id = add_client(client_socket, client_addr);
        if (client_id == -1) {
            printf("Maximum clients reached. Rejecting connection.\n");
            closesocket(client_socket);
            continue;
        }
        
        // Create thread to handle client
        HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, handle_client, &clients[client_id], 0, NULL);
        if (thread == NULL) {
            printf("Failed to create thread for client %d\n", client_id);
            remove_client(client_id);
        } else {
            CloseHandle(thread);
        }
    }
    
    closesocket(server_socket);
    cleanup_server();
    return 0;
}

void initialize_server() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("Failed to initialize Winsock. Error Code : %d\n", WSAGetLastError());
        exit(1);
    }
    
    InitializeCriticalSection(&clients_mutex);
    
    // Initialize clients array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = INVALID_SOCKET;
        clients[i].type = CLIENT_UNKNOWN;
        clients[i].id = -1;
    }
    
    printf("=== Publisher-Subscriber Server ===\n");
}

void cleanup_server() {
    DeleteCriticalSection(&clients_mutex);
    WSACleanup();
}

SOCKET create_server_socket(int port) {
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed. Error: %d\n", WSAGetLastError());
        exit(1);
    }
    
    // Set socket option to reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        printf("Setsockopt failed. Error: %d\n", WSAGetLastError());
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Bind failed. Error: %d\n", WSAGetLastError());
        exit(1);
    }
    
    if (listen(server_socket, 5) < 0) {
        printf("Listen failed. Error: %d\n", WSAGetLastError());
        exit(1);
    }
    
    return server_socket;
}

void display_server_info(int port) {
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
    printf("Waiting for client connections...\n");
    printf("----------------------------------------\n");
}

unsigned __stdcall handle_client(void* arg) {
    Client* client = (Client*)arg;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + 100];
    
    print_client_info(client, "Connected");
    
    // First, receive client type
    int bytes_received = recv(client->socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        print_client_info(client, "Disconnected (failed to receive type)");
        remove_client(client->id);
        return 0;
    }
    
    buffer[bytes_received] = '\0';
    
    // Remove newline if present
    char* newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';
    newline = strchr(buffer, '\r');
    if (newline) *newline = '\0';
    
    if (strcmp(buffer, "PUBLISHER") == 0) {
        client->type = CLIENT_PUBLISHER;
        printf("Client %d (%s) registered as PUBLISHER\n", client->id, client->ip_str);
    } else if (strcmp(buffer, "SUBSCRIBER") == 0) {
        client->type = CLIENT_SUBSCRIBER;
        printf("Client %d (%s) registered as SUBSCRIBER\n", client->ip_str, client->ip_str);
    } else {
        printf("Client %d (%s) sent invalid type: %s\n", client->id, client->ip_str, buffer);
        remove_client(client->id);
        return 0;
    }
    
    // Handle messages from client
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(client->socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            print_client_info(client, "Disconnected");
            remove_client(client->id);
            break;
        }
        
        buffer[bytes_received] = '\0';
        
        // Check for termination message
        if (strncmp(buffer, "terminate", 9) == 0) {
            print_client_info(client, "Terminated");
            remove_client(client->id);
            break;
        }
        
        // If client is a publisher, broadcast message to subscribers
        if (client->type == CLIENT_PUBLISHER) {
            printf("Publisher %d (%s): %s", client->id, client->ip_str, buffer);
            
            // Create formatted message
            snprintf(message, sizeof(message), "Publisher %d: %s", client->id, buffer);
            broadcast_to_subscribers(message, client->id);
        }
        // If subscriber sends a message, just log it (subscribers typically don't send messages)
        else if (client->type == CLIENT_SUBSCRIBER) {
            printf("Subscriber %d (%s): %s", client->id, client->ip_str, buffer);
        }
    }
    
    return 0;
}

void broadcast_to_subscribers(const char* message, int sender_id) {
    EnterCriticalSection(&clients_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != INVALID_SOCKET && 
            clients[i].type == CLIENT_SUBSCRIBER && 
            clients[i].id != sender_id) {
            
            int send_result = send(clients[i].socket, message, strlen(message), 0);
            if (send_result == SOCKET_ERROR) {
                printf("Failed to send message to subscriber %d\n", clients[i].id);
            }
        }
    }
    
    LeaveCriticalSection(&clients_mutex);
}

int add_client(SOCKET client_socket, struct sockaddr_in client_addr) {
    EnterCriticalSection(&clients_mutex);
    
    int client_id = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == INVALID_SOCKET) {
            clients[i].socket = client_socket;
            clients[i].address = client_addr;
            clients[i].type = CLIENT_UNKNOWN;
            clients[i].id = i;
            
            // Convert IP to string
            inet_ntop(AF_INET, &client_addr.sin_addr, clients[i].ip_str, INET_ADDRSTRLEN);
            
            client_count++;
            client_id = i;
            break;
        }
    }
    
    LeaveCriticalSection(&clients_mutex);
    return client_id;
}

void remove_client(int client_id) {
    if (client_id < 0 || client_id >= MAX_CLIENTS) return;
    
    EnterCriticalSection(&clients_mutex);
    
    if (clients[client_id].socket != INVALID_SOCKET) {
        closesocket(clients[client_id].socket);
        clients[client_id].socket = INVALID_SOCKET;
        clients[client_id].type = CLIENT_UNKNOWN;
        clients[client_id].id = -1;
        client_count--;
    }
    
    LeaveCriticalSection(&clients_mutex);
}

void print_client_info(Client* client, const char* action) {
    printf("Client %d (%s:%d) %s\n", 
           client->id, 
           client->ip_str, 
           ntohs(client->address.sin_port), 
           action);
}
