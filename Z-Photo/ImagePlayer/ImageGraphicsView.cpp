#include "ImageGraphicsView.h"
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

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QUrl>


ImageGraphicsView::ImageGraphicsView(QWidget* parent)
        : QGraphicsView(parent), m_scaleStep(0.1), m_item(nullptr), m_isFirstShow(true)
	{
		// 创建默认场景，之后可以直接使用该类
		setScene(new QGraphicsScene(this));
		
        // 基础配置
		// 缩放中心跟随鼠标位置，这就是专业看图软件的体验，不要删掉。
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
		
		// 左键按住拖拽平移；
        setDragMode(QGraphicsView::ScrollHandDrag);
		
		// 抗锯齿，缩放画面更平滑。
        setRenderHint(QPainter::Antialiasing);
		
		// 隐藏滚动条
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
	
void ImageGraphicsView::setImage(const QString& file_path)
{
	// 加载图片
	QPixmap pix(file_path);
	qDebug() << "ImageGraphicsView::setImage: " << file_path;
    // 若图片加载失败，显示提示并返回
    if (pix.isNull()) {
        m_item = nullptr;
        showEmptyTip();
		
		emit imageLoaded(false);
        return;
    }
		
	/*
		QGraphicsScene（画布、数据层）
		删除场景里所有图元（Item），包括 QGraphicsPixmapItem、线条、文字等一切图形元素。
		clear() 不会影响视图的缩放、平移状态（这些属于view变换，View 的变换矩阵不受影响）
	*/
	/*
		scene()->clear() 
		会删除场景中所有已添加的图元（Items），并自动释放它们占用的内存。
		这是 Qt 对象树机制的一部分，非常可靠。
	*/
    scene()->clear();
	m_textTip = nullptr;
	
	// QGraphicsPixmapItem：图片对象，不能独立存在，必须挂载到 Scene
    m_item = scene()->addPixmap(pix);

	// view组件可视时（已加载）才渲染图片（未加载时大小为100*100）
	// 不可视（未加载时）只设置m_item，直到组件渲染触发QShowEvent，再加载图片
	/*
		具体流程
		view->setImage(pix) 在 mainWin.show() 之前执行
		此时窗口还没渲染，view->isVisible() → false
		setImage内部只执行到m_item = scene()->addPixmap(pix);
		仅仅把图片图元保存到成员变量 m_item，不执行适配
		mainWin.show() 触发界面绘制
		ImageGraphicsView 第一次显示，自动触发 showEvent
		showEvent 判断 m_item != nullptr 此时控件视口尺寸已经就绪，计算正确，图片正常适配
		
		当切换图片时，isVisible为真（即视图已经加载）
		则可以重置镜头+设置图片 以实现切换图片
	*/
    if (isVisible())
    {
		/*
			QGraphicsView（镜头、视图层）
			把 View 内部的坐标变换矩阵重置为初始状态：
			缩放倍数恢复为 1.0
			图片偏移（拖拽产生的位移）清零
·			所有滚轮缩放、鼠标拖拽造成的镜头变化全部清除。
		*/
		/*
		切换（注意不是第一次加载）不同尺寸图片时会导致图片出现的位置错位
			关键：重置场景边界为当前图元的边界
			QGraphicsScene 有一个 场景矩形（sceneRect），它定义了场景的坐标系范围。
			
			如果未显式设置，场景会自动计算所有图元的包围盒（itemsBoundingRect()）
			作为 sceneRect。
			
			QGraphicsView 的滚动条范围、fitInView 和 centerOn 
			等操作都依赖于当前的 sceneRect。
			
			问题：
			先显示大图（原图） → 场景的自动 sceneRect 会扩展为该大图的尺寸
			（比如 4000×3000）。
			
			切换为小图（缩略图） → 虽然调用了 scene()->clear() 
			并添加了新图元，但场景不会自动缩小 
			sceneRect，它仍然保持之前大图的矩形（除非显式重置或重新计算）。
			
			此时，fitInView(m_item, ...) 
			会计算缩放比例，但它的内部算法会结合当前的 sceneRect 
			来确定视口中心点，以保证整个场景在视口中可见（或按指定矩形适配）。
			
			由于 sceneRect 远大于当前图元，fitInView 为了让整个 sceneRect 
			可见（或基于此计算），会使得图元在视口中偏左或偏右。
			
			即使调用了 centerOn(m_item)，由于 sceneRect 
			很大，视口的中心点映射到场景坐标时可能产生偏移，导致图元不居中。
			
			解决：
			scene()->setSceneRect(m_item->boundingRect());
			显式强制 sceneRect 等于当前图元的包围盒，即与图元尺寸完全匹配。

			这样，fitInView 和 centerOn 
			所依据的场景范围就是图元本身的大小，计算出的变换矩阵和中心点就会精确地将图
			元置于视口中央，不受之前大图的影响。
		*/
        if (m_item) {
            scene()->setSceneRect(m_item->boundingRect());
        }
		
        resetTransform();
        // 先强制居中一次，确保基础位置正确
        if (m_item) centerOn(m_item);
		
        fitInView(m_item, Qt::KeepAspectRatio);
		
        // 再居中一次，确保 fitInView 后视图中心正确
        centerOn(m_item);
    }
	// 发送一个信号，通知gridwidget来show PreviewWindow
	/*
		初始化时，isVisible()可能为空，所以先设置m_item，
		发送信号后gridwidget来show触发showEvent
		
		再在showEvent中fitInView
		
		网络模式的逻辑是一样的
		
		如果时切换，由于isVisible()为真，所以可以直接fitInView
	*/
	emit imageLoaded(true);
}

void ImageGraphicsView::loadFromUrl(QString host, int port, const QString& file_path)
{
    // 先清空当前显示
    scene()->clear();
    m_item = nullptr;
    m_textTip = nullptr; 
	
	// 构造 URL
	// 如果 file_path 包含空格、中文或特殊符号，需要对路径进行 URL 编码，否则服务器可能无法正确
	QString encodedPath = QUrl::toPercentEncoding(file_path);
	QString url = QString("http://%1:%2/images/%3")
					.arg(host).arg(port).arg(encodedPath);

	
    static QNetworkAccessManager manager;
    QNetworkReply* reply = manager.get(QNetworkRequest(url));

    QPointer<ImageGraphicsView> guard(this);
    connect(reply, &QNetworkReply::finished, [this, reply, guard, url]() {
		bool success = true;
        if (!guard) {
            reply->deleteLater();
            return;
        }
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
			qDebug() << "ImageGraphicsView::loadFromUrl data size: " << data.size();
            QPixmap pix;
			if (pix.loadFromData(data)) {
				//scene()->clear();
				m_item = scene()->addPixmap(pix);
				// 延迟执行适配，确保视口就绪
				QTimer::singleShot(0, this, [this]() {
					if (m_item && isVisible()) {
						// 完整重置场景矩形和变换（与本地加载一致）
						scene()->setSceneRect(m_item->boundingRect());
						resetTransform();
						fitInView(m_item, Qt::KeepAspectRatio);
						centerOn(m_item);
						qDebug() << "ImageGraphicsView::loadFromUrl fitInView successfully";
					}
					// 在适配完成后发射信号，确保窗口显示时图片已居中
					emit imageLoaded(true);
				});
			} 
			else {
				success = false;
				// 加载数据失败（图片数据无效）
				qDebug() << "ImageGraphicsView::loadFromUrl loadFromData err: image Invalid " << url;
			}
        }
		else{
			success = false;
			// 网络错误
			qDebug() << "ImageGraphicsView::loadFromUrl: net err " << reply->errorString()  << " url:"<< url;
		}
		
		if(!success){
			// 失败统一处理
			// 清理场景并显示“暂时没有图片”
			scene()->clear();
			m_item = nullptr;
			m_textTip = nullptr;
			showEmptyTip();
			emit imageLoaded(false); 
		}
		// 成功与否都是释放
		reply->deleteLater();
    });
}

// 清空图片，恢复“暂时没有图片”提示
void ImageGraphicsView::clearImage()
{
    scene()->clear();
    m_item = nullptr;
	m_textTip = nullptr; 
	resetTransform();   // 清除缩放和平移
	centerOn(0, 0);     // 使场景原点居中
    showEmptyTip();
}

// 重写WheelEvent，实现图片缩放逻辑，不上传到父类的滚动条逻辑
void ImageGraphicsView::wheelEvent(QWheelEvent* event)
{
    // 没有图片时禁止缩放
    if (m_item == nullptr)
    {
        event->accept();
        return;
    }
		
	// >0 放大
    double factor = event->angleDelta().y() > 0 ? (1 + m_scaleStep) : (1 - m_scaleStep);
	// 获取当前尺寸
    double currentScale = transform().m11();
    double newScale = currentScale * factor;

	// 调整范围，0.01 ~ 50 倍
    if (newScale < 0.01 || newScale > 50.0){
        return;
	}

    scale(factor, factor);
    event->accept();
	/*
		在 Qt 的事件系统中，每个事件对象都有一个 接受（accepted） 
		或 忽略（ignored） 的状态。
		调用 event->accept() 表示该事件已被当前对象处理完毕，不再需要进一步传播；
		反之，若调用 event->ignore()，则事件会继续向上传递给父组件或默认处理器。
		
		消费滚轮事件，阻止事件继续传递给 QGraphicsView 内置的滚动条逻辑。
		如果不写，视图默认想用滚轮滚动画面，缩放代码不会生效。
		
		即使您已经通过 setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff) 关闭了滚动条，
		QGraphicsView 内部仍然有默认的滚轮事件处理器
		（例如，若滚动条隐藏但仍有焦点，可能触发视图的滚动逻辑）。
		如果不调用 accept()，事件可能会沿父链传递或触发默认行为，
		导致缩放和滚动同时发生（或缩放失效）。
	*/
}

/*
	重写 showEvent：如果图片在视图可见之前就已设置（例如在 setImage 被调用时视图还未显示）
	则当视图首次显示时会触发 showEvent，在其中再次调用 fitInView，确保图片根据最终显示尺寸进行调整。
	
	当视图首次加载时会触发showEvent事件，如果在视图加载前就设置了图片，则重新渲染
*/
void ImageGraphicsView::showEvent(QShowEvent* event)
{
    QGraphicsView::showEvent(event);
		
    // view组件可视时（已加载）还没有图片，展示提示文字
    if(m_isFirstShow)
    {
        m_isFirstShow = false;
        if(m_item == nullptr)
        {
            showEmptyTip();
        }
    }
	
	// m_item不为空，则加载图片，在目标矩形内等比例缩放； 图片完整显示，不会拉伸变形；
    if (m_item != nullptr)
    {
        resetTransform();
        fitInView(m_item, Qt::KeepAspectRatio);
    }
	else{
		showEmptyTip();
	}
}

// 重写按键行为，将左右方向键上传到父控件PreviewWindow，左右方向键被ImageGraphicsView截获了
void ImageGraphicsView::keyPressEvent(QKeyEvent* event)
{
	/*
		左右方向键被ImageGraphicsView接受用于控制滚轮（其实这里隐藏了滚动条）
		需要写接收到左右方向键的行为，不处理信号，自动发送到上层
	*/
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        event->ignore();   // 不处理，让父窗口接收
    } else {
        QGraphicsView::keyPressEvent(event);  // 其他键保持默认行为
    }
}

void ImageGraphicsView::showEmptyTip()
{
    m_textTip = new QGraphicsTextItem("暂时没有图片");
    QFont font;
    font.setPointSize(16);
    m_textTip->setFont(font);

    // 文字居中
    QRectF rect = m_textTip->boundingRect();
    m_textTip->setPos(-rect.width() / 2.0, -rect.height() / 2.0);
    
	scene()->addItem(m_textTip);
}