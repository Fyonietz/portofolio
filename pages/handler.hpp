#pragma once

#include "../core/starter.hpp"
#include "pyro.hpp"
#include "../core/engine.hpp"
#include "../routes/register.hpp"
#include "../libs/Phoenix/sqlite3.hpp"
#include "../libs/Phoenix/controller.hpp"
#ifdef _WIN32
    #define EXPORT __declspec(dllexport)
#else   
    #define EXPORT __attribute__((visibility("default")))
#endif
extern Pnix Server;
extern Global test;
// Forward declarations
struct mg_connection;

EXPORT int default_handler(struct mg_connection *connection, void * /*callbackdata*/);
EXPORT int home(struct mg_connection *connection,void *callback);


// Even simpler - no EXPORT in macro
#define route(PATH, NAME) \
    int NAME(struct mg_connection* connection, void* cb); \
    namespace { struct NAME##_Reg { \
        NAME##_Reg() { add_route(PATH, NAME); } \
    } NAME##_instance; } \
    int NAME(struct mg_connection* connection, void* cb)
