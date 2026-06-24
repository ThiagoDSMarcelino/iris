# LUFT — Lightweight UDP File Transfer

## About the Project

LUFT is a client-server application for **file transfer over UDP**, built from scratch on top of the operating system's socket API. The goal is to manually implement the reliability mechanisms that TCP provides natively — segmentation, ordering, error detection, and retransmission — over a protocol that, by nature, offers none of these guarantees.

> Reliable file transfer over UDP using raw sockets, without any network abstraction libraries.

## Dependencies

- CMake >= 3.25
- Ninja
- GCC with C++23 support
- clang-tidy >= 19 (required — build fails without it)
- zlib

```bash
sudo apt install cmake ninja-build g++ clang-tidy-19 zlib1g-dev
```

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
ninja -C build/debug luft_server
ninja -C build/debug luft_client
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
./build/debug/luft_server <port> <search_directory>
```

**Example:** start the server on port 9000, serving files from `/tmp/files`:

```bash
./build/debug/luft_server 9000 /tmp/files
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

The client connects to a server and downloads a file.

```bash
./build/debug/luft_client <ip> <port> <filename> [output_dir]
```

- `output_dir` is optional. If omitted, the file is saved in the current directory.
- If a file with the same name already exists, the client automatically renames the new file to `filename (N).ext`, incrementing `N` until a free name is found.

**Example:** download `hello.txt` from a server running on `127.0.0.1:9000`:

```bash
./build/debug/luft_client 127.0.0.1 9000 hello.txt
```

**Example:** download to a specific directory:

```bash
./build/debug/luft_client 127.0.0.1 9000 hello.txt /tmp/downloads
```

Errors:

| Situation                       | Output                                              |
| ------------------------------- | --------------------------------------------------- |
| Wrong number of arguments       | `Usage: client <ip> <port> <filename> [output_dir]` |
| Output directory does not exist | `Failed to create client: invalid output directory` |
| Connection or receive failure   | `Failed to fetch file: receive failed`              |
| Corrupted packet                | `Failed to fetch file: deserialize failed`          |

---

### Running both together

Open two terminals:

```bash
# Terminal 1 — start the server
./build/debug/luft_server 9000 /tmp/files

# Terminal 2 — download a file
./build/debug/luft_client 127.0.0.1 9000 hello.txt
```
