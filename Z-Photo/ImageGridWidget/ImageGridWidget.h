#ifndef IMAGEGRIDWIDGET_H
#define IMAGEGRIDWIDGET_H

#include <QWidget>
#include <QGridLayout>
#include <QList>
#include <QPointer>
#include <vector>
#include "schema.h"          // media_elem 是值类型成员，必须包含

// 前向声明，减少头文件依赖
class MediaLabel;
class ZLens;
class AddTagWindow;
class SqliteApi;


/*
	管理一个 QGridLayout 用于显示缩略图。
	作用
		接收图片路径列表，自动创建 ImageLabel 并添加到网格中。
		提供对内部 ImageLabel 列表的访问（用于懒加载遍历）。
		支持动态更新（清空并重新填充）。
		可设置列数、间距等。
	继承QWidget，可以添加布局，也可以直接被添加到其他布局中
*/
/*
	使用实例
	
	#include "ImageGridWidget.h"

	// ... 在创建 scrollArea 后
	ImageGridWidget* gridWidget = new ImageGridWidget(4); // 4列
	gridWidget->setImagePaths(paths);
	scrollArea->setWidget(gridWidget);

	// 获取标签列表用于懒加载
	QList<ImageLabel*> imgList = gridWidget->imageLabels();

	// 后续 updateVisibleImages 仍可遍历 mediaList
*/

class ImageGridWidget : public QWidget
{
    Q_OBJECT
public:
	// 构造函数
    explicit ImageGridWidget(int columns = 4, QWidget* parent = nullptr);

    // 设置图片路径列表，并重新构建网格rebuildGrid
	void setMediaPaths(const std::vector<media_elem>& medias);
	
	// 设置数据库连接句柄
	void setSqliteApi(SqliteApi* sqlite);
	
    // 清空现有的布局项以及QList<ImageLabel*>m_labels
    void clear();

    // 获取所有图片标签（用于懒加载遍历）
    QList<MediaLabel*> mediaLabels() const { return m_labels; }

    // 设置列数（需重建网格）
    void setColumns(int columns);

    // 获取当前列数
    int columns() const { return m_columns; }
	
signals:
	//
	void show_clicked_media_info(const media_elem& media);

protected:
    // 重建网格（清空并重新添加）自动构建QList<MediaLabel*> + 网格，
    void rebuildGrid();

private:
    int m_columns;
    QGridLayout* m_layout;
    QList<MediaLabel*> m_labels;
	
	// 使用QPointer管理g_previewWindow g_AddTagWindow
	/*
		QPointer是 Qt 提供的一个智能指针
		它监听所指向对象的 destroyed 信号，当对象被销毁时，QPointer 
		自动将内部指针置为 nullptr。
	*/
	QPointer<ZLens> g_zlens; 
	
	// 用于标记 zlens 是否属于别的目录
	bool zlens_not_close = false;
	
	// 为媒体添加标签的界面
	QPointer<AddTagWindow> g_AddTagWindow; 
	
	// 记录当前图片的索引，即在 medias 中的下标
	int m_currentIndex = -1;
	
	SqliteApi* m_sqlite;
	
public:
	// image_elem，与image表中字段保持一致，构成QList
	std::vector<media_elem> medias;
};

#endif