#include "sqlite3.h"
#include "engine.hpp"
#include <iostream>

sqlite3 *g_db = nullptr;
// Open the database globally once
EXPORT bool Sqlite_Open(const std::string &filename = "data.db") {
  if (sqlite3_open(filename.c_str(), &g_db) != SQLITE_OK) {
    std::cerr << "Cannot open database: " << sqlite3_errmsg(g_db) << std::endl;
    return false;
  }
  return true;
}

// Close the database globally once
EXPORT void Sqlite_Close() {
  if (g_db) {
    sqlite3_close(g_db);
    g_db = nullptr;
  }
}

EXPORT std::string Escape(const std::string& input){
  std::string escaped = "'";
    for (char c : input) {
        if (c == '\'') {
            escaped += "''";  // SQLite escape for single quote
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}
