#ifndef SCHEMA_H
#define SCHEMA_H

#include<string>

typedef struct {
	int image_id;
	std::string file_path;
	std::string name;
	std::string source_folder;
	long long  mtime;  // 使用64位整型保存时间戳
	std::string thumbnail_path;
}image_elem;

typedef struct {
    int tag_id;
    std::string tag_name;
}tag_elem;

#endif