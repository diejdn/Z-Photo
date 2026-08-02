#include <iostream>
#include <string>
#include <vector>
#include "sqlite_api.h"

using namespace std;

/*
	编译 找libsqlite3.a
	g++ test.cpp sqlite_api.cpp -L. -lsqlite3 -o test
	或者
	g++ test.cpp sqlite_api.cpp sqlite3.o -o text
*/

int main(){
	
	string db_path = "media.db";
	SqliteApi sqlite(db_path);
	sqlite.open();
	cout << "isOpen: " << sqlite.isOpen() << endl;
	cout << "isForeign: " << sqlite.isForeign() << endl;
	
	/*
	// query_image_source 和 count_by_source测试
	vector<string> list;
	sqlite.query_image_source(list);
	
	for(string item : list){
		cout << item << endl;
	}
	
	for(string item : list){
		int count = sqlite.count_by_source(item);
		
		cout << item << " have img: " << count << endl;
	}
	*/
	
	/*
	// add_tag 测试
	string tag_name = "唯美";
	bool res = sqlite.add_tag(tag_name);
	cout << "add tag is: " << res << endl;
	*/
	
	/*
	// get_all_tag 测试 第一个true表示启动排序，第二个true表示启动降序
	vector<tag_elem> taglist;
	sqlite.get_all_tag(taglist, true, true);
	
	for(tag_elem item : taglist){
		cout << item.tag_id << ": " << item.tag_name << endl;
	}
	*/
	
	
	// query_all_image_by_source 测试
	/*
	vector<image_elem> imgList;
	string source_folder = "data/蕾姆拉姆/p1";
	sqlite.query_all_image_by_source(imgList, source_folder);
	
	for(image_elem img : imgList){
		cout << img.file_path << " " << img.thumbnail_path << endl;
	}
	*/
	
	/*
	// add_tag_to_img 单值测试
	bool res = sqlite.add_tag_to_img(2, 1);
	cout << "add_tag_to_img is: " << res;
	*/
	
	/*
	// add_tag_to_img 多值插入测试 为image_id 3图片插入
	vector<int> tag_ids = {1,2,3,5,7,8,9};
	bool res = sqlite.add_tag_to_img(3, tag_ids);
	if(res){
		cout << "add_tag_to_img multiple success";
	}
	else{
		cout << "err: " << sqlite.getError() << endl;
	}
	*/
	
	/*
	// query_images_by_tags 测试
	vector<int> tag_ids = {1,2,3};
	string source_folder = "data/蕾姆拉姆/p1";
	vector<image_elem> imgList;
	bool res = sqlite.query_images_by_tags(imgList, tag_ids, source_folder);
	if(res){
		cout << "query_images_by_tags success" << endl;
		for(image_elem img : imgList){
			cout << img.image_id << " " << img.file_path << " " << img.thumbnail_path << endl;
		}
	}
	else{
		cout << "err: " << sqlite.getError() << endl;
	}
	*/
	
	/*
	// query_images_by_tags 重载测试
	vector<int> tag_ids = {1,2,3};
	vector<image_elem> imgList;
	bool res = sqlite.query_images_by_tags(imgList, tag_ids, "", 0,0);
	if(res){
		cout << "query_images_by_tags success" << endl;
		for(image_elem img : imgList){
			cout << img.image_id << " " << img.file_path << " " << img.thumbnail_path << endl;
		}
	}
	else{
		cout << "err: " << sqlite.getError() << endl;
	}
	*/
	
	/*
	// get_tags_by_image 测试
	vector<tag_elem> taglist;
	int image_id = 3;
	sqlite.get_tags_by_image(taglist, image_id);
	
	for(tag_elem item : taglist){
		cout << item.tag_id << ": " << item.tag_name << endl;
	}
	*/
	
	/*
	// count_imagenum_by_tag 测试
	int count = sqlite.count_imagenum_by_tag(2);
	cout << count << endl;
	*/
	
	/*
	// delete_tag 测试
	bool res = sqlite.delete_tag(13);
	cout << res << endl;*/
	
	/*
	// query_all_image_by_source 重载测试
	vector<media_elem> mediaList;
	string source_folder = "data/video/telegram";
	sqlite.query_all_media_by_source(mediaList, source_folder, 2, 2, 10);
	
	cout << mediaList.size() << endl;
	for(media_elem media : mediaList){
		cout << media.name << endl;
	}*/
	
	// query_images_by_tags 重载测试
	vector<int> tag_ids = {1,2,3};
	vector<media_elem> imgList;
	bool res = sqlite.query_medias_by_tags(imgList, tag_ids, "", 16000,17000, 1, 2,2,10);
	if(res){
		cout << "query_medias_by_tags success" << endl;
		for(media_elem img : imgList){
			cout << img.media_id << " " << img.name << endl;
		}
	}
	else{
		cout << "err: " << sqlite.getError() << endl;
	}
	
	return 0;
}