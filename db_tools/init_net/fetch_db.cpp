#include "httplib.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <windows.h>

/*
	网络模式中，从远端下载数据库文件
	用法：
	fetch_db.exe 输入远端ip 端口 以及文件路径（基于服务器配置的rootDir）
	
	注意，如果使用ipv6 需要在两边添加 []。即格式为 [2001:0db8:85a3:0000:0000:8a2e:0370:7334]	
*/

/*
g++ -std=c++17 -o fetch_db.exe fetch_db.cpp -lws2_32 -lpthread

g++ -std=c++17 -o fetch_db.exe fetch_db.cpp -lws2_32 -lpthread -static
强制静态链接
*/

// 将 UTF-8 字符串转为 wstring（用于 Windows 宽字符文件操作）
std::wstring utf8_to_wstring(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], len);
    wstr.pop_back();
    return wstr;
}

// 从路径中提取文件名（支持 / 和 \）
std::string get_filename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::string server_ip, server_port, remote_path;
    std::cout << "==== 获取远端数据库文件 ====" << std::endl;
    std::cout << "远端服务器 IP（如 127.0.0.1 或 [240e:...]）: ";
    std::getline(std::cin, server_ip);
    std::cout << "端口（如 8080）: ";
    std::getline(std::cin, server_port);
    std::cout << "远端数据库路径（如 /images/media.db）: ";
    std::getline(std::cin, remote_path);

    if (server_ip.empty() || server_port.empty() || remote_path.empty()) {
        std::cerr << "输入不能为空！" << std::endl;
        return 1;
    }

    // 提取本地文件名
    std::string local_filename = get_filename(remote_path);
    if (local_filename.empty()) {
        std::cerr << "无法从远端路径中提取文件名。" << std::endl;
        return 1;
    }

    // 构造请求 URL（远端路径）
    httplib::Client cli(server_ip, std::stoi(server_port));
    cli.set_read_timeout(30);
    cli.set_connection_timeout(10);

    std::cout << "正在连接服务器..." << std::endl;
    auto res = cli.Get(remote_path);

    if (!res) {
        std::cerr << "连接失败或请求超时。" << std::endl;
        return 1;
    }

    if (res->status != 200) {
        std::cerr << "HTTP 错误，状态码: " << res->status << std::endl;
        return 1;
    }

    // 将响应体写入本地文件（同名）
    std::wstring wpath = utf8_to_wstring(local_filename);
    std::ofstream file(wpath.c_str(), std::ios::binary);
    if (!file) {
        std::cerr << "无法创建本地文件: " << local_filename << std::endl;
        return 1;
    }

    file.write(res->body.c_str(), res->body.size());
    file.close();

    std::cout << "下载完成！文件已保存为: " << local_filename << std::endl;
    std::cout << "文件大小: " << res->body.size() << " 字节" << std::endl;

    return 0;
}