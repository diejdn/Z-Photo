#include <iostream>
#include <string>
#include <windows.h> 
extern "C"{
	#include "sqlite3.h"
}

/*
	初始化数据库，建立三张表，并保存为db文件
	
	用法：
	initdb.exe media.db  在当前目录创建初始化表并保存为文件media.db
	
	initdb.exe 无参数，当前目录默认生成metadata.db
*/

/*
g++ -std=c++17 -o initdb.exe initdb.cpp -L. -lsqlite3 -lws2_32 -lpthread

g++ -std=c++17 -o initdb.exe initdb.cpp -L. -lsqlite3 -lws2_32 -lpthread -static
*/

// 关于 gbk 宽字符 utf8 之间的转换
std::wstring gbk_to_wstring(const std::string& gbk) {
    if (gbk.empty()) return L"";
    int len = MultiByteToWideChar(CP_ACP, 0, gbk.c_str(), -1, nullptr, 0);
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_ACP, 0, gbk.c_str(), -1, &wstr[0], len);
    wstr.pop_back();
    return wstr;
}

std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8[0], len, nullptr, nullptr);
    utf8.pop_back();
    return utf8;
}

std::wstring utf8_to_wstring(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], len);
    wstr.pop_back();
    return wstr;
}

int execute_sql(sqlite3* db, const char* sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return rc;
    }
    return SQLITE_OK;
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    
    std::wstring wpath = L"metadata.db";
    if (argc >= 2) {
        std::string arg_gbk = argv[1];
        wpath = gbk_to_wstring(arg_gbk);
    }

    sqlite3* db = nullptr;
    int rc = sqlite3_open16(wpath.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return 1;
    }

    std::cout << "Database opened/created: " << wstring_to_utf8(wpath) << std::endl;

    // 开启外键约束
    if (execute_sql(db, "PRAGMA foreign_keys = ON;") != SQLITE_OK) {
        sqlite3_close(db);
        return 1;
    }

    // WAL 模式（提高并发性能）
    if (execute_sql(db, "PRAGMA journal_mode = WAL;") != SQLITE_OK) {
        sqlite3_close(db);
        return 1;
    }

    // 增大缓存（10 万+ 数据必备）
    if (execute_sql(db, "PRAGMA cache_size = -20000;") != SQLITE_OK) {
        sqlite3_close(db);
        return 1;
    }

    // 创建表结构（新 media 表）
    const char* create_table_sql =
        "CREATE TABLE IF NOT EXISTS media ("
        "    media_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    file_path TEXT NOT NULL UNIQUE,"
        "    name TEXT NOT NULL,"
        "    source_folder TEXT NOT NULL,"
        "    mtime INTEGER NOT NULL,"
        "    thumbnail_path TEXT,"
        "    media_type INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS tag ("
        "    tag_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    tag_name TEXT NOT NULL UNIQUE"
        ");"
        "CREATE TABLE IF NOT EXISTS media_tag_rel ("
        "    media_id INTEGER NOT NULL,"
        "    tag_id INTEGER NOT NULL,"
        "    PRIMARY KEY (media_id, tag_id),"
        "    FOREIGN KEY (media_id) REFERENCES media(media_id) ON DELETE CASCADE,"
        "    FOREIGN KEY (tag_id) REFERENCES tag(tag_id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_media_source_folder ON media(source_folder);"
        "CREATE INDEX IF NOT EXISTS idx_media_mtime ON media(mtime);"
        "CREATE INDEX IF NOT EXISTS idx_media_type ON media(media_type);"
        "CREATE INDEX IF NOT EXISTS idx_rel_media_id ON media_tag_rel(media_id);"
        "CREATE INDEX IF NOT EXISTS idx_rel_tag_id ON media_tag_rel(tag_id);";

    if (execute_sql(db, create_table_sql) != SQLITE_OK) {
        sqlite3_close(db);
        return 1;
    }

    std::cout << "Database schema created successfully." << std::endl;

    sqlite3_close(db);
    return 0;
}