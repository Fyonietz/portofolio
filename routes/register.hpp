#pragma once
#include "civetweb.h"
#include <vector>
#include <iostream>

// Export macro for shared library symbols
#ifndef EXPORT
#define EXPORT __attribute__((visibility("default")))
#endif

// Forward declarations
struct mg_connection;
struct mg_context;

// Simple C-style Route structure
typedef struct {
    void (*add)(mg_context*, const char*, int (*)(mg_connection*, void*), void*);
} RouteStruct;

// Function declarations
EXPORT void add_route(const char* path, int (*handler)(mg_connection*, void*), void* data = nullptr);
EXPORT size_t get_routes_count();

// Update function for registering routes
extern "C" {
    EXPORT void update(mg_context *ctx);
}

// Declare the global Route instance (without EXPORT to make it just a declaration)
extern "C" {
    extern RouteStruct GlobalRoute;  // Remove EXPORT here
}