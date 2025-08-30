#include "register.hpp"
#include <iostream>



// Define RouteEntry structure
struct RouteEntry {
    const char* path;
    int (*handler)(mg_connection*, void*);
    void* data = nullptr;
};

// Define global route vector
static std::vector<RouteEntry>& get_routes_internal() {
    static std::vector<RouteEntry> routes_instance;
    return routes_instance;
}

// Route add implementation
void route_add(mg_context *context, const char *url, 
               int (*handler)(mg_connection*, void*), void *data) {
    if (context && handler) {
        mg_set_request_handler(context, url, handler, data);
    }
}

// Define the global Route instance (with EXPORT here)
extern "C" {
    EXPORT RouteStruct GlobalRoute = {route_add};  // EXPORT here for definition
}

// Function to add routes
EXPORT void add_route(const char* path, int (*handler)(mg_connection*, void*), void* data) {
    get_routes_internal().push_back({path, handler, data});
}

// Function to get routes count
EXPORT size_t get_routes_count() {
    return get_routes_internal().size();
}

// Example update function to register routes
extern "C" EXPORT void update(mg_context *context) {
    if (!context) {
        std::cerr << "update: missing context!\n";
        return;
    }

    auto& routes = get_routes_internal();
    std::cout << " > Phoenix[Info]: Update: Registering " << routes.size() << " routes\n";

    

    // Register all routes from the vector
    for (const auto& route : routes) {
        if (route.handler) {
            std::cout << " > Phoenix[Info]: Registering route: " << route.path << std::endl;
            GlobalRoute.add(context, route.path, route.handler, route.data);
        }
    }
}

// Constructor to verify library loads
__attribute__((constructor))
static void init() {
    std::cout << " > Phoenix[Debug]: Routes library loaded successfully" << std::endl;
    std::cout << " > Phoenix[Debug]: Pre-registered routes: " << get_routes_internal().size() << std::endl;
}