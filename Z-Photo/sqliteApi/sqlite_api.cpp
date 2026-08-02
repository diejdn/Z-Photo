#include "sqlite_api.h"
#include <cstring>
#include <string>
#include <vector>

using namespace std;

SqliteApi::SqliteApi(const string& path): db_path(path), foreign_keys(false), wal(false), cache(false){
	
}

SqliteApi::~SqliteApi()
{
    close();
}

bool SqliteApi::open(){
	// sqlite3_open(const char *filename, sqlite3 **ppDb)
	int rc = sqlite3_open(db_path.c_str(), &db);
	if(rc != SQLITE_OK){
		// 保存报错信息
		lastErr = sqlite3_errmsg(db);
		// sqlite3_open 失败时，db 内部是无效指针，不能传给 sqlite3_close。
		
		db = nullptr;
		return false; 
	}
	
    // 1. 外键约束（保证数据完整性）
    rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
	if (rc != SQLITE_OK){
		foreign_keys = false;
	}
	else{
		foreign_keys = true;
	}
	
    // 2. WAL 模式（允许读与写并发执行）
    // 离线扫描工具在写入时，仍然可以查询浏览，不会被阻塞
    rc = sqlite3_exec(db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
	if (rc != SQLITE_OK){
		wal = false;
	}
	else{
		wal = true;
	}

    // 3. 增大缓存（10 万+ 数据必备）
    //    -20000 表示 20,000 KB（约 20MB）缓存，让索引常驻内存，查询速度毫秒级
    rc = sqlite3_exec(db, "PRAGMA cache_size = -20000;", nullptr, nullptr, nullptr);
	if (rc != SQLITE_OK){
		cache = false;
	}
	else{
		cache = true;
	}

	return true;
}

void SqliteApi::close()
{
    if (db)
    {
        sqlite3_close(db);
        db = nullptr;
		
		// 关闭数据库不需要清空错误信息，方便排查
    }
}

bool SqliteApi::isOpen() const{
	return db != nullptr;
}

bool SqliteApi::isForeign() const{
	return foreign_keys;
}

string SqliteApi::getError() const{
	return lastErr;
}

/*
统一执行流程
	所有增 / 删 / 改 / 查接口固定四步：
	第一步：isOpen() 校验连接，空连接直接返回并记录错误
	第二步：sqlite3_prepare_v2 预编译，失败无需释放 stmt
	第三步：绑定参数（如有），绑定失败释放 stmt 再返回
	第四步：sqlite3_step 执行，区分查询（SQLITE_ROW）/ 插入（SQLITE_DONE），异常统一释放句柄
	收尾：正常流程释放 stmt、清空 lastErr
*/

bool SqliteApi::query_media_source(vector<string>& dirVector){
	// 目标容器引用固定 清空容器
	dirVector.clear();
	
	// 固定1 判断数据库是否连接
    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return false;
    }
	
	// sql和stmt 统计所有source_folder
	const char* sql = "select distinct source_folder from media order by source_folder asc;";
	sqlite3_stmt* stmt = nullptr;
	
	// 固定2 预编译sql句柄，使用二级指针为stmt赋值
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		// 函数返回错误码（失败）：stmt 不会被初始化，内部仍然是之前赋值的 nullptr
		// 此时不需要sqlite3_finalize，只有prepare成功后才能释放
		lastErr = sqlite3_errmsg(db);
		return false;
	}
	
	//int row = 0;
	while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
		// 读取第0个结果 即source_folder
		string source_folder = (const char*)sqlite3_column_text(stmt, 0);
		
		dirVector.push_back(source_folder);
		
		//row++;
	}
	
	// 固定3 运行错误
	if (rc != SQLITE_DONE)
    {
        lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
    }
	
	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
	sqlite3_finalize(stmt);
	lastErr.clear();
	return true;
}

int SqliteApi::count_by_source(const string& source_folder){
	// 固定1 判断数据库是否连接
    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return 0;
    }
	
	// sql和stmt 根据source_folder查询图片总数
	const char* sql = "select count(*) from media where source_folder = ?;";
	sqlite3_stmt* stmt = nullptr;
	
	// 固定2 预编译sql句柄，使用二级指针为stmt赋值
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		lastErr = sqlite3_errmsg(db);
		return 0;
	}

	// 绑定参数，一个 text
	rc = sqlite3_bind_text(stmt, 1, source_folder.c_str(), -1, SQLITE_TRANSIENT);
	// 绑定参数固定 获取lastErr，释放stmt
	if (rc != SQLITE_OK)
	{
		lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return 0;
	}
	
	int count = 0;
	// 其实只有一行结果，使用if也行
	while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
		// 读取第0个结果 即count(*)
		count = sqlite3_column_int(stmt, 0);
	}
	
	// 固定3 运行错误
	if (rc != SQLITE_DONE)
    {
        lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return 0;
    }
	
	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
	sqlite3_finalize(stmt);
	lastErr.clear();
	
	return count;
}

int SqliteApi::count_media_by_tag(int tag_id){
	// 固定1 判断数据库是否连接
    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return 0;
    }
	
	// sql和stmt 根据tag_id查询该tag对应的图片数量
	const char* sql = "select count(*) as bindMedia from media_tag_rel r where tag_id = ?;";
	sqlite3_stmt* stmt = nullptr;
	
	// 固定2 预编译sql句柄，使用二级指针为stmt赋值
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		lastErr = sqlite3_errmsg(db);
		return 0;
	}
	
	// 绑定参数，一个 int
	rc = sqlite3_bind_int(stmt, 1, tag_id);
	// 绑定参数固定 获取lastErr，释放stmt
	if (rc != SQLITE_OK)
	{
		lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return 0;
	}
	
	int count = 0;
	// 其实只有一行结果，使用if也行
	while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
		// 读取第0个结果 即bindImage
		count = sqlite3_column_int(stmt, 0);
	}
	
	// 固定3 运行错误
	if (rc != SQLITE_DONE)
    {
        lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return 0;
    }
	
	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
	sqlite3_finalize(stmt);
	lastErr.clear();
	
	return count;
}

bool SqliteApi::add_tag(const string& tag_name){
	// 检测数据库是否连接
    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return false;
    }
	
	// sql和stmt 向tag表添加标签
	// INSERT OR IGNORE 标签已存在 → 不报错、不插入，直接执行完成，不会触发错误分支。
	const char* sql = "insert or ignore into tag(tag_name) values(?);";
	sqlite3_stmt* stmt = nullptr;
	
	// 固定2 预编译sql句柄，使用二级指针为stmt赋值
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		lastErr = sqlite3_errmsg(db);
		return false;
	}
	
	// 绑定参数 一个text
	rc = sqlite3_bind_text(stmt, 1, tag_name.c_str(), -1, SQLITE_TRANSIENT);
	// 绑定参数固定 获取lastErr，释放stmt
	if (rc != SQLITE_OK)
	{
		lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
	}
	
	// 插入固定 插入没有SQLITE_ROW，直接返回SQLITE_DONE
	rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE)
    {	
		lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
    }
	
	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
	sqlite3_finalize(stmt);
	lastErr.clear();
	return true;
}

bool SqliteApi::delete_tag(int tag_id){
	// 检测数据库是否连接
    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return false;
    }
	
	// sql和stmt 删除标签，PRAGMA foreign_keys = ON;下，可级联删除media_tag_rel中内容
	const char* sql = "delete from tag where tag_id = ?;";
	sqlite3_stmt* stmt = nullptr;
	
	// 固定2 预编译sql句柄，使用二级指针为stmt赋值
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		lastErr = sqlite3_errmsg(db);
		return false;
	}
	
	// 绑定参数，一个 int
	rc = sqlite3_bind_int(stmt, 1, tag_id);
	// 绑定参数固定 获取lastErr，释放stmt
	if (rc != SQLITE_OK)
	{
		lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
	}
	
	// 删除固定 插入没有SQLITE_ROW，直接返回SQLITE_DONE
	rc = sqlite3_step(stmt);
	// 删除固定，tag_id不存在时，不会删除，检查影响的行数
	if (sqlite3_changes(db) == 0) {
		lastErr = "标签不存在";
		sqlite3_finalize(stmt);
		return false;
	}
    if(rc != SQLITE_DONE)
    {	
		lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
    }
	
	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
	sqlite3_finalize(stmt);
	lastErr.clear();
	return true;
}

bool SqliteApi::get_all_tag(vector<tag_elem>& tagVector, bool isOrder, bool isDesc){
	// 目标容器引用固定 清空容器
	tagVector.clear();
	
	// 固定1 判断数据库是否连接
    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return false;
    }
	
	// 动态拼接SQL，仅两种固定格式，无注入风险，支持按tag_name升降排序
	// SQLite 占位符只能替换值，不能替换列名、排序关键字，所以只能固定分支拼接 SQL；
    string sqlBase = "select tag_id, tag_name from tag";
    string std_sql;
    if (isOrder)
    {
        if (isDesc)
        {
            std_sql = sqlBase + " order by tag_name desc;";
        }
        else
        {
            std_sql = sqlBase + " order by tag_name asc;";
        }
    }
    else
    {
        std_sql = sqlBase + ";";
    }
	
	// sql和stmt 统计所有tag
	const char* sql = std_sql.c_str();
	sqlite3_stmt* stmt = nullptr;
	
	// 固定2 预编译sql句柄，使用二级指针为stmt赋值
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		// 函数返回错误码（失败）：stmt 不会被初始化，内部仍然是之前赋值的 nullptr
		// 此时不需要sqlite3_finalize，只有prepare成功后才能释放
		lastErr = sqlite3_errmsg(db);
		return false;
	}
	
	//int row = 0;
	while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
		// 读取第0个结果 即tag_id
		int tag_id = sqlite3_column_int(stmt, 0);
		// 读取第1个结果 即tag_name
		string tag_name = (const char*)sqlite3_column_text(stmt, 1);
		
		tag_elem tag = {tag_id, tag_name};
		
		tagVector.push_back(tag);
		
		//row++;
	}
	
	// 固定3 运行错误
	if (rc != SQLITE_DONE)
    {
        lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
    }
	
	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
	sqlite3_finalize(stmt);
	lastErr.clear();
	return true;
}

// 根据media_id查询某图片绑定的标签
bool SqliteApi::get_tags_by_media(vector<tag_elem>& tagVector, int media_id){
	// 目标容器引用固定 清空容器
	tagVector.clear();
	
	// 固定1 判断数据库是否连接
    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return false;
    }
	
	// sql和stmt 查询某图片绑定的tag
	const char* sql = "select t.* from media_tag_rel r join tag t on t.tag_id = r.tag_id where r.media_id = ?;";
	sqlite3_stmt* stmt = nullptr;
	
	// 固定2 预编译sql句柄，使用二级指针为stmt赋值
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		lastErr = sqlite3_errmsg(db);
		return false;
	}
	
	// 绑定参数 一个int
	rc = sqlite3_bind_int(stmt, 1, media_id);
	// 绑定参数固定 获取lastErr，释放stmt
	if (rc != SQLITE_OK)
	{
		lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
	}
	
	//int row = 0;
	while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
		// 读取第0个结果 即tag_id
		int tag_id = sqlite3_column_int(stmt, 0);
		// 读取第1个结果 即tag_name
		string tag_name = (const char*)sqlite3_column_text(stmt, 1);
		
		tag_elem tag = {tag_id, tag_name};
		
		tagVector.push_back(tag);
		
		//row++;
	}
	
	// 固定3 运行错误
	if (rc != SQLITE_DONE)
    {
        lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
    }
	
	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
	sqlite3_finalize(stmt);
	lastErr.clear();
	return true;
}

bool SqliteApi::query_all_media_by_source(vector<media_elem>& mediaVector, const string& source_folder){
	// 目标容器引用固定 清空容器
	mediaVector.clear();
	
	// 固定1 判断数据库是否连接
    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return false;
    }
	
	// sql和stmt 查询source_folder所有media
	const char* sql = "select * from media where source_folder = ?;";
	sqlite3_stmt* stmt = nullptr;
	
	// 固定2 预编译sql句柄，使用二级指针为stmt赋值
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		// 函数返回错误码（失败）：stmt 不会被初始化，内部仍然是之前赋值的 nullptr
		// 此时不需要sqlite3_finalize，只有prepare成功后才能释放
		lastErr = sqlite3_errmsg(db);
		return false;
	}
	
	// 绑定参数，一个 text
	rc = sqlite3_bind_text(stmt, 1, source_folder.c_str(), -1, SQLITE_TRANSIENT);
	// 绑定参数固定 获取lastErr，释放stmt
	if (rc != SQLITE_OK)
	{
		lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
	}
	
	//int row = 0;
	while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
		// 读取第0个结果 media_id
		int media_id = sqlite3_column_int(stmt, 0);
		// 读取第1个结果 file_path
		string file_path = (const char*)sqlite3_column_text(stmt, 1);
		// 读取第2个结果 name
		string name = (const char*)sqlite3_column_text(stmt, 2);
		// 读取第3个结果 source_folder
		string source_folder = (const char*)sqlite3_column_text(stmt, 3);
		// 读取第4个结果 mtime
		long long  mtime = sqlite3_column_int64(stmt, 4);
		// 读取第5个结果 thumbnail_path
		string thumbnail_path = (const char*)sqlite3_column_text(stmt, 5);
		// 读取第6个结果 media_type
		int media_type = sqlite3_column_int(stmt, 6);
		
		media_elem media = {media_id, file_path, name, source_folder, mtime, thumbnail_path, media_type};
		
		mediaVector.push_back(media);
		
		//row++;
	}
	
	// 固定3 运行错误
	if (rc != SQLITE_DONE)
    {
        lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
    }
	
	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
	sqlite3_finalize(stmt);
	lastErr.clear();
	return true;
}


bool SqliteApi::query_all_media_by_source(std::vector<media_elem>& mediaVector, const std::string& source_folder, int media_type, int orderMtime, int orderName, int limit){
	// 目标容器引用固定 清空容器
	mediaVector.clear();
	
	// 固定1 判断数据库是否连接
    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return false;
    }
	
	// 逐步拼接sql
    string sqlBase = R"(
select *
from media
)";
	string whereSql = " where source_folder = ?";
	
	// 判断是否限制查询媒体类型 图片 0 视频 1
	bool hasType = false;
	if(media_type >= 0){
		whereSql += " and media_type = ?";
		hasType = true;
	}
	
	// 判断是否排序，支持mtime name排序
	if(orderMtime == 0){
		if(orderName == 1){
			whereSql += " order by name asc";
		}
		else if(orderName == 2){
			whereSql += " order by name desc";
		}
	}
	else if(orderMtime == 1){
		whereSql += " order by mtime asc";
		 
		if(orderName == 1){
			whereSql += ", name asc";
		}
		else if(orderName == 2){
			whereSql += ", name desc";
		}
	}
	else if(orderMtime == 2){
		whereSql += " order by mtime desc";
		
		if(orderName == 1){
			whereSql += ", name asc";
		}
		else if(orderName == 2){
			whereSql += ", name desc";
		}
	}
	
	string limitSql = " limit ?";
	
    // 完整SQL
    string std_sql = sqlBase + whereSql + limitSql + ";";
    cout << std_sql << endl;
	
	// sql和stmt 查询source_folder所有media
	sqlite3_stmt* stmt = nullptr;
	
	// 固定2 预编译sql句柄，使用二级指针为stmt赋值
	int rc = sqlite3_prepare_v2(db, std_sql.c_str(), -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		// 函数返回错误码（失败）：stmt 不会被初始化，内部仍然是之前赋值的 nullptr
		// 此时不需要sqlite3_finalize，只有prepare成功后才能释放
		lastErr = sqlite3_errmsg(db);
		return false;
	}
	
	// 使用 顺序变量 绑定参数
	int bindIdx = 1; 
	// 第一步 绑定参数 source_folder，一个 text
	rc = sqlite3_bind_text(stmt, bindIdx++, source_folder.c_str(), -1, SQLITE_TRANSIENT);
	// 绑定参数固定 获取lastErr，释放stmt
	if (rc != SQLITE_OK)
	{
		lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
	}
	
	// 第二步：绑定media_type
    if (hasType)
    {
        int rc_bind_int = sqlite3_bind_int(stmt, bindIdx++, media_type);
        if (rc_bind_int != SQLITE_OK)
        {
            lastErr = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            return false;
        }
    }
	
	// 第二步：绑定limit，传-1查询全部
    rc =sqlite3_bind_int(stmt, bindIdx++, limit);
	// 绑定参数固定 获取lastErr，释放stmt
	if (rc != SQLITE_OK)
	{
		lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
	}
	
	//int row = 0;
	while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
		// 读取第0个结果 media_id
		int media_id = sqlite3_column_int(stmt, 0);
		// 读取第1个结果 file_path
		string file_path = (const char*)sqlite3_column_text(stmt, 1);
		// 读取第2个结果 name
		string name = (const char*)sqlite3_column_text(stmt, 2);
		// 读取第3个结果 source_folder
		string source_folder = (const char*)sqlite3_column_text(stmt, 3);
		// 读取第4个结果 mtime
		long long  mtime = sqlite3_column_int64(stmt, 4);
		// 读取第5个结果 thumbnail_path
		string thumbnail_path = (const char*)sqlite3_column_text(stmt, 5);
		// 读取第6个结果 media_type
		int media_type = sqlite3_column_int(stmt, 6);
		
		media_elem media = {media_id, file_path, name, source_folder, mtime, thumbnail_path, media_type};
		
		mediaVector.push_back(media);
		
		//row++;
	}
	
	// 固定3 运行错误
	if (rc != SQLITE_DONE)
    {
        lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
    }
	
	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
	sqlite3_finalize(stmt);
	lastErr.clear();
	return true;
}
	

// 为media添加标签
bool SqliteApi::add_tag_to_media(int media_id, int tag_id){
	// 固定1 判断数据库是否连接
    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return false;
    }
	
	// sql和stmt 为图片添加tag
	const char* sql = "insert or ignore into media_tag_rel(media_id, tag_id) values(?, ?);";
	sqlite3_stmt* stmt = nullptr;
	
	// 固定2 预编译sql句柄，使用二级指针为stmt赋值
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		// 函数返回错误码（失败）：stmt 不会被初始化，内部仍然是你之前赋值的 nullptr
		// 此时不需要sqlite3_finalize，只有prepare成功后才能释放
		lastErr = sqlite3_errmsg(db);
		return false;
	}
	
	// 绑定参数 两个int
	int rc1 = sqlite3_bind_int(stmt, 1, media_id);
	int rc2 = sqlite3_bind_int(stmt, 2, tag_id);
	// 绑定参数固定 获取lastErr，释放stmt
	if (rc1 != SQLITE_OK || rc2 != SQLITE_OK)
	{
		lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
	}
	
	// 插入固定 插入没有SQLITE_ROW，直接返回SQLITE_DONE
	rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE)
    {	
		lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
    }
	
	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
	sqlite3_finalize(stmt);
	lastErr.clear();
	return true;
}

// 为media添加标签 一次多个标签
bool SqliteApi::add_tag_to_media(int media_id, vector<int>& tag_ids){
	// 固定1 判断数据库是否连接
    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return false;
    }
	
	// sql和stmt 为图片添加tag，每次只插入一条，配合事务的开启与提交循环插入
	const char* sql = "insert or ignore into media_tag_rel(media_id, tag_id) values(?, ?);";
	sqlite3_stmt* stmt = nullptr;
	
	// 固定2 预编译sql句柄，使用二级指针为stmt赋值
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		// 函数返回错误码（失败）：stmt 不会被初始化，内部仍然是你之前赋值的 nullptr
		// 此时不需要sqlite3_finalize，只有prepare成功后才能释放
		lastErr = sqlite3_errmsg(db);
		return false;
	}
	
	// 多值插入固定 开启事务
	// 开启事务，将若干个单词插入工作包含在一个事务内提交
	char* errMsg = nullptr;
	rc = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);
	if( rc != SQLITE_OK){
		lastErr = errMsg ? errMsg : "无法开启事务";
		
		// 释放errMsg指向的内存
		sqlite3_free(errMsg);
		sqlite3_finalize(stmt);
		return false;
	}
	
	bool success = true;
	for(int i = 0; i < tag_ids.size(); i++){
        // 重置语句游标，复用stmt
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
		
		// 绑定当前行参数 
		int tag_id = tag_ids[i];
		
		// 绑定参数 两个int
		// 绑定image_id
		int rc1 = sqlite3_bind_int(stmt, 1, media_id);
		// 绑定tag_id
		int rc2 = sqlite3_bind_int(stmt, 2, tag_id);
		// 绑定参数固定 获取lastErr，释放stmt
		if (rc1 != SQLITE_OK || rc2 != SQLITE_OK)
		{
			lastErr = sqlite3_errmsg(db);
			
			// 由于不直接return，所以留到最后再释放句柄
			// sqlite3_finalize(stmt);
			
			// 当绑定参数失败时，退出循环，设置false，用于回滚判断
			success = false;
			break;
		}
		
		// 插入固定 插入没有SQLITE_ROW，直接返回SQLITE_DONE
		rc = sqlite3_step(stmt);
		if(rc != SQLITE_DONE)
		{	
			lastErr = sqlite3_errmsg(db);
			
			// 由于不直接return，所以留到最后再释放句柄
			// sqlite3_finalize(stmt);
			
			// 当插入失败时，退出循环，设置false，用于回滚判断
			success = false;
			break;
		}
	}
	
	// 多值插入固定 提交事务或则回滚
	// 根据success判断是否成功
	if(success){
		// 全部成功，提交事务
		rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg);
		
		// 判断事务提交是否完成
        if (rc != SQLITE_OK) {
            lastErr = errMsg ? errMsg : "提交事务失败";
            sqlite3_free(errMsg);
            success = false;
        } else {
            // 提交成功，清空错误信息
            lastErr.clear();
        }
	}
	else{
        // 任意一条失败 回滚，避免部分写入
        int rollbackRc = sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, &errMsg);
		
		// 判断事务回滚是否完成
        if (rollbackRc != SQLITE_OK) {
			if(errMsg){
				lastErr += std::string("；回滚事务失败: ") + errMsg;
				sqlite3_free(errMsg);
			}
			else{
				lastErr += "回滚事务失败";
			}
            success = false;
        } // 不清空lastErr，因为本来就失败了
	}
	
	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
	sqlite3_finalize(stmt);
	
	// 只在成功时才清空lastErr
	if(success){
		lastErr.clear();
	}
	
	return success;
}

bool SqliteApi::query_medias_by_tags(vector<media_elem>& mediaVector, std::vector<int>& tag_ids, const string& source_folder){
	// 目标容器引用固定 清空容器
	mediaVector.clear();
	if(tag_ids.size() == 0){
		lastErr = "tag列表为空";
        return false;
	}
	
	// 固定1 判断数据库是否连接
    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return false;
    }

	// 动态拼接SQL，仅两种固定格式，无注入风险，支持按tag_name升降排序
	// SQLite 占位符只能替换值，不能替换列名、排序关键字，所以只能固定分支拼接 SQL；
    string sqlBase = "select distinct m.* from media m join media_tag_rel r on m.media_id = r.media_id join tag t on t.tag_id = r.tag_id";
    string std_sql;
	// 拼接where t.tag_id in(,,)
	string tagId_in = " where t.tag_id in(";
	for(int i = 0; i < tag_ids.size(); i++){
		// 安全写法将数字转换为字符串
		tagId_in += std::to_string(tag_ids[i]);
		if(!(i == tag_ids.size() - 1)){
			tagId_in += ",";
		}
	}
	tagId_in += ")"; 
	
	// 拼接 and source_folder = ? 供之后添加参数
	string and_source = "";
	if(!source_folder.empty()){
		and_source =  std::string(" and source_folder = ?");
	}
	// 拼接最终查询语句
	std_sql = sqlBase + tagId_in + and_source + ";";
	cout << std_sql << endl;
	
	// sql和stmt 为图片添加tag，每次只插入一条，配合事务的开启与提交循环插入
	const char* sql = std_sql.c_str();
	sqlite3_stmt* stmt = nullptr;
	
	// 固定2 预编译sql句柄，使用二级指针为stmt赋值
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		// 函数返回错误码（失败）：stmt 不会被初始化，内部仍然是你之前赋值的 nullptr
		// 此时不需要sqlite3_finalize，只有prepare成功后才能释放
		lastErr = sqlite3_errmsg(db);
		return false;
	}
	
	// source_folder不为空，则添加参数
	if(!source_folder.empty()){
		// 绑定参数，一个 text
		rc = sqlite3_bind_text(stmt, 1, source_folder.c_str(), -1, SQLITE_TRANSIENT);
		// 绑定参数固定 获取lastErr，释放stmt
		if (rc != SQLITE_OK)
		{
			lastErr = sqlite3_errmsg(db);
			sqlite3_finalize(stmt);
			return false;
		}
	}
	
	//int row = 0;
	while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
		// 读取第0个结果 media_id
		int media_id = sqlite3_column_int(stmt, 0);
		// 读取第1个结果 file_path
		string file_path = (const char*)sqlite3_column_text(stmt, 1);
		// 读取第2个结果 name
		string name = (const char*)sqlite3_column_text(stmt, 2);
		// 读取第3个结果 source_folder
		string source_folder = (const char*)sqlite3_column_text(stmt, 3);
		// 读取第4个结果 mtime
		long long  mtime = sqlite3_column_int64(stmt, 4);
		// 读取第5个结果 thumbnail_path
		string thumbnail_path = (const char*)sqlite3_column_text(stmt, 5);
		// 读取第6个结果 media_type
		int media_type = sqlite3_column_int(stmt, 6);
		
		media_elem media = {media_id, file_path, name, source_folder, mtime, thumbnail_path, media_type};
		
		mediaVector.push_back(media);
		
		//row++;
	}
	
	// 固定3 运行错误
	if (rc != SQLITE_DONE)
    {
        lastErr = sqlite3_errmsg(db);
		sqlite3_finalize(stmt);
		return false;
    }
	
	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
	sqlite3_finalize(stmt);
	lastErr.clear();
	return true;
}

bool SqliteApi::query_medias_by_tags(vector<media_elem>& mediaVector,
                                     std::vector<int>& tag_ids,
                                     const string& source_folder,
                                     long long start_time, //修改为64位整型
                                     long long end_time,
									 int media_type,
									 int orderMtime, 
									 int orderName,
                                     int limit)
{
    mediaVector.clear();
    if (tag_ids.empty())
    {
        lastErr = "tag列表为空";
        return false;
    }

    if (!isOpen())
    {
        lastErr = "数据库未打开，请先调用open()";
        return false;
    }

    // 基础关联语句
    string sqlBase = R"(
select distinct m.*
from media m
join media_tag_rel r on m.media_id = r.media_id
join tag t on t.tag_id = r.tag_id
)";
    string whereSql = " where t.tag_id in(";

    // 拼接 tag_id in(?, ?, ?) 占位符，不用拼数字字符串
    string tagPlaceHolder;
    for (size_t i = 0; i < tag_ids.size(); i++)
    {
        if (i > 0) tagPlaceHolder += ",";
        tagPlaceHolder += "?";
    }
    whereSql += tagPlaceHolder + ")";

    // 拼接额外筛选条件，按需追加 AND
	// bindIdx作为参数占位索引，tag占用1~tag_ids.size()
    int bindIdx = 1; 
    bool hasSourceFolder = false;
	bool hasTimeSection = false;
	bool hasType = false;

    // 1. 目录筛选
    if (!source_folder.empty())
    {
        whereSql += " and m.source_folder = ?";
        hasSourceFolder = true;
    }
    // 2. 修改时间区间筛选
    if (start_time > 0 && end_time > 0 && start_time <= end_time)
    {
        whereSql += " and m.mtime between ? and ?";
        hasTimeSection = true;
    }
	
	// 3. 文件类型筛选，范围比较，以后可能会有新的type
	if(media_type >= 0){
		whereSql += " and m.media_type = ?";
		hasType = true;
	}
	
	// 4. 判断是否排序，支持mtime name排序
	if(orderMtime == 0){
		if(orderName == 1){
			whereSql += " order by name asc";
		}
		else if(orderName == 2){
			whereSql += " order by name desc";
		}
	}
	else if(orderMtime == 1){
		whereSql += " order by mtime asc";
		 
		if(orderName == 1){
			whereSql += ", name asc";
		}
		else if(orderName == 2){
			whereSql += ", name desc";
		}
	}
	else if(orderMtime == 2){
		whereSql += " order by mtime desc";
		
		if(orderName == 1){
			whereSql += ", name asc";
		}
		else if(orderName == 2){
			whereSql += ", name desc";
		}
	}

    // LIMIT 统一拼接，-1自动返回全部
    string limitSql = " limit ?";

    // 完整SQL
    string std_sql = sqlBase + whereSql + limitSql + ";";
    cout << std_sql << endl;

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, std_sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        lastErr = sqlite3_errmsg(db);
        return false;
    }

    // 第一步：绑定所有tag_id bindIdx从1开始
    for (size_t i = 0; i < tag_ids.size(); i++)
    {
        int rc_bind_int = sqlite3_bind_int(stmt, bindIdx++, tag_ids[i]);
        if (rc_bind_int != SQLITE_OK)
        {
            lastErr = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            return false;
        }
    }

    // 第二步：绑定source_folder
    if (hasSourceFolder)
    {
        int rc_bint_text = sqlite3_bind_text(stmt, bindIdx++, source_folder.c_str(), -1, SQLITE_TRANSIENT);
        if (rc_bint_text != SQLITE_OK)
        {
            lastErr = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            return false;
        }
    }

    // 第三步：绑定时间区间
    if (hasTimeSection)
    {
		// 绑定int64 
        int rc_bind_start = sqlite3_bind_int64(stmt, bindIdx++, start_time);
        int rc_bind_end = sqlite3_bind_int64(stmt, bindIdx++, end_time);
		
		if (rc_bind_start != SQLITE_OK || rc_bind_end != SQLITE_OK)
		{
			lastErr = sqlite3_errmsg(db);
			sqlite3_finalize(stmt);
			return false;
		}
    }
	
    // 第四步：绑定media_type
    if (hasType)
    {
        int rc_bind_int = sqlite3_bind_int(stmt, bindIdx++, media_type);
        if (rc_bind_int != SQLITE_OK)
        {
            lastErr = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            return false;
        }
    }


    // 第五步：绑定limit，传-1查询全部
    sqlite3_bind_int(stmt, bindIdx++, limit);

    // 循环读取结果
	while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
		// 读取第0个结果 media_id
		int media_id = sqlite3_column_int(stmt, 0);
		// 读取第1个结果 file_path
		string file_path = (const char*)sqlite3_column_text(stmt, 1);
		// 读取第2个结果 name
		string name = (const char*)sqlite3_column_text(stmt, 2);
		// 读取第3个结果 source_folder
		string source_folder = (const char*)sqlite3_column_text(stmt, 3);
		// 读取第4个结果 mtime
		long long  mtime = sqlite3_column_int64(stmt, 4);
		// 读取第5个结果 thumbnail_path
		string thumbnail_path = (const char*)sqlite3_column_text(stmt, 5);
		// 读取第6个结果 media_type
		int media_type = sqlite3_column_int(stmt, 6);
		
		media_elem media = {media_id, file_path, name, source_folder, mtime, thumbnail_path, media_type};
		
		mediaVector.push_back(media);
	}

	// 固定3 运行错误
    if (rc != SQLITE_DONE)
    {
        lastErr = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        return false;
    }

	// 固定4 查询结束，释放stmt句柄，清空lastErr字符串
    sqlite3_finalize(stmt);
    lastErr.clear();
    return true;
}