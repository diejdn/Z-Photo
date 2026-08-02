#include <QCoreApplication>
#include <QString>
#include <QDir>
#include <QSettings>
#include <windows.h> 
#include "MediaHttpServer.h"

/*
	简易http服务器，支持ipv4和ipv6
	需要从配置文件中读取rootDir，用作于请求url进行拼接，形成文件的真实路径
	
	分为两个路由 /videos/ 和 /images/
	
	举例说明：
	rootDir 为 D:/Z-Project/
	请求url为 http://127.0.0.1:8080/images/data/image/001.webp
	
	会拼接文件真实路径 D:/Z-Project/data/image/001.webp
	
	/videos/ 的路由也是一样的
	
	实现简易路径保护，只允许请求 D:/Z-Project/ 下的文件
	也就是说，只要url和rootDir拼接的文件路径有效，就能请求D:/Z-Project/下的某文件
*/

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
	
    // 读取配置文件 server.ini
    QSettings settings("server.ini", QSettings::IniFormat);
    // 从 [Server] 组中读取 rootDir，若不存在则使用默认值 "./"
    QString rootDir = settings.value("Server/rootDir", "./").toString();
	// 统一转换为正斜杠（跨平台风格）分隔符使用 \\ 或 / ，最终都转换为 / 风格
	rootDir = QDir::fromNativeSeparators(rootDir);
	// 如果路径不为空且不以 / 结尾，则添加 /
	if (!rootDir.isEmpty() && !rootDir.endsWith('/')) {
		rootDir += '/';
	}

    // 如果配置文件中路径为空或无效，可增加二次校验（可选）
    if (rootDir.isEmpty()) {
        qWarning() << "Warning: rootDir is empty in config, using current directory './'";
        rootDir = "./";
    }
	
    // 打印实际使用的根目录，方便调试
    qDebug() << "Using rootDir:" << rootDir;

    MediaHttpServer server(rootDir);
    bool ok = server.listen(QHostAddress::Any, 8080);
    if (ok)
    {
        qDebug() << "Server listen on 0.0.0.0:8080";
    }
    else
    {
        qDebug() << "Listen failed!";
    }

    return a.exec();
}