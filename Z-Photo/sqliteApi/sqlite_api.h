#ifndef SQLITE_WRAPPER_H
#define SQLITE_WRAPPER_H
#include <iostream>
#include <string>
#include <vector>
#include "schema.h"
extern "C"{
	#include "sqlite3.h"
}
// 头文件引入会污染所有包含该头文件的 .cpp，只在 .cpp 保留 using namespace std;。

/*
	定义对sqlite的各种操作，供系统查询使用
	
	其中 libsqlite3.a 是预编译的sqlite库
	
	SqliteApi是使用利用sqlite库编写的针对该系统的数据库操作
*/

class SqliteApi{
public:
	// 构造函数
	SqliteApi(const std::string& path);
	// 析构函数
	~SqliteApi();
	
	// 禁止复制实例
	SqliteApi(const SqliteApi&) = delete;
	SqliteApi& operator=(const SqliteApi&) = delete;
	
	// 打开数据库
	bool open();
	// 关闭数据库
	void close();
	
	/*
	安全性规范
		关于const函数
		const 类名& 对象 代表常量对象，编译器认定：这个对象内部所有成员变量都禁止被修改。
		常量对象不能调用非 const 成员函数；
		常量对象只能调用标记了 const 的成员函数。
		为了保持常量对象合法
		
		例如，传入的参数设置为常量，即该对该对象的引用在test函数内禁止修改
		void test(const SqliteApi& db)
		{
			db.isOpen();  // 报错：isOpen() 无 const，编译器认为存在修改风险
			db.getError();// 同样报错
		}
	*/
	// 判断数据库是否打开
	bool isOpen() const;
	
	// 判断是否开启外键删除策略
	bool isForeign() const;
	
	// 获得报错信息
	std::string getError() const;
	
	// 获取所有的source_folder select
	bool query_media_source(std::vector<std::string>& dirVector);
	
	// 查询某source_folder内的图片数量 select
	int count_by_source(const std::string& source_folder);
	
	// 根据tag_id查询该标签所包含的图片数量
	int count_media_by_tag(int tag_id);
	
	// 为tag表添加标签，单值insert 
	bool add_tag(const std::string& tag_name);
	
	// 删除某个标签
	bool delete_tag(int tag_id);
	
	// 获取所有标签 select
	bool get_all_tag(std::vector<tag_elem>& tagVector, bool isOrder=false, bool isDesc=false);
	
	// 根据media_id查询某图片绑定的标签
	bool get_tags_by_media(std::vector<tag_elem>& tagVector, int media_id);

	// 根据source_folder返回其中的所有图片 select
	bool query_all_media_by_source(std::vector<media_elem>& mediaVector, const std::string& source_folder);
	// 根据source_folder返回其中的所有图片 select 支持按照时间，名称排序 0 不排序，1 升序排序 2 降序排序
	bool query_all_media_by_source(std::vector<media_elem>& mediaVector, const std::string& source_folder, int media_type = -1, int orderMtime = 0, int orderName = 0, int limit = -1);
	
	// 为media添加标签，单值insert 
	bool add_tag_to_media(int media_id, int tag_id);
	// 为media添加标签，多值insert 
	bool add_tag_to_media(int media_id, std::vector<int>& tag_ids);
	
	// 根据tag_ids查询图片，支持source_folder，直接修改mediaVector select
	bool query_medias_by_tags(std::vector<media_elem>& mediaVector, std::vector<int>& tag_ids, const std::string& source_folder);
	// 重载，支持 是否全部目录 时间范围(时间戳) 返回数量(-1表示无限制)
	bool query_medias_by_tags(std::vector<media_elem>& mediaVector,
                          std::vector<int>& tag_ids,
                          const std::string& source_folder,
                          long long start_time, //修改为64位整型
                          long long end_time,
                          int media_type = -1,
						  int orderMtime = 0, 
						  int orderName = 0,
						  int limit = -1);
private:
	sqlite3* db = nullptr;
	std::string db_path;
	std::string lastErr;
	
	bool foreign_keys;
	bool wal;
	bool cache;
};




#endif // SQLITE_WRAPPER_H