#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include "engine.hpp"

// Atomic flag for thread control
std::atomic<bool> running{true};

// Function to handle user commands
void command_loop() {
    std::string command;
    
    std::cout << "\n>>> Command console activated. Type 'help' for commands.\n";
    
    while (running) {
        std::cout << " > Phoenix[CLI]: ";
        std::getline(std::cin, command);
        
        if (command == "help") {
            std::cout << "Available commands:\n";
            std::cout << "  help     - Show this help\n";
            std::cout << "  status   - Show server status\n";
            std::cout << "  reload   - Reload routes\n";
            std::cout << "  stop     - Stop the server\n";
            std::cout << "  exit     - Exit program\n";
            std::cout << "  clear    - Clear screen\n";
        }
        else if (command == "status") {
            std::cout << "Server status: Running\n";
            std::cout << "Mode: " << Config::routes << "\n";
            std::cout << "Port: " << Config::port << "\n";
            // Add more status information as needed
        }
        else if (command == "reload") {
            std::cout << "Reloading routes...\n";
            if (Config::routes == "debug") {
                debug();
                std::cout << "Routes reloaded successfully!\n";
            } else {
                std::cout << "Reload only available in debug mode\n";
            }
        }
        else if (command == "stop") {
            std::cout << "Stopping server...\n";
            running = false;
            break;
        }
        else if (command == "exit") {
            std::cout << "Exiting...\n";
            running = false;
            break;
        }
        else if (command == "clear") {
            // Clear screen (works on most terminals)
            std::cout << "\033[2J\033[1;1H";
        }
        else if (!command.empty()) {
            std::cout << "Unknown command: " << command << "\n";
            std::cout << "Type 'help' for available commands\n";
        }
    }
}

int main() {
    initConfig();
    
    // Start command thread
    std::thread command_thread(command_loop);
    
    if (Config::routes == "debug") {
        debug_start(Config::root.c_str());
        std::cout << Global::info << "Method:" << Config::routes << std::endl;
        
        // Wait briefly to ensure server is fully initialized
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (debug()) {
            std::thread t(debug_watcher);
            t.detach();
        }
    } else {
        release_start(Config::root.c_str());
        std::cout << Global::info << "Method:" << Config::routes << std::endl;
        
        // Wait briefly to ensure server is fully initialized
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        release();
    }
    
    // Main loop - wait for exit signal
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Optional: Add periodic tasks here
        // std::cout << "Server heartbeat...\n";
    }
    
    // Cleanup
    std::cout << "Shutting down...\n";
    cleanup_temp_so_files();
    
    // Wait for command thread to finish
    if (command_thread.joinable()) {
        command_thread.join();
    }
    
    return 0;
}
