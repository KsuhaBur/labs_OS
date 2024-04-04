#pragma once
#include "sqlite3.h"
inline sqlite3* db;
using namespace std;
inline auto createTotalDB =
"CREATE TABLE IF NOT EXISTS total_temp (\
        id INTEGER PRIMARY KEY AUTOINCREMENT,\
        timestamp DATETIME,\
        temperature REAL);";

inline auto createDayDB =
"CREATE TABLE IF NOT EXISTS day_temp (\
        id INTEGER PRIMARY KEY AUTOINCREMENT,\
        timestamp DATETIME,\
        temperature REAL);";

inline auto createHourDB =
"CREATE TABLE IF NOT EXISTS hour_temp (\
        id INTEGER PRIMARY KEY AUTOINCREMENT,\
        timestamp DATETIME,\
        temperature REAL);";

auto DB_NAME = "temperature_data_database";


int executeQuery(sqlite3* db, const char* query) {
    char* errorMessage = nullptr;
    int rc = sqlite3_exec(db, query, nullptr, nullptr, &errorMessage);
    if (rc != SQLITE_OK) {
        cerr << "Error executing query: " << errorMessage << endl;
        sqlite3_free(errorMessage);
    }
    return rc;
}

auto CreateDB()
{
    int rc = sqlite3_open("temperature_logs.db", &db);
    if (rc != SQLITE_OK) {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return rc;
    }

    const char* createTableAllQuery =
        "CREATE TABLE IF NOT EXISTS total_temp ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp DATETIME,"
        "temperature REAL);";

    const char* createTableHourQuery =
        "CREATE TABLE IF NOT EXISTS hour_temp ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp DATETIME,"
        "temperature REAL);";

    const char* createTableDayQuery =
        "CREATE TABLE IF NOT EXISTS day_temp ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp DATETIME,"
        "temperature REAL);";

    rc = executeQuery(db, createTableDayQuery);
    if (rc != SQLITE_OK) {
        cerr << "Can't create table 'day_temp'" << endl;
        sqlite3_close(db);
        return rc;
    }

    rc = executeQuery(db, createTableHourQuery);
    if (rc != SQLITE_OK) {
        cerr << "Can't create table 'hour_temp'" << endl;
        sqlite3_close(db);
        return rc;
    }

    rc = executeQuery(db, createTableAllQuery);
    if (rc != SQLITE_OK) {
        cerr << "Can't create table 'total_temp'" << endl;
        sqlite3_close(db);
        return rc;
    }

}