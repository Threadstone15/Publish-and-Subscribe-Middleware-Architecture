# Topic-Based Publisher-Subscriber Middleware Architecture - Task 3

This implementation extends the Publisher-Subscriber system to include **topic-based message filtering**, allowing publishers and subscribers to work with specific topics/subjects for more targeted message routing.

## Features

1. **Topic-Based Message Filtering**: Messages are only delivered to subscribers interested in the same topic
2. **Multi-threaded Server**: Handles multiple concurrent client connections with different topics
3. **Fourth Command Line Argument**: Clients specify their topic of interest
4. **Concurrent Different Topics**: Multiple publishers and subscribers can work with different topics simultaneously
5. **Topic Statistics**: Server displays real-time statistics about active topics and client counts
6. **Enhanced Logging**: Server logs show topic information for better visibility

## Files

- `server.c` - Topic-aware multi-threaded server
- `client.c` - Generic client application with topic support
- `compile.bat` - Batch script to compile all files

## Key Improvements from Task 2

1. **Topic Support**: Added fourth command line argument for topic specification
2. **Message Filtering**: Only subscribers of matching topics receive messages
3. **Topic Statistics**: Real-time display of active topics and participant counts
4. **Enhanced Protocol**: Client sends both type and topic to server
5. **Better Message Formatting**: Messages include topic information

## Compilation

### Method 1: Using the batch script

```cmd
.\compile.bat
```

### Method 2: Manual compilation

```cmd
gcc server.c -o server -lws2_32
gcc client.c -o client -lws2_32
```

## Usage

### Command Line Format

```
<executable> <SERVER_IP> <PORT> <CLIENT_TYPE> <TOPIC>
```

### Examples

#### 1. Start the Server

```cmd
server.exe 5000
```

#### 2. Start Publishers for Different Topics

```cmd
client.exe 127.0.0.1 5000 PUBLISHER SPORTS
```

#### 3. Start Subscribers for Different Topics

```cmd
client.exe 127.0.0.1 5000 SUBSCRIBER SPORTS
```

## System Behavior

### Server

- **Displays IP and port** when started
- **Shows topic statistics** when clients connect/disconnect
- **Logs messages by topic** with format `[TOPIC] Publisher X: message`
- **Routes messages** only to subscribers of the same topic
- **Tracks active topics** and participant counts

### Publishers

- **Specify topic** as fourth command line argument
- **Send messages** that are routed only to subscribers of the same topic
- **See confirmation** when messages are published
- **Topic displayed** in client interface

### Subscribers

- **Specify topic** as fourth command line argument
- **Receive messages** only from publishers of the same topic
- **Messages formatted** with topic and publisher information
- **Real-time updates** from multiple publishers on the same topic

## Testing Scenarios

### Scenario 1: Single Topic with Multiple Clients

1. Start server: `server.exe 5000`
2. Start publisher: `client.exe 127.0.0.1 5000 PUBLISHER SPORTS`
3. Start subscribers:
   - `client.exe 127.0.0.1 5000 SUBSCRIBER SPORTS`
   - `client.exe 127.0.0.1 5000 SUBSCRIBER SPORTS`
4. Send messages from publisher - both subscribers receive them

### Scenario 2: Multiple Topics Concurrently

1. Start server: `server.exe 5000`
2. Start SPORTS publisher: `client.exe 127.0.0.1 5000 PUBLISHER SPORTS`
3. Start NEWS publisher: `client.exe 127.0.0.1 5000 PUBLISHER NEWS`
4. Start SPORTS subscriber: `client.exe 127.0.0.1 5000 SUBSCRIBER SPORTS`
5. Start NEWS subscriber: `client.exe 127.0.0.1 5000 SUBSCRIBER NEWS`
6. Send messages from both publishers - each subscriber only receives messages from their topic

### Scenario 3: Mixed Topic Subscriptions

1. Start server: `server.exe 5000`
2. Start publishers for different topics
3. Start subscribers for various topics (some same, some different)
4. Observe that messages are properly filtered by topic

## Protocol Details

### Client-Server Communication

1. **Connection**: Client connects to server
2. **Registration**: Client sends `TYPE:TOPIC` (e.g., `PUBLISHER:SPORTS`)
3. **Messaging**:
   - Publishers send messages that get routed to topic subscribers
   - Subscribers receive formatted messages: `[TOPIC] Publisher X: message`

### Message Format

- **From Publisher**: Raw message text
- **To Subscribers**: `[TOPIC] Publisher X: message`
- **Server Logs**: `[TOPIC] Publisher X (IP): message`

## Technical Implementation

- **Topic Storage**: Each client struct contains topic string (max 64 chars)
- **Message Routing**: `broadcast_to_topic_subscribers()` filters by topic
- **Statistics**: Real-time counting of publishers/subscribers per topic
- **Thread Safety**: Critical sections protect shared client data
- **Protocol**: Enhanced to include topic in initial handshake

## Error Handling

- **Invalid topic length** (>63 characters)
- **Malformed registration** (missing colon separator)
- **Network errors** with proper cleanup
- **Maximum client limits** (50 concurrent connections)

This implementation provides a robust foundation for topic-based publish-subscribe messaging with excellent scalability and maintainability.
