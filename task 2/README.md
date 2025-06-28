# Publisher-Subscriber Middleware Architecture - Task 2

This implementation provides a multi-threaded Publisher-Subscriber system where multiple clients can connect to a server simultaneously, with publishers sending messages that are broadcasted to all subscribers.

## Features

1. **Multi-threaded Server**: Handles multiple concurrent client connections
2. **Publisher-Subscriber Pattern**: Clients can act as either publishers or subscribers
3. **Message Broadcasting**: Messages from publishers are sent to all connected subscribers
4. **Thread-safe Operations**: Uses critical sections for safe client management
5. **Modular Design**: Well-structured code with separate functions for different operations

## Files

- `server.c` - Multi-threaded server that manages publishers and subscribers
- `client1.c` - Client application (identical functionality to client2 and client3)
- `client2.c` - Client application (identical functionality to client1 and client3)
- `client3.c` - Client application (identical functionality to client1 and client2)
- `compile.bat` - Batch script to compile all files

## Compilation

### Method 1: Using the batch script

```cmd
compile.bat
```

### Method 2: Manual compilation

```cmd
gcc server.c -o server -lws2_32
gcc client1.c -o client1 -lws2_32
gcc client2.c -o client2 -lws2_32
gcc client3.c -o client3 -lws2_32
```

## Usage

### 1. Start the Server

```cmd
server.exe <PORT>
```

Example:

```cmd
server.exe 5000
```

### 2. Start Publisher Clients

```cmd
client1.exe <SERVER_IP> <PORT> PUBLISHER
```

Example:

```cmd
client1.exe 127.0.0.1 5000 PUBLISHER
```

### 3. Start Subscriber Clients

```cmd
client2.exe <SERVER_IP> <PORT> SUBSCRIBER
client3.exe <SERVER_IP> <PORT> SUBSCRIBER
```

Examples:

```cmd
client2.exe 127.0.0.1 5000 SUBSCRIBER
client3.exe 127.0.0.1 5000 SUBSCRIBER
```

## System Behavior

1. **Server**:

   - Displays its IP address and port when started
   - Shows when clients connect/disconnect
   - Logs all messages from publishers
   - Manages up to 50 concurrent clients

2. **Publishers**:

   - Can send messages that will be broadcasted to all subscribers
   - Do not receive messages from other publishers
   - See confirmation when messages are sent

3. **Subscribers**:
   - Receive all messages sent by publishers
   - Can send messages (though typically they just listen)
   - Messages appear with publisher identification

## Testing Scenario

1. Open 4 terminal windows
2. Terminal 1: Start server (`server.exe 5000`)
3. Terminal 2: Start publisher (`client1.exe 127.0.0.1 5000 PUBLISHER`)
4. Terminal 3: Start subscriber (`client2.exe 127.0.0.1 5000 SUBSCRIBER`)
5. Terminal 4: Start subscriber (`client3.exe 127.0.0.1 5000 SUBSCRIBER`)
6. Type messages in the publisher terminal
7. Observe messages appearing in both subscriber terminals
8. Type `terminate` in any client to disconnect

## Key Improvements from Task 1

1. **Multi-threading**: Server can handle multiple clients simultaneously
2. **Client Types**: Distinction between publishers and subscribers
3. **Message Broadcasting**: Publishers' messages are sent to all subscribers
4. **Thread Safety**: Critical sections protect shared client data
5. **Better Error Handling**: More robust error checking and cleanup
6. **Modular Design**: Functions are separated for better maintainability

## Technical Details

- **Threading**: Uses Windows threading (`_beginthreadex`)
- **Synchronization**: Critical sections for thread-safe operations
- **Socket Programming**: Windows Sockets API (Winsock2)
- **Memory Management**: Proper cleanup and resource management
- **Client Management**: Dynamic client array with efficient add/remove operations
