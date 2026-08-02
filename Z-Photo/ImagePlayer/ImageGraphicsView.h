#ifndef IMAGEGRAPHICS_VIWE_H
#define IMAGEGRAPHICS_VIWE_H
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QPixmap>
#include <QWheelEvent>
#include <QTransform>
#include <QShowEvent>
#include <QString>
#include <QFont>

/*
	ImageGraphicsView，作为图片查看器
	支持：
	1.使用滚轮缩放图片
	2.鼠标拖拽图片
	
	需要将ImageGraphicsView放入弹窗PreviewWindow中
*/

/*
使用方法，设置ImageGraphicsView的scene
    QGraphicsScene* scene = new QGraphicsScene(&mainWin);
    ImageGraphicsView* view = new ImageGraphicsView(&mainWin);
    view->setScene(scene);
    view->setFixedSize(700, 350);
	
	// 构造函数中创建默认场景
    setScene(new QGraphicsScene(this));
	则可以直接使用
	    m_view = new ImageGraphicsView(this);
		m_view->setImage(imagePath);
		layout->addWidget(m_view);
*/

class ImageGraphicsView : public QGraphicsView
{
    Q_OBJECT
	
signals:
    void imageLoaded(bool success);
	
public:
	// 构造函数
    explicit ImageGraphicsView(QWidget* parent = nullptr);

    // 外部调用
	// 使用file_path加载 本地
    void setImage(const QString& file_path);
	// 使用url加载图片 网络
	void loadFromUrl(QString host, int port, const QString& file_path);
	
	// 清空图片，恢复“暂时没有图片”提示
    void clearImage();

protected:
	/*
		重写WheelEvent，实现图片缩放逻辑，不上传到父类的滚动条逻辑
	*/
    void wheelEvent(QWheelEvent* event) override;

	/*
		重写 showEvent 
		则当视图首次显示时会触发 showEvent，在其中再次调用 
		fitInView，确保图片根据最终显示尺寸进行调整。
	*/
    void showEvent(QShowEvent* event) override;
	
	// 重写按键行为，将左右方向键上传到父控件PreviewWindow
    void keyPressEvent(QKeyEvent* event) override;
	
private:
    // 在画布显示空白提示文字
    void showEmptyTip();

private:
	// 缩放刻度
    double m_scaleStep;
	// QGraphicsPixmapItem，sence上的图片
    QGraphicsPixmapItem* m_item;
	// 文字
	QGraphicsTextItem* m_textTip;
	// 是否是第一加载
	bool m_isFirstShow;
};


#endif