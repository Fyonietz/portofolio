#include "engine.hpp"
#include <dlfcn.h> // For dynamic library handling on Linux
#include <filesystem>
#include <iostream>
#include <chrono>
#include <mutex>

// Global Variables
std::mutex dllMutex;

LibraryHandle dllHandle = nullptr;
RouteFunc routefunc = nullptr;
std::filesystem::file_time_type lastWriteTime;
EXPORT int num = 2;

// Release the routes from shared library
void release() {
    std::cout << Global::info << "Routes Type: Static" << std::endl;

    dllHandle = dlopen(Global::dll_name.c_str(), RTLD_LAZY);
    if (!dllHandle) {
        std::cerr << Global::info << "Failed to load shared library: " << dlerror() << std::endl;
        return;
    }

    typedef void (*UpdateFunc)(struct mg_context *);
    UpdateFunc update = (UpdateFunc)dlsym(dllHandle, "update");
    if (!update) {
        std::cerr << Global::info << "Failed to find function: " << dlerror() << std::endl;
        dlclose(dllHandle);
        dllHandle = nullptr;
        return;
    }

    update(Config::context);
}

// Clean old temporary shared libraries
void clean_old_temp_dlls() {
    try {
        for (const auto &entry : std::filesystem::directory_iterator("core")) {
            if (entry.path().string().find("libroutes-temp-") != std::string::npos) {
                // Don't delete our current temp shared library
                if (dllHandle) {
                    Dl_info info;
                    if (dladdr(dllHandle, &info) && entry.path().string() == info.dli_fname) {
                        continue;
                    }
                }

                try {
                    std::filesystem::remove(entry.path());
                } catch (...) {
                    // Ignore deletion errors
                }
            }
        }
    } catch (...) {
        // Ignore directory iteration errors
    }
}

// Delete all *.temp.so files in the core directory
void cleanup_temp_so_files() {
    std::string dir = "core";
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            auto path = entry.path();
            if (path.extension() == ".so" && path.string().find(".temp") != std::string::npos) {
                std::cout << Global::info << "Deleting temp file: " << path << std::endl;
                std::filesystem::remove(path);
            }
        }
    }
}

// Debug function to reload dynamic routes
bool debug() {
    std::lock_guard<std::mutex> lock(dllMutex); // This mutex protects dllHandle and routefunc, but NOT Global::dll_name

    // 🔽🔽🔽 CRITICAL FIX: Make a local copy of the global path immediately. 🔽🔽🔽
    std::string local_dll_name = Global::dll_name;
    // 🔼🔼🔼 Now all operations use this local, thread-safe copy. 🔼🔼🔼

    std::cout << Global::info << "Routes Type: Dynamic" << std::endl;

    if (dllHandle) {
        dlclose(dllHandle);
        dllHandle = nullptr;
        routefunc = nullptr;
    }

    std::string temp_dll = "core/libroutes-temp-" +
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".so";
    
    // Debug the LOCAL copy, not the global
    std::cout << Global::info << "local_dll_name address: " << &local_dll_name << "\n";
    std::cout << Global::info <<"local_dll_name value: " << local_dll_name << "\n";

    // 🔒 Check the existence of the LOCAL copy
    std::cout << Global::info <<"Checking file existence at: " << local_dll_name << "\n";
    if (!std::filesystem::exists(local_dll_name)) {
        std::cerr << Global::info << "Shared library not found: " << local_dll_name << std::endl;
        return false;
    } else {
        std::cout << Global::info << "Shared library exists at: " << local_dll_name << std::endl;
    }

    try {
        std::cout << Global::info <<"Copying library from: " << local_dll_name << " to: " << temp_dll << std::endl;
        std::filesystem::copy_file(local_dll_name, temp_dll,
                                   std::filesystem::copy_options::overwrite_existing);
        
        dllHandle = dlopen(temp_dll.c_str(), RTLD_LAZY);
        if (!dllHandle) {
            std::cerr << Global::info << "Failed to load shared library: " << dlerror() << std::endl;
            // std::cout << "Error message from dlerror: " << dlerror() << std::endl; // Don't call dlerror twice!
            return false;
        }

        typedef void (*UpdateFunc)(struct mg_context *);
        UpdateFunc update = (UpdateFunc)dlsym(dllHandle, "update");
        if (!update) {
            std::cerr << Global::info << "Failed to get function: " << dlerror() << std::endl;
            dlclose(dllHandle);
            dllHandle = nullptr;
            return false;
        }

        update(Config::context);
        // 🔒 Update lastWriteTime using the LOCAL copy to be consistent
        lastWriteTime = std::filesystem::last_write_time(local_dll_name);
        clean_old_temp_dlls();

        return true;
    } catch (const std::exception &e) {
        std::cerr << Global::info << "Error in dynamic_routes: " << e.what() << "\n";
        return false;
    }
}

// Watch for changes and reload routes
void debug_watcher() {
    while (true) {
        try {
            // Check for file changes
            auto currentTime = std::filesystem::last_write_time(Global::dll_name);
            if (currentTime != lastWriteTime) {
                if (debug()) {
                    std::cout << Global::info << "Successfully reloaded routes!\n";
                } else {
                    std::cerr << Global::info << "Failed to reload routes\n";
                }
            }

            // Call update function if available
            if (routefunc) {
                std::lock_guard<std::mutex> lock(dllMutex);
                routefunc();
            }

            std::this_thread::sleep_for(std::chrono::seconds(3));
            system("make -C build routes > /dev/null 2>&1");
        } catch (...) {
            std::cerr << Global::info << "Error in reload thread\n";
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

// Debug start method
void debug_start(const char *root, const char *port, const char *threads, const char *alive) {
    updateConfig();

    if (!Config::context) {
        std::cerr << Global::info << "Failed to start server in debug mode\n";
        return;
    }

    std::cout << Global::info << "Server running in debug mode\n";
}

// Release start method
void release_start(const char *root, const char *port, const char *threads, const char *alive) {
    updateConfig();
}
