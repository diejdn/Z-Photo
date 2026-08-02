#ifndef IMAGEPLAYER_H
#define IMAGEPLAYER_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QKeyEvent>

// 前向声明，避免在头文件中包含 ImageGraphicsView.h
#include "ImageGraphicsView.h"
// schema.h 必须包含，因为 media_elem 作为值类型成员
#include "schema.h"


/*
	Z-Photo 实现的图片查看器
	
	用法：
	全屏/缩小： 			点按 F11

	放大/缩小图片：		滚动滚轮
	
	拖动图片：			点击鼠标左键进行拖拽
	
	为图片添加标签：		点按t (开启tag弹窗)
	
	切换下一个图片：		点按 d / 点按 右方向键
	切换上一个图片：		点按 a / 点按 左方向键 
*/

/*
	ImagePlayer，作为弹窗，并承载图片查看器ImageGraphicsView
	
	创建一个 预览窗口类 ImagePlayer（继承 QWidget），
	内部包含一个 ImageGraphicsView。

	预览窗口提供 全屏切换 功能（点击按钮或按 F11 键）。

	窗口关闭时自动释放内存（WA_DeleteOnClose）。

	在信号槽中，当收到 clicked 信号时，新建 ImagePlayer 并传入图片路径，显示窗口。
*/

class ImagePlayer : public QWidget
{
    Q_OBJECT
// 自定义信号，发到上层ImageGridWidget进行处理 event->key() == Qt::Key_Left / Qt::Key_Right
/*
	符合“单一职责”和“关注点分离”原则——ImagePlayer 只负责 UI 交互（显示图片、发送信号），
	ImageGridWidget 负责核心业务逻辑(ImageGridWidget有全局图片的路径)
*/
signals:
	// 切换图片信号
    void next(int step);
	
	// 添加标签信号 -> show 添加标签页面
	void addtag(int media_id);
	
public:
	explicit ImagePlayer(QWidget* parent = nullptr);
	
	// 析构函数，当PreviewWindow关闭时输出信息
	~ImagePlayer() { qDebug() << "ImagePlayer destroyed"; }
	
	// 设置图片，供切换图片使用
	// 提供 media 来切换图片，本地 + 网络
	void setMedia(const media_elem& media);
	
protected:
	// 重写KeyEvent键盘事件，实现按F11全屏
    void keyPressEvent(QKeyEvent* event) override;
	
	void closeEvent(QCloseEvent* event) override;

private slots:
	// 自定义槽函数，实现全屏
    void toggleFullScreen();
	
	// 不需要实现窗口放大同步让m_view放大，因为m_view实现滚轮放大+拖拽

private:
    QPushButton* m_fullScreenBtn;
    bool m_isFullScreen; // 判断是否全屏
	
public:
	// media_elem，与media表中字段保持一致
	media_elem med;
	
	// 内置的图片显示组件
	ImageGraphicsView* m_view;
};

#endif // PREVIEWWINDOW_H