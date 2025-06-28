#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 50
#define MAX_TOPIC_LENGTH 64

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
    char topic[MAX_TOPIC_LENGTH];
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
void broadcast_to_topic_subscribers(const char* message, const char* topic, int sender_id);
void remove_client(int client_id);
int add_client(SOCKET client_socket, struct sockaddr_in client_addr);
void print_client_info(Client* client, const char* action);
void display_topic_statistics();
int count_publishers_by_topic(const char* topic);
int count_subscribers_by_topic(const char* topic);

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
        memset(clients[i].topic, 0, MAX_TOPIC_LENGTH);
    }
    
    printf("=== Topic-Based Publisher-Subscriber Server ===\n");
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
    printf("Supporting topic-based message routing\n");
    printf("Waiting for client connections...\n");
    printf("----------------------------------------\n");
}

unsigned __stdcall handle_client(void* arg) {
    Client* client = (Client*)arg;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + 150];
    
    print_client_info(client, "Connected");
    
    // First, receive client type and topic
    int bytes_received = recv(client->socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        print_client_info(client, "Disconnected (failed to receive type and topic)");
        remove_client(client->id);
        return 0;
    }
    
    buffer[bytes_received] = '\0';
    
    // Remove newline if present
    char* newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';
    newline = strchr(buffer, '\r');
    if (newline) *newline = '\0';
    
    // Parse type and topic (format: "TYPE:TOPIC")
    char* colon = strchr(buffer, ':');
    if (colon == NULL) {
        printf("Client %d (%s) sent invalid format. Expected TYPE:TOPIC\n", client->id, client->ip_str);
        remove_client(client->id);
        return 0;
    }
    
    *colon = '\0';  // Split the string
    char* type_str = buffer;
    char* topic_str = colon + 1;
    
    // Set client type
    if (strcmp(type_str, "PUBLISHER") == 0) {
        client->type = CLIENT_PUBLISHER;
    } else if (strcmp(type_str, "SUBSCRIBER") == 0) {
        client->type = CLIENT_SUBSCRIBER;
    } else {
        printf("Client %d (%s) sent invalid type: %s\n", client->id, client->ip_str, type_str);
        remove_client(client->id);
        return 0;
    }
    
    // Set client topic
    strncpy(client->topic, topic_str, MAX_TOPIC_LENGTH - 1);
    client->topic[MAX_TOPIC_LENGTH - 1] = '\0';
    
    printf("Client %d (%s) registered as %s for topic '%s'\n", 
           client->id, client->ip_str, 
           (client->type == CLIENT_PUBLISHER ? "PUBLISHER" : "SUBSCRIBER"), 
           client->topic);
    
    // Display current topic statistics
    display_topic_statistics();
    
    // Handle messages from client
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(client->socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            print_client_info(client, "Disconnected");
            remove_client(client->id);
            display_topic_statistics();
            break;
        }
        
        buffer[bytes_received] = '\0';
        
        // Check for termination message
        if (strncmp(buffer, "terminate", 9) == 0) {
            print_client_info(client, "Terminated");
            remove_client(client->id);
            display_topic_statistics();
            break;
        }
        
        // If client is a publisher, broadcast message to subscribers of the same topic
        if (client->type == CLIENT_PUBLISHER) {
            printf("[%s] Publisher %d (%s): %s", client->topic, client->id, client->ip_str, buffer);
            
            // Create formatted message with topic and publisher info
            snprintf(message, sizeof(message), "[%s] Publisher %d: %s", client->topic, client->id, buffer);
            broadcast_to_topic_subscribers(message, client->topic, client->id);
        }
        // If subscriber sends a message, just log it
        else if (client->type == CLIENT_SUBSCRIBER) {
            printf("[%s] Subscriber %d (%s): %s", client->topic, client->id, client->ip_str, buffer);
        }
    }
    
    return 0;
}

void broadcast_to_topic_subscribers(const char* message, const char* topic, int sender_id) {
    EnterCriticalSection(&clients_mutex);
    
    int subscribers_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != INVALID_SOCKET && 
            clients[i].type == CLIENT_SUBSCRIBER && 
            clients[i].id != sender_id &&
            strcmp(clients[i].topic, topic) == 0) {
            
            int send_result = send(clients[i].socket, message, strlen(message), 0);
            if (send_result == SOCKET_ERROR) {
                printf("Failed to send message to subscriber %d\n", clients[i].id);
            } else {
                subscribers_count++;
            }
        }
    }
    
    LeaveCriticalSection(&clients_mutex);
    
    printf("Message broadcasted to %d subscribers on topic '%s'\n", subscribers_count, topic);
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
        memset(clients[client_id].topic, 0, MAX_TOPIC_LENGTH);
        client_count--;
    }
    
    LeaveCriticalSection(&clients_mutex);
}

void print_client_info(Client* client, const char* action) {
    if (strlen(client->topic) > 0) {
        printf("Client %d (%s:%d) [%s] %s\n", 
               client->id, 
               client->ip_str, 
               ntohs(client->address.sin_port),
               client->topic,
               action);
    } else {
        printf("Client %d (%s:%d) %s\n", 
               client->id, 
               client->ip_str, 
               ntohs(client->address.sin_port), 
               action);
    }
}

void display_topic_statistics() {
    EnterCriticalSection(&clients_mutex);
    
    printf("\n--- Topic Statistics ---\n");
    printf("Total connected clients: %d\n", client_count);
    
    // Count unique topics
    char topics[MAX_CLIENTS][MAX_TOPIC_LENGTH];
    int topic_count = 0;
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != INVALID_SOCKET && strlen(clients[i].topic) > 0) {
            // Check if topic already counted
            int found = 0;
            for (int j = 0; j < topic_count; j++) {
                if (strcmp(topics[j], clients[i].topic) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                strcpy(topics[topic_count], clients[i].topic);
                topic_count++;
            }
        }
    }
    
    if (topic_count > 0) {
        printf("Active topics: %d\n", topic_count);
        for (int i = 0; i < topic_count; i++) {
            int publishers = count_publishers_by_topic(topics[i]);
            int subscribers = count_subscribers_by_topic(topics[i]);
            printf("  - '%s': %d publishers, %d subscribers\n", topics[i], publishers, subscribers);
        }
    } else {
        printf("No active topics\n");
    }
    printf("------------------------\n\n");
    
    LeaveCriticalSection(&clients_mutex);
}

int count_publishers_by_topic(const char* topic) {
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != INVALID_SOCKET && 
            clients[i].type == CLIENT_PUBLISHER &&
            strcmp(clients[i].topic, topic) == 0) {
            count++;
        }
    }
    return count;
}

int count_subscribers_by_topic(const char* topic) {
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != INVALID_SOCKET && 
            clients[i].type == CLIENT_SUBSCRIBER &&
            strcmp(clients[i].topic, topic) == 0) {
            count++;
        }
    }
    return count;
}
