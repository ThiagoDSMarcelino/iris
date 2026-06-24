# IRIS — File Transfer & Chat over TCP

## About the Project

**IRIS** is a client-server application for **file transfer and group chat over TCP**, built from scratch on top of the operating system's socket API. The same server and port serve both: a 1-byte opcode tells a file download apart from a chat session (see [Protocol](#protocol)).

The name is a nod to **Iris**, the Greek goddess who personified the rainbow and served as the **messenger of the gods**, linking the **heavens and the earth** — here, the **server and the client**. Just as Iris carried messages between two worlds, this project carries files between two hosts.

> Where its predecessor [LUFT](https://en.wikipedia.org/wiki/User_Datagram_Protocol) had to _manually_ implement reliability over UDP — sequence numbers, acknowledgements, checksums and retransmission — IRIS leans on **TCP**, which provides ordered, reliable, error-checked delivery natively. The result is a much smaller, stream-oriented program.

### What TCP gives us for free

The UDP version needed a large machinery to be reliable. With TCP, the kernel handles it, so IRIS drops:

| UDP version (LUFT)                       | TCP version (IRIS)                           |
| ---------------------------------------- | -------------------------------------------- |
| Per-packet sequence numbers + ACK/NACK   | TCP guarantees ordering and delivery         |
| Application-level CRC32 checksums        | TCP checksums every segment                  |
| Timeout + retransmission, RTT estimation | TCP retransmits transparently                |
| Per-client session demultiplexing        | Each connection is its own `accept()` socket |
| Fault injection (to _test_ reliability)  | N/A — the transport is already reliable      |

What remains is a tiny **framing protocol** on top of the byte stream (TCP is a stream, not messages), one thread per connection, and straightforward streaming of file bytes.

## Protocol

Every client -> server frame starts with a 1-byte **opcode** so a single connection
(and port) can serve both file downloads and chat:

```txt
opcode (1 byte): 0 = FileRequest, 1 = ChatJoin, 2 = ChatMessage

Client -> server:
  FileRequest : [ 0 | nameLength (2) | name ]
  ChatJoin    : [ 1 | nickLength (2) | nick | roomLength (2) | room ]
  ChatMessage : [ 2 | textLength (2) | text ]

Server -> client:
  File response : [ status (1) | length (8) | body (length) ]
                    - status OK    -> body is the file content (length = file size)
                    - status != OK -> body is a UTF-8 error message
  Chat line     : [ length (2) | text ]   # already formatted, e.g. "alice: oi"
```

A file download is a one-shot request/response (the connection closes afterwards).
A chat connection is **persistent**: after `ChatJoin` the client streams `ChatMessage`
frames and the server pushes `Chat line` frames (peer messages and join/leave notices)
to every member of the same room until the client disconnects.

All multi-byte integers are sent in big-endian (network) order.

## Dependencies

- CMake >= 3.25
- Ninja
- GCC with C++23 support
- clang-tidy >= 19 (required — build fails without it)

```bash
sudo apt install cmake ninja-build g++ clang-tidy-19
```

> The project targets POSIX/Linux sockets (`<sys/socket.h>`, `<netinet/in.h>`). On Windows, build inside WSL.

## Build

Configure and compile using one of the available presets:

| Preset           | Description                     |
| ---------------- | ------------------------------- |
| `debug`          | All targets, with debug symbols |
| `release`        | All targets, with optimizations |
| `server-debug`   | Server only (debug)             |
| `client-debug`   | Client only (debug)             |
| `server-release` | Server only (release)           |
| `client-release` | Client only (release)           |

```bash
# Configure
cmake --preset debug

# Build
ninja -C build/debug
```

To build only a specific target (e.g. server or client):

```bash
ninja -C build/debug iris_server
ninja -C build/debug iris_client
```

For a release build:

```bash
cmake --preset release
ninja -C build/release
```

Binaries are generated in `build/debug/` or `build/release/`.

## Usage

### Server

The server requires a port and a directory to search files in.

```bash
./build/debug/iris_server <port> <search_directory>
```

**Example:** start the server on port 9000, serving files from `/tmp/files`:

```bash
./build/debug/iris_server 9000 /tmp/files
```

```bash
Server listening on port 9000
```

Errors on startup:

| Situation                 | Output                                       |
| ------------------------- | -------------------------------------------- |
| Wrong number of arguments | `Usage: server <port> <search_directory>`    |
| Directory does not exist  | `Failed to create server: invalid directory` |
| Port already in use       | `Server error: bind failed`                  |

---

### Client

Run the client with just the server address to get an **interactive menu**:

```bash
./build/debug/iris_client <ip> <port>
```

```txt
=== IRIS ===
1. Baixar arquivo
2. Entrar no chat
>
```

- **Option 1 (download):** prompts for a filename and an optional output directory.
  If omitted, the file is saved in the current directory. If a file with the same name
  already exists, the client renames the new one to `filename (N).ext`, incrementing `N`
  until a free name is found.
- **Option 2 (chat):** prompts for a nickname and a room name. The room is **created if it
  does not exist** and joined otherwise. Everything you type is broadcast to the other
  members of the same room; type `/sair` to leave.

**Direct download** (no menu, handy for scripts):

```bash
./build/debug/iris_client 127.0.0.1 9000 hello.txt              # save in current dir
./build/debug/iris_client 127.0.0.1 9000 hello.txt /tmp/downloads
```

Errors:

| Situation                       | Output                                              |
| ------------------------------- | --------------------------------------------------- |
| Wrong number of arguments       | `Usage: client <ip> <port> ...`                     |
| Output directory does not exist | `Failed to create client: invalid output directory` |
| Server is unreachable           | `Failed to fetch file: connect failed`              |
| File does not exist on server   | `Server error: File not found`                      |

#### Chat example

```bash
# Terminal 1 — server
./build/debug/iris_server 9000 /tmp/files

# Terminals 2 and 3 — two clients
./build/debug/iris_client 127.0.0.1 9000
# choose 2, pick a nick, join room "xpto", and start talking
```

---

### Running both together

Open two terminals:

```bash
# Terminal 1 — start the server
./build/debug/iris_server 9000 /tmp/files

# Terminal 2 — download a file
./build/debug/iris_client 127.0.0.1 9000 hello.txt
```
