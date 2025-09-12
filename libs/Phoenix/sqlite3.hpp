#include "../json.hpp"
#include "engine.hpp"
#include <iostream>
#include <sqlite3.h>
#include <string>

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

// Global SQLite DB handle
extern EXPORT sqlite3 *g_db;

EXPORT bool Sqlite_Open(const std::string &filename = "data.db");
EXPORT bool Sqlite_Close();
EXPORT std::string Escape(const std::string& input);
extern "C" struct EXPORT Sqlite3 {
  enum QueryType { SELECT_Q, INSERT_Q, DELETE_Q, NONE_Q } query_type = NONE_Q;

  // For SELECT
  std::string select_stmt;
  std::string from_stmt;

  // For WHERE clause (used in SELECT and DELETE)
  std::string where_stmt;

  // For INSERT
  std::string insert_table;
  std::string insert_columns; // e.g. "(col1, col2)"
  std::string insert_values;  // e.g. "('val1', 'val2')"

  // SELECT clause
  Sqlite3 SELECT(const std::string &select_stmt_f) const {
    Sqlite3 copy = *this;
    copy.query_type = SELECT_Q;
    copy.select_stmt = select_stmt_f;
    return copy;
  }

  // FROM clause for SELECT
  Sqlite3 FROM(const std::string &from_stmt_f) const {
    Sqlite3 copy = *this;
    copy.from_stmt = from_stmt_f;
    return copy;
  }

  // WHERE clause for SELECT and DELETE
  Sqlite3 WHERE(const std::string &where_stmt_f) const {
    Sqlite3 copy = *this;
    copy.where_stmt = where_stmt_f;
    return copy;
  }

  // INSERT INTO table (cols) VALUES (vals)
  Sqlite3 INSERT(const std::string &table, const std::string &columns,
                 const std::string &values) const {
    Sqlite3 copy = *this;
    copy.query_type = INSERT_Q;
    copy.insert_table = table;
    copy.insert_columns = columns;
    copy.insert_values = values;
    return copy;
  }

  // DELETE FROM table
  Sqlite3 DELETE(const std::string &table) const {
    Sqlite3 copy = *this;
    copy.query_type = DELETE_Q;
    copy.from_stmt = table; // reuse from_stmt as table name
    return copy;
  }

  // Build the SQL query string based on type
  std::string build_query() const {
    switch (query_type) {
    case SELECT_Q: {
      std::string query = "SELECT " + select_stmt + " FROM " + from_stmt;
      if (!where_stmt.empty()) {
        query += " WHERE " + where_stmt;
      }
      return query + ";";
    }
    case INSERT_Q: {
      return "INSERT INTO " + insert_table + " " + insert_columns + " VALUES " +
             insert_values + ";";
    }
    case DELETE_Q: {
      std::string query = "DELETE FROM " + from_stmt;
      if (!where_stmt.empty()) {
        query += " WHERE " + where_stmt;
      }
      return query + ";";
    }
    default:
      throw std::runtime_error("No query type specified");
    }
  }

  // Execute SELECT query and return JSON
  nlohmann::json JSON() const {
    if (query_type != SELECT_Q) {
      throw std::runtime_error("JSON() only valid for SELECT queries");
    }
    if (!g_db)
      throw std::runtime_error("Database not opened");

    std::string query = build_query();
    sqlite3_stmt *stmt;
    nlohmann::json result = nlohmann::json::array();

    if (sqlite3_prepare_v2(g_db, query.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
      throw std::runtime_error("Failed to prepare statement: " +
                               std::string(sqlite3_errmsg(g_db)));
    }

    int col_count = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
      nlohmann::json row;
      for (int i = 0; i < col_count; ++i) {
        std::string col_name = sqlite3_column_name(stmt, i);
        const unsigned char *val = sqlite3_column_text(stmt, i);
        row[col_name] = val ? reinterpret_cast<const char *>(val) : nullptr;
      }
      result.push_back(row);
    }

    sqlite3_finalize(stmt);
    return result;
  }

  // Execute SELECT query and return raw strings
  std::vector<std::string> RAW() const {
    if (query_type != SELECT_Q) {
      throw std::runtime_error("RAW() only valid for SELECT queries");
    }
    if (!g_db)
      throw std::runtime_error("Database not opened");

    std::string query = build_query();
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(g_db, query.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
      throw std::runtime_error("Failed to prepare statement: " +
                               std::string(sqlite3_errmsg(g_db)));
    }

    int col_count = sqlite3_column_count(stmt);
    std::vector<std::string> result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
      std::string row;
      for (int i = 0; i < col_count; ++i) {
        const unsigned char *val = sqlite3_column_text(stmt, i);
        row += (val ? reinterpret_cast<const char *>(val) : "NULL");
        if (i < col_count - 1)
          row += " | ";
      }
      result.push_back(row);
    }

    sqlite3_finalize(stmt);
    return result;
  }

  // Execute non-select queries (INSERT, DELETE)
  void execute() const {
    if (query_type != INSERT_Q && query_type != DELETE_Q) {
      throw std::runtime_error(
          "execute() only valid for INSERT or DELETE queries");
    }
    if (!g_db)
      throw std::runtime_error("Database not opened");

    std::string query = build_query();
    char *errmsg = nullptr;
    int rc = sqlite3_exec(g_db, query.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
      std::string err_str = errmsg ? errmsg : "Unknown error";
      sqlite3_free(errmsg);
      throw std::runtime_error("SQL error: " + err_str);
    }
  }
};

inline const Sqlite3 sqlite;
