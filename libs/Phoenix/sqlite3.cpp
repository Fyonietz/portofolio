#include "../json.hpp"
#include <sqlite3.h>
#include <iostream>
#include <string>

extern "C" struct Sqlite3 {
    std::string select_stmt;
    std::string from_stmt;
    std::string where_stmt;

    // SELECT clause
    Sqlite3 SELECT(const std::string &select_stmt_f) const {
        Sqlite3 copy = *this;
        copy.select_stmt = select_stmt_f;
        return copy;
    }

    // FROM clause
    Sqlite3 FROM(const std::string &from_stmt_f) const {
        Sqlite3 copy = *this;
        copy.from_stmt = from_stmt_f;
        return copy;
    }

    // Optional WHERE clause
    Sqlite3 WHERE(const std::string &where_stmt_f) const {
        Sqlite3 copy = *this;
        copy.where_stmt = where_stmt_f;
        return copy;
    }

    // Build final query string
    std::string build_query() const {
        std::string query = "SELECT " + select_stmt + " FROM " + from_stmt;
        if (!where_stmt.empty()) {
            query += " WHERE " + where_stmt;
        }
        return query + ";";
    }

    // Execute the query and return result as JSON (simple implementation)
    nlohmann::json JSON(sqlite3* db) const {
        std::string query = build_query();
        sqlite3_stmt* stmt;
        nlohmann::json result = nlohmann::json::array();

        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
        }

        int col_count = sqlite3_column_count(stmt);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            nlohmann::json row;
            for (int i = 0; i < col_count; ++i) {
                std::string col_name = sqlite3_column_name(stmt, i);
                const unsigned char* val = sqlite3_column_text(stmt, i);
                row[col_name] = val ? reinterpret_cast<const char*>(val) : nullptr;
            }
            result.push_back(row);
        }

        sqlite3_finalize(stmt);
        return result;
    }
    std::vector<std::string> RAW(sqlite3* db) const {
        std::string query = build_query();
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
        }

        int col_count = sqlite3_column_count(stmt);
        std::vector<std::string> result;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string row;
            for (int i = 0; i < col_count; ++i) {
                const unsigned char* val = sqlite3_column_text(stmt, i);
                row += (val ? reinterpret_cast<const char*>(val) : "NULL");

                if (i < col_count - 1)
                    row += " | ";
            }
            result.push_back(row);
        }

        sqlite3_finalize(stmt);
        return result;
    }
};

// Global fluent instance
inline const Sqlite3 sqlite;

