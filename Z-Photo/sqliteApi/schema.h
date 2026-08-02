#ifndef SCHEMA_H
#define SCHEMA_H

#include<string>

/*
	定义字段结构，便于系统中使用
*/

typedef struct {
	int media_id;
	std::string file_path;
	std::string name;
	std::string source_folder;
	long long  mtime;  // 使用64位整型保存时间戳
	std::string thumbnail_path;
	int media_type;      // 0: 图片, 1: 视频
}media_elem;

typedef struct {
    int tag_id;
    std::string tag_name;
}tag_elem;

#endif