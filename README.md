
# Phoenix

Phoenix is a modern web framework using C++ as its backend language. Designed for performance and flexibility, it provides a clean structure for building APIs, routing, and serving static files.

## Features

- **C++ Backend:** Built for high performance and control.
- **Modular Structure:** Clear separation of API logic, routing, server configuration, and static files.
- **Easy Configuration:** Simple config files to manage server settings.
- **Static File Serving:** Built-in support for serving public assets.

## Directory Structure

| Folder      | Purpose                                                          |
|-------------|------------------------------------------------------------------|
| `act`       | API logic. Place your endpoint logic here.                       |
| `pages`     | Route registration and route settings. Define available routes.  |
| `core`      | The engine of Web++. Main server and framework internals.        |
| `config`    | Server configuration files (e.g., `server.wpp`).                 |
| `public`    | Static files to be served (HTML, CSS, JS, images, etc.).         |
| `scripts`   | Build scripts for compiling and managing the Web++ project.      |
| `libs`      | Third-party libraries and dependencies.                          |

## Getting Started

### Prerequisites

- C++ compiler (C++11 or newer)
- CMake or relevant build tools

### Build & Run

```bash
# Clone the repository
git clone https://github.com/Fyonietz/Phoenix.git
cd WPP

# Build using provided scripts
./scripts/build.cmd         # or use your build system

#Or Using Phoenix-Cli
git clone https://github.com/Fyonietz/Phoenix-Cli.git
pnix talon
