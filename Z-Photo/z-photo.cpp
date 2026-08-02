// Qt Core
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QWindow>

// Qt GUI
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QTextEdit>
#include <QLineEdit>
#include <QWidget>

// Layouts
#include <QHBoxLayout>
#include <QVBoxLayout>

// Other
#include <QRect>
#include <QListWidget>
#include <QListWidgetItem>

// C++ standard
#include <iostream>
#include <string>
#include <vector>

// Project headers
#include "sqlite_api.h"
#include "MediaLabel.h"
#include "ImageGridWidget.h"
#include "AddTagWindow.h"
#include "DateTimeUtils.h"
#include "StartupDialog.h"
#include "AppConfig.h"
#include "limitRandom.h"
#include "ZLens.h"

using namespace std;

// 判断媒体是 图片 还是 视频
static const QStringList imageExts = {"jpg", "jpeg", "png", "gif", "bmp", "webp", "tiff"};
static const QStringList videoExts = {"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm"};

/*
	自定义MyMainWindow继承QMainWindow
	由于app.setQuitOnLastWindowClosed(false);存在
	
	所以重写closeEvent，关闭MyMainWindow时退出程序
*/
class MyMainWindow : public QMainWindow
{
public:
    explicit MyMainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {}

protected:
    void closeEvent(QCloseEvent *event) override {
        QMainWindow::closeEvent(event);  // 正常关闭事件处理
        qApp->quit();                    // 退出应用程序
    }
};

int main(int argc, char *argv[])
{
	qputenv("QT_MEDIA_BACKEND", "ffmpeg");
	
	// 要先写app，读取命令行输入
	// 如果先初始化数据库句柄，则会先输出Open database successfully，被app用命令行接受
	QApplication app(argc, argv);
	
	
	QStringList args = app.arguments();
	// 处理 --help 参数
	if (args.contains("--help") || args.contains("-h")) {
		std::cout << "========================================" << std::endl;
		std::cout << "  Z-Photo Media Viewer" << std::endl;
		std::cout << "========================================" << std::endl;
		std::cout << "Usage:" << std::endl;
		std::cout << "  z-photo.exe                  Start GUI main interface (Media Manager)" << std::endl;
		std::cout << "  z-photo.exe <file_path>      Open a single image or video file directly" << std::endl;
		std::cout << "  z-photo.exe --help / -h      Show this help message" << std::endl;
		std::cout << std::endl;
		std::cout << "Supported Formats:" << std::endl;
		std::cout << "  Images: jpg, jpeg, png, gif, bmp, webp, tiff" << std::endl;
		std::cout << "  Videos: mp4, avi, mkv, mov, wmv, flv, webm" << std::endl;
		std::cout << std::endl;
		std::cout << "Tips:" << std::endl;
		std::cout << "  In GUI mode, you can browse directories, manage tags, and support network mode." << std::endl;
		std::cout << "========================================" << std::endl;
		return 0;
	}
	
	// 单文件预览模式
	if (args.size() == 2){
		// args[0] 是程序自身路径，args[1] 是第一个参数（视频绝对路径，适配本地模式）
		QString filePath = args[1];
		QString ext = QFileInfo(filePath).suffix();  // 返回 "jpg"、"png"、"mp4" 等
		
		// 设置本地模式，便于复用图片，视频播放器
		AppConfig::instance().setMode(AppConfig::Local);
		
		int media_type;
		if (imageExts.contains(ext)) {
			media_type = 0;
		} else if (videoExts.contains(ext)) {
			media_type = 1;
		} else {
			qWarning() << "unsupport file format: " << ext;
			
			// 暂时阻塞，输出提示信息
			qDebug() << "Press Enter to exit...";
			std::cin.get(); 
			return -1;
		}
		
		// 构造临时media_elem
		media_elem media = {1, filePath.toStdString(), "", "", 0, "", media_type};
		
		// 初始化ZLensl类，开始播放 图片 或 视频
		ZLens zlens;
		zlens.showMedia(media);
		
		return app.exec();
	}
	
	/*
		setQuitOnLastWindowClosed
		设置当最后一个可视窗口被关闭时，应用程序是否自动退出。 默认行为（true）
		
		dlg.exec() 弹出这个模态对话框。 用户选择后，对话框关闭（accept）。
		此时 mainWin（主窗口）还没有显示出来。
		StartupDialog 关闭时，Qt 发现最后一个窗口（StartupDialog）已经关闭
		所以会直接quit()
		
		setQuitOnLastWindowClosed(false)
		确保StartupDialog dlg关闭时程序不直接退出，而是运行mainWin
	*/
	app.setQuitOnLastWindowClosed(false); 
	
    // 显示启动对话框，选择本地或网络模式
    StartupDialog dlg;
    if (dlg.exec() != QDialog::Accepted) {
        return 0; // 用户取消
    }

    // 根据用户选择设置配置 AppConfig::instance()保存配置
    if (dlg.isNetworkMode()) {
        AppConfig::instance().setMode(AppConfig::Network);
        AppConfig::instance().setServerHost(dlg.serverHost());
        AppConfig::instance().setServerPort(dlg.serverPort());
		
		AppConfig::instance().setLocalRoot("");
    } else {
        AppConfig::instance().setMode(AppConfig::Local);
		AppConfig::instance().setLocalRoot(dlg.dataRoot());
    }

	// 输出配置信息
	if(AppConfig::instance().mode() == AppConfig::Local){
		qDebug() << "Mode Local";
	}
	else{
		qDebug() << "Mode Network, remote server: " << AppConfig::instance().serverHost() << ":" << AppConfig::instance().serverPort();
	}
	
	// sqliteapi 此部分在网络模式 需要使用从远端下载的 db文件
	// string db_path = "media.db";
	qDebug() << "db path: " << dlg.dbPath();
	string db_path = dlg.dbPath().toStdString();
	// 初始化api类
	SqliteApi sqlite(db_path);
	// 打开数据库连接
	sqlite.open();
	// 输出控制信息
	qDebug() << "db isOpen: " << sqlite.isOpen();
	qDebug() << "db isForeign: " << sqlite.isForeign();

    // 初始化主窗口
	MyMainWindow mainWin;
    mainWin.setWindowTitle("Z-Photo");
    mainWin.resize(1200, 600);
	
	// 加载主窗口样式表
	QFile styleFile(":/style/mainstyle.qss");
	if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
		mainWin.setStyleSheet(styleFile.readAll());
	} else {
		qDebug() << "con`t load mainstyle";
	}
	
// 公用控制变量
	// 本地模式时，需要data_root，用来和media_elem中的file_path进行拼接，用于本地加载。即本地模式需要绝对路径
	string local_root = AppConfig::instance().localRoot().toStdString();
	qDebug() << "local_root: " << local_root;
	
	// 设置变量来接受按钮的情况，作为后续查询的参数
	bool is_all_dir = false;   // true=所有目录, false=仅本目录（和source_folder进行配合）
	bool is_limit = false;    // true=限制数量, false=全部结果
	bool is_time_range = false;
	
	// 保存当前选中的目录
	string source_folder = "";
	
    // 保存所有图片控件，滚动滚轮时遍历
	QList<MediaLabel*> mediaList;
	
	// 保存sqlite查询返回的source_folder
	vector<string> dirVector;
	
	// 保存sqlite查询返回的media_elem
	vector<media_elem> mediaVector;

// a 控件布局部分
    // 中心部件
    QWidget* centralWidget = new QWidget(&mainWin);
	// 设置 centralWidget 内边距全为0
	centralWidget->setContentsMargins(0, 0, 0, 0);
	// 为窗口绑定centerWidget
    mainWin.setCentralWidget(centralWidget);
	
	// 为 centralWidget 绑定布局，主布局水平布局
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
	// 主布局mainLayout的内边距也为0
	mainLayout->setContentsMargins(0, 0, 0, 0);
	// 布局中各子组件的空隙 mainLayout只有一个子组件splitter
	mainLayout->setSpacing(0);

    // 左右可拖动分割器 QSplitter 核心控件
    // Qt自带可拖拽分割线，替代手动布局，用户可以自由拉动左右宽度
	/*
		Qt::Horizontal代表水平分割 子控件左右横向排列，分割线垂直，鼠标左右拖动调整宽度。
		Qt::Horizontal → 左右布局（竖分割线）
			[控件A] | [控件B]
		Qt::Vertical → 上下布局（横分割线）
			[控件A]
			-------
			[控件B]

		centralWidget设置父对象 窗口销毁时，自动回收 splitter 内存，防止内存泄漏；
		父对象 ≠ 自动进入布局
	*/
    QSplitter* splitter = new QSplitter(Qt::Horizontal, centralWidget);
	// 为主布局上添加splitter，分成[控件A] | [控件B]左右两块
    mainLayout->addWidget(splitter);
	
// 左边布局 垂直
// dir_btn刷新目录列表按钮 editSearch输入目录过滤文本 listDir目录列表 textInfo信息输出区域
	// 在QWidget载体上添加布局后在将载体添加到上层布局中
	QWidget* leftPanel = new QWidget();
	// setObjectName 用于qss定义样式
	leftPanel->setObjectName("leftPanel");
	
	// 左控件（分割线 splitter 左边）垂直布局
	QVBoxLayout* Leftlayout = new QVBoxLayout(leftPanel);
	
	// dir_btn 刷新按钮
	QPushButton* dir_btn = new QPushButton("刷新目录列表");
	
	// editSearch 目录搜索输入框 
	QLineEdit* editSearch = new QLineEdit();
	editSearch->setPlaceholderText("筛选目录（支持中英文模糊搜索）");
	
	// listDir 目录列表
	QListWidget* listDir = new QListWidget();
	
	// textInfo 信息展示区域，也作为公用控制变量，输出各种操作的信息
	QTextEdit* textInfo = new QTextEdit();
	// 设置只读，不能手动编辑
	textInfo->setReadOnly(true); 
	// 预定义展示内容
    QString infoText = R"(图片元数据信息
选中目录：无
图片总数：0

提示：点击左侧目录查看详情
)";
	textInfo->setText(infoText);
	
	// 各控件添加到leftPanel，遵循布局Leftlayout
	Leftlayout->addWidget(dir_btn);
	Leftlayout->addWidget(editSearch);
	Leftlayout->addWidget(listDir);
	Leftlayout->addWidget(textInfo);
	
	// Leftlayout垂直布局，setStretchFactor控制组件的高度权重
	Leftlayout->setStretchFactor(dir_btn, 1);
	Leftlayout->setStretchFactor(editSearch, 1);
	Leftlayout->setStretchFactor(listDir, 6);
	Leftlayout->setStretchFactor(textInfo, 4);
	
// 右侧布局 垂直
// topWidget类型/排序选择组件 check_box_widget筛选逻辑组件 scrollArea滚轮区域
	// 右侧的widget控件 之后将rightPanel加入到splitter中
	QWidget* rightPanel = new QWidget();
	rightPanel->setObjectName("rightPanel");
	// 右控件 垂直布局
	QVBoxLayout* rightlayout = new QVBoxLayout(rightPanel);
	
// topWidget 顶部水平布局（类型 + 排序）
	QWidget* topWidget = new QWidget();
	topWidget->setObjectName("topControlBar");
	QHBoxLayout* topLayout = new QHBoxLayout(topWidget);
	topLayout->setSpacing(10);
	topLayout->setContentsMargins(10, 0, 10, 0);

	/*
		获得选择值 mediaTypeCombo->currentData().toInt();
		获得当前文本 mediaTypeCombo->currentText();
	*/
	// 1. 媒体类型选择 图片 0   视频 1
	QLabel* typeLabel = new QLabel("类型:");
	QComboBox* mediaTypeCombo = new QComboBox();
	mediaTypeCombo->addItem("全部", -1);
	mediaTypeCombo->addItem("图片", 0);
	mediaTypeCombo->addItem("视频", 1);
	mediaTypeCombo->setToolTip("选择媒体类型");

	// 2. 修改时间排序
	QLabel* mtimeLabel = new QLabel("时间排序:");
	QComboBox* mtimeSortCombo = new QComboBox();
	mtimeSortCombo->addItem("不排序", 0);
	mtimeSortCombo->addItem("升序", 1);
	mtimeSortCombo->addItem("降序", 2);
	mtimeSortCombo->setToolTip("修改时间排序");

	// 3. 文件名排序
	QLabel* nameLabel = new QLabel("名称排序:");
	QComboBox* nameSortCombo = new QComboBox();
	nameSortCombo->addItem("不排序", 0);
	nameSortCombo->addItem("升序", 1);
	nameSortCombo->addItem("降序", 2);
	nameSortCombo->setToolTip("文件名排序");

	// 添加到顶部布局typeLabel，遵循布局topLayout
	topLayout->addWidget(typeLabel);
	topLayout->addWidget(mediaTypeCombo);
	topLayout->addWidget(mtimeLabel);
	topLayout->addWidget(mtimeSortCombo);
	topLayout->addWidget(nameLabel);
	topLayout->addWidget(nameSortCombo);
	topLayout->addStretch(); // 右侧弹性空间

// check_box_widget筛选逻辑组件 水平 checkAllDir是否查询所有目录 checkLimit是否限制返回数量 querry_by_tags按标签查找按钮
	QWidget* check_box_widget = new QWidget();
	check_box_widget->setObjectName("checkBoxContainer");
	QHBoxLayout* check_box_layout = new QHBoxLayout(check_box_widget);
	
	// 使用chekobox来实现参数控制 修改公有控制变量 is_all_dir 和 is_limit

	// 是否选择所有目录 checkAllDir
	// 控制is_all_dir
	QCheckBox* checkAllDir = new QCheckBox("所有目录");
	/*
		默认未选中时 checked 为 false，即 is_all_dir = false。
		用户点击勾选后，checked 变为 true，is_all_dir 同步更新。
	*/
	QObject::connect(checkAllDir, &QCheckBox::toggled, [&](bool checked) {
		is_all_dir = checked;
		qDebug() << "is_all_dir: " << checked;
	});
	
	// 是否存在返回数量限制 checkLimit 选中后可输入限制数量
	// 控制is_limit
	QCheckBox* checkLimit = new QCheckBox("限制数量");
	
	QLabel* limitLabel = new QLabel("限制数量(random):");
	QSpinBox* limitSpinBox = new QSpinBox();
	// 0 表示不限制，最大可调至 100000  获取输入值limitSpinBox->value();
	limitSpinBox->setRange(-1, 100000);
	limitSpinBox->setValue(-1);          
	limitSpinBox->setToolTip("输入返回的最大图片数，-1 表示不限制");
	// 默认隐藏
	limitLabel->setVisible(false);
	limitSpinBox->setVisible(false);
	// 根据是否选中checkLimit来显示或隐藏limitLabel limitSpinBox
	QObject::connect(checkLimit, &QCheckBox::toggled, [&](bool checked) {
		is_limit = checked;
		limitLabel->setVisible(checked);
		limitSpinBox->setVisible(checked);
		
		// 显示或隐藏时都将limitSpinBox设置为-1
		limitSpinBox->setValue(-1); 
		qDebug() << "is_limit: " << checked;
	});
	
	// 是否限制时间范围 checkTimeRange 选中后输入开始时间和结束时间
	// 时间范围复选框
	QCheckBox* checkTimeRange = new QCheckBox("时间范围");

	/*
		获取时间信息
		QDateEdit* startDateEdit
		startDate = startDateEdit->date();
		endDate = endDateEdit->date();
		startDate.year(), startDate.month(), startDate.day()
	*/
	// 开始日期编辑框
	QDateEdit* startDateEdit = new QDateEdit();
	startDateEdit->setCalendarPopup(true);
	startDateEdit->setDisplayFormat("yyyy-MM-dd");
	startDateEdit->setDate(QDate::currentDate());  // 默认当天
	startDateEdit->setVisible(false);

	// 结束日期编辑框
	QDateEdit* endDateEdit = new QDateEdit();
	endDateEdit->setCalendarPopup(true);
	endDateEdit->setDisplayFormat("yyyy-MM-dd");
	endDateEdit->setDate(QDate::currentDate());
	endDateEdit->setVisible(false);

	// 根据复选框状态切换是否显示日期编辑框
	QObject::connect(checkTimeRange, &QCheckBox::toggled, [&](bool checked) {
		is_time_range = checked;
		startDateEdit->setVisible(checked);
		endDateEdit->setVisible(checked);
		qDebug() << "is_time_rang: " << checked;
	});
	
	// 按标签查询按钮 querry_by_tags 点击后跳出弹窗选择标签
	// 使用QPointer管理指针，实例销毁时自动将指针置为空
	QPointer<AddTagWindow> queryWin = nullptr;
	QPushButton* querry_by_tags = new QPushButton("按标签查询");
	// 补充setObjectName，不然不会加载样式
	querry_by_tags->setObjectName("queryBtn");  
	
	// 标签管理按钮 tag_manage 点击后跳出弹窗管理标签
	QPointer<AddTagWindow> manageWin = nullptr;
	QPushButton* tag_manage = new QPushButton("标签管理");
	// 补充setObjectName
	tag_manage->setObjectName("manageBtn"); 

	// 将各控件加入到check_box_widget，遵循布局check_box_layout
	check_box_layout->addWidget(checkAllDir);
	check_box_layout->addWidget(checkLimit);
	check_box_layout->addWidget(limitLabel);
	check_box_layout->addWidget(limitSpinBox);
	check_box_layout->addWidget(checkTimeRange);
	check_box_layout->addWidget(startDateEdit);
	check_box_layout->addWidget(endDateEdit);
	check_box_layout->addWidget(querry_by_tags);
	check_box_layout->addWidget(tag_manage);

// scrollArea滚轮区域，网格布局，用于显示缩略图medialabel gridWidget内置网格布局
// scrollArea建立
	// 设置滚动容器，可以滚动的区域，是widget，之后会添加到垂直布局mainLayout中
    QScrollArea* scrollArea = new QScrollArea();
	// 让内部 scrollContent 跟随网格布局自动扩张高度。 必须开启
    scrollArea->setWidgetResizable(true);
	// 关闭水平滚动，因为主布局是垂直布局，是不会水平方向超过布局的
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	// 需要时显示垂直滚动条
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	
// gridWidget内置网格布局，其中实现部分核心功能
	ImageGridWidget* gridWidget = new ImageGridWidget(4);
	// setSqliteApi需要SqliteApi*
	gridWidget->setSqliteApi(&sqlite);
	// 将gridWidget添加到scrollArea
	scrollArea->setWidget(gridWidget);
	
	
	// 将add_tag_widget check_box_widget scrollArea加入rightPanel，遵循布局rightlayout
	rightlayout->addWidget(topWidget);
	rightlayout->addWidget(check_box_widget);
	rightlayout->addWidget(scrollArea);
	
	// rightlayout垂直布局，setStretchFactor控制组件的高度权重
	rightlayout->setStretchFactor(topWidget, 1);
	rightlayout->setStretchFactor(check_box_widget, 1);
	rightlayout->setStretchFactor(scrollArea, 9);
	
// 将leftPanel rightPanel添加到splitter
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    // 设置初始左右宽度比例 4:6
    splitter->setSizes({320, 580});
	
	
//b 逻辑控制部分
	// 1. dir_btn 刷新目录列表 sqlite.query_image_source
	// 信号槽：点击按钮刷新目录项，引用捕获dirList，listDir是指针，可以值捕获 textInfo输出操作信息
    QObject::connect(dir_btn, &QPushButton::clicked,[&](){
		// 首先判断数据库是否连接
		if (!sqlite.open()) {
			qDebug() << "sqlite not open!";
			return;
		}
		
		// 首先从数据库读取所有source_folder，存入dirVector中，之后直接使用dirVector过滤
		if(dirVector.size() == 0){
			// 先清空容器，不清空也没有问题
			dirVector.clear(); 
			bool res = sqlite.query_media_source(dirVector);
			
			if(!res){
				// 取出数据库错误
				QString err = QString::fromStdString(sqlite.getError());
				textInfo->setText(QString("刷新目录列表失败：%1").arg(err));
				
				return;
			}
		}
 
		// 按照editSearch的内容进行过滤 toLower只有 Unicode 拉丁字母（A-Z）会转为小写 a-z，其他字符不会改变
		QString keyword = editSearch->text().trimmed().toLower();
		std::vector<std::string> filtered;
		// 过滤文本为空，则返回所有source_folder
		if(keyword.isEmpty()){
			filtered = dirVector;
		}
		// 存在过滤文本，则从dirVector中过滤
		else{
			for(const auto& path : dirVector){
				QString p = QString::fromStdString(path).toLower();
				if(p.contains(keyword)){
					filtered.push_back(path);
				}
			}
		}
		
		// 刷新UI QListWidget，将过滤出的 目录 作为QListWidget的元素
		listDir->clear();
		for(const auto& path : filtered){
			new QListWidgetItem(QString::fromStdString(path), listDir);
		}
    });
	
	
	
	/*
		核心功能
		
		懒加载实现：仅加载可视区域内的缩略图，节省内存
	
		统一刷新图片逻辑
		作用于gridWidget中的网格元素MediaLabel，控制其是否加载图片
		内部使用滚轮的位置来计算可视缓冲区
		
		所以在图片刚加载时，需要手动触发滚轮事件加载初始可视缓冲区中获得
		lamda表达式，引用捕获&scrollArea, &mediaList
		
		forceReload 强制刷新
		
		滚轮值变化 forceReload = false
		界面resize forceReload = true
	*/
	auto updateVisibleImages = [&scrollArea, &mediaList](bool forceReload)
	{
		// 视口在 scrollContent 上的相对坐标矩形
		/*
			拿到可视窗口自身的矩形，坐标原点是 viewport 自己左上角，
			此时 viewRect 的 y 范围永远是 0 ~ viewport高度。
			举例 窗口可视区域高 350，宽 700，得到 QRect(0,0,700,350)。
		*/
		QRect viewRect = scrollArea->viewport()->rect();
		
		/*
			int offset = scrollArea->verticalScrollBar()->value();
			viewRect.translate(0, offset);
			
			translate(dx, dy)：给矩形整体坐标加上偏移。
			滚动偏移 offset=150，viewRect 的 y 坐标全部 + 150；
			
			原本视口 y∈[0,350] → 变成 y∈[150,500]。
			现在 viewRect 代表scrollContent 中当前正在被用户看见的那一块矩形区域。
			
			即将viewRect视口的坐标映射到scrollContent中
		*/
		viewRect.translate(0, scrollArea->verticalScrollBar()->value());
		
		/*
			再上下扩展400像素缓冲 参数 左 上 右 下
			上边界向上扩展 400 像素
			下边界向下扩展 400 像素
			原本可视矩形只包含屏幕里看得见的区域，现在上下各多出 400px 缓冲区域。
			
			减少频繁加载 / 卸载反复切换
		*/
		viewRect.adjust(0, -400, 0, 400);

		for(MediaLabel* lab : mediaList)
		{
			/*
				lab->geometry() 直接拿到 label 在 scrollContent 上的位置矩形；
				
				获取每个图片 Label 在 scrollContent 中的位置矩形
					类似于QRect(0,0,700,350)
				geometry() 返回控件相对于父窗口（scrollContent） 的矩形，
				坐标原点是 scrollContent 左上角。
			*/
			QRect labRect = lab->geometry();
			
			/*
				判断图片label是否再视图装口内
				intersects(rect)：判断两个矩形有没有重叠区域。
				重叠：Label 有一部分出现在可视窗口 → 加载图片；
				完全不重叠：Label 在屏幕上方 / 下方看不见，跳过。
			*/
			bool isVisible = labRect.intersects(viewRect);
			
			if(isVisible)
			{
				/* 
				正常滚动：仅未加载才加载；窗口缩放强制重新加载 forceReload = false
				1. (!(isLoaded() = false)=true 未加载 || forceReload=false) = true
				2. (!(isLoaded() = true)=false 已加载 || forceReload=false) = false
				正常滚动，forceReload=false，不会进入lab->update();
				!(isLoaded() = false)=true  未加载标签只会loadImage
				已加载图片不会进入lab->update();
				
				窗口放大后 Label 尺寸变大，但图片已经加载过（m_loaded=true），
				不会重新缩放绘制大图，图片还是旧小尺寸，模糊不变大。
				
				界面resize forceReload = true
				1. (!(isLoaded() = false)=true 未加载 || forceReload=true) = true
				2. (!(isLoaded() = true)=false 已加载 || forceReload=true) = true
				界面resize 只要在视图内的标签都会加载/重加载
				!(isLoaded() = false)=true 未加载的直接loadImage
				forceReload=true 已加载直接执行lab->update()进行重绘制（不用卸载图片在加载）
				
				*/
				if (!lab->isLoaded()) {
					// 未加载 → 加载图片
					lab->loadImage(AppConfig::instance().mode());   // 内部会调用 update()
					qDebug() << "load: " << lab->path();
				} 
				else if (forceReload) {
					// 已加载且强制刷新 → 只需触发重绘，让 paintEvent 按新尺寸绘制
					lab->update();
					
					qDebug() << "forceReload: " << lab->path();
				}
			}
			else
			{
				/*
					标签离开视图： 界面缩小 / 滚轮滑动 已加载则直接卸载
				*/
				if(lab->isLoaded()){
					lab->unloadImage();
					
					qDebug() << "unload: " << lab->path();
				}
			}
		}
	};
	
	// 3. listDir点击事件
    // 信号槽：点击左侧目录项，右侧加载缩略图
    QObject::connect(listDir, &QListWidget::itemClicked, [&](QListWidgetItem* item){
		// 首先判断数据库是否连接
		if (!sqlite.open()) {
			qDebug() << "sqlite not open!";
			return;
		}
		
		// 注意，一定要先清空容器
		mediaVector.clear();
		
		// 获取点击的 目录
        QString q_source_folder = item->text();
		// 转换为std::string 作为sqlite方法的参数
		string std_dirName = q_source_folder.toStdString();
		
		// 修改公有控制变量source_folder，表示当前选中目录（之后用于按标签查询）
		source_folder = q_source_folder.toStdString();
		
		qDebug() << "public source_folder: " << source_folder;
		
		// 统计数据库返回的记录数量
		int count = 0;
		// 保存数据库查询时出现的错误
		QString err;
		
		// 直接从limitSpinBox获取限制值，默认为-1
		int limit = limitSpinBox->value();
		
		// 从mediaTypeCombo获取 查询的媒体类型
		int media_code =  mediaTypeCombo->currentData().toInt();
		// 从mtimeSortCombo nameSortCombo获取是否需要排序
		int mtime_sort = mtimeSortCombo->currentData().toInt();
		int name_sort = nameSortCombo->currentData().toInt();
		
		// 获取文字描述
		QString media_type =  mediaTypeCombo->currentText();
		QString mtime_type = mtimeSortCombo->currentText();
		QString name_type = nameSortCombo->currentText();
		
		// 输出日志 以及 拼接params用于在UI显示
		qDebug() << "params is: " << "source_folder: " << q_source_folder << " media_type: " << media_code << " mtime_sort: " << mtime_type << " name_sort: " << name_sort << " limit: " << limit;
		QString params = QString(R"(source_folder: %1,  media_type: %2, mtime_sort: %3, name_sort: %4, limit: %5)").arg(q_source_folder).arg(media_type).arg(mtime_type).arg(name_type).arg(limit);
		
		bool res = sqlite.query_all_media_by_source(mediaVector, source_folder, media_code, mtime_sort, name_sort, -1);
		// 查询成功
		if(res){
			// 图片总数
			count = mediaVector.size();
			
			err = QString("目录打开成功");
			
			// 如果有限制返回，则使用Fisher_Yates随机返回指定数量的记录
			if(limit != -1 && limit != 0){
				Fisher_Yates(mediaVector, limit);
				qDebug() << "Fisher_Yates limit and random";
				
				// 重置限制数量
				limitSpinBox->setValue(-1);
			}
			
			// 当本地模式时，拼接本地路径
			if(AppConfig::instance().mode() == AppConfig::Local){
				for(media_elem& media : mediaVector){
					media.thumbnail_path = local_root + media.thumbnail_path;
					media.file_path = local_root + media.file_path;
				}				
			}

			qDebug() << "query_all_media_by_source mediaVector size: " << count;


			// 切换mediaVector固定步骤
			gridWidget->setMediaPaths(mediaVector);
			// imgList是指针，这里修改指针内容，使用引用捕获
			mediaList = gridWidget->mediaLabels();
			// 强制布局立即更新 QGridLayout 立即计算所有子控件的几何位置，用于updateVisibleImages可视区缓冲区计算标签位置 使 geometry() 返回正确值。
			gridWidget->layout()->activate();
			
			// 重置滚轮到顶部
			// 手动开关信号阻塞，防止 setValue 触发 valueChanged（启动 scrollTimer）
			scrollArea->verticalScrollBar()->blockSignals(true);
			scrollArea->verticalScrollBar()->setValue(0);
			scrollArea->verticalScrollBar()->blockSignals(false);

			// 延迟到布局几何计算完成后，再调用updateVisibleImages加载缩略图
			// 0 毫秒不表示“立刻”，而是表示 “放在事件队列的最后面，等当前正在执行的所有代码跑完，且主事件循环空闲时再执行”。
			QTimer::singleShot(0, [&]() {
				updateVisibleImages(false);
			});
		}
		// 查询失败，则从数据库中读取错误信息
		else{
			count = 0;
			err = QString::fromStdString(sqlite.getError());
		}
		// 在textInfo上展示操作输出信息
		// 使用 C++11 原始字符串字面量 R"(...)"，不用手动转义换行、双引号，多行文本非常清爽；
		// .arg(q_source_folder) 添加参数， 参数使用%1进行占位
        QString newInfo = QString(R"(图片元数据信息
选中目录：%1

查询参数：%2

图片总数：%3

限制数量：%4

目录打开成功与否：%5

备注：可以在这里展示sqlite查询出来的信息
)").arg(q_source_folder).arg(params).arg(count).arg(limit).arg(err);
        textInfo->setText(newInfo);
    });
	
	// 4.querry_by_tags 按标签查询
	// 引用捕获sqlapi实例sqlite，取地址传给AddTagWindow 引用捕获指针queryWin，因为要修改指针的值
    QObject::connect(querry_by_tags, &QPushButton::clicked,[&](){
		// 首先判断数据库是否连接
		if (!sqlite.open()) {
			qDebug() << "sqlite not open!";
			return;
		}
		
		// AddTagWindow* queryWin 如果指针不初始化为nullptr，则指针是垃圾值，直接跳过if
		if(!queryWin){
			// queryWin是指针，这里修改指针内容，使用引用捕获
			queryWin = new AddTagWindow(&sqlite, -1);
			qDebug() << "querry_by_tags create AddTagWindow queryWin";
			
			// queryWin初始化后才能用于连接，在queryWin中按 标签筛选 按钮 queryWin发送带有参数vector<int>的信号，这里接受信号进行处理
			QObject::connect(queryWin, QOverload<std::vector<int>>::of(&AddTagWindow::tagSelected), [&](vector<int> tag_ids) {
				// 注意，一定要先清空容器
				mediaVector.clear();
				
				// 接受到 vector<int> ，拼接为日志，之后添加到ui
				QString select_tag_ids = "";
				qDebug() << "AddTagWindow signal tagSelected, tag_ids is:";
				for(int tag_id : tag_ids){
					qDebug() << tag_id;
					select_tag_ids += QString::number(tag_id) + ",";
				}
				
				// 直接从limitSpinBox获取限制值，默认为-1
				int limit = limitSpinBox->value();
	
				// 是否查询所有目录
				if(is_all_dir){
					// 将source_folder置为空
					source_folder = "";
					
					// 清空listDir的选中状态
					listDir->clearSelection();
				}
				
				// 是否存在时间区间
				long long start_time = 0;
				long long end_time = 0;
				if (is_time_range) {
					QDate startDate = startDateEdit->date();
					QDate endDate = endDateEdit->date();
					if (startDate.isValid() && endDate.isValid() && startDate <= endDate) {
						time_t start_start, start_end;
						time_t end_start, end_end;
						// 利用 dateToUtcTimestamp 将 本地日期 转换为 标准UTC时间戳
						bool startOk = dateToUtcTimestamp(startDate.year(), startDate.month(), startDate.day(), start_start, start_end);
						bool endOk   = dateToUtcTimestamp(endDate.year(), endDate.month(), endDate.day(), end_start, end_end);
						if (startOk && endOk) {
							start_time = start_start;   // 开始日期的起始时间戳
							end_time   = end_end;       // 结束日期的结束时间戳
						} else {
							// 转换失败，禁用时间筛选
							start_time = 0;
							end_time = 0;
						}
					} else {
						// 日期无效或 start > end，禁用时间筛选
						start_time = 0;
						end_time = 0;
					}
				}
				
				// 获得排序+类型
				int media_code =  mediaTypeCombo->currentData().toInt();
				int mtime_sort = mtimeSortCombo->currentData().toInt();
				int name_sort = nameSortCombo->currentData().toInt();
				
				QString media_type =  mediaTypeCombo->currentText();
				QString mtime_type = mtimeSortCombo->currentText();
				QString name_type = nameSortCombo->currentText();
				
				qDebug() << "params is: " <<  "source_folder " << source_folder <<  " start_time: " << start_time << " end_time: " << end_time << " media_type: " << media_code << " mtime_sort: " << mtime_type << " name_sort: " << name_sort << " limit: " << limit;;
				QString params = QString(R"(source_folder: %1, start_time: %2, end_time: %3, media_type: %4, mtime_sort: %5, name_sort: %5, limit: %7)").arg(QString::fromStdString(source_folder)).arg(start_time).arg(end_time).arg(media_type).arg(mtime_type).arg(name_type).arg(limit);
				
				// 统计数据库返回的记录数量
				int query_count = 0;
				// 保存数据库查询时出现的错误
				QString err;
				
				bool res = sqlite.query_medias_by_tags(mediaVector, tag_ids, source_folder, start_time, end_time, media_code, mtime_sort, name_sort, -1);
				if(res){
					query_count = mediaVector.size();
					err = QString(R"(查询成功)");
					
					// 如果有限制返回，则使用Fisher_Yates随机返回指定数量的记录
					if(limit != -1 && limit != 0){
						Fisher_Yates(mediaVector, limit);
						qDebug() << "Fisher_Yates limit and random";
						
						// 重置限制数量
						limitSpinBox->setValue(-1);
					}
					
					// 当本地模式时，拼接本地路径
					if(AppConfig::instance().mode() == AppConfig::Local){
						for(media_elem& media : mediaVector){
							media.thumbnail_path = local_root + media.thumbnail_path;
							media.file_path = local_root + media.file_path;
						}
					}
					
					qDebug() << "query_images_by_tags mediaVector size: " << query_count;

					// 切换mediaVector固定步骤
					gridWidget->setMediaPaths(mediaVector);
					// mediaList是指针，这里修改指针内容，使用引用捕获
					mediaList = gridWidget->mediaLabels();
					// 强制布局立即更新 QGridLayout 立即计算所有子控件的几何位置，用于updateVisibleImages可视区缓冲区计算标签位置 使 geometry() 返回正确值。
					gridWidget->layout()->activate();
					
					// 重置滚轮到顶部
					// 手动开关信号阻塞，防止 setValue 触发 valueChanged（启动 scrollTimer）
					scrollArea->verticalScrollBar()->blockSignals(true);
					scrollArea->verticalScrollBar()->setValue(0);
					scrollArea->verticalScrollBar()->blockSignals(false);

					// 延迟到布局几何计算完成后，再调用updateVisibleImages加载缩略图
					// 0 毫秒不表示“立刻”，而是表示 “放在事件队列的最后面，等当前正在执行的所有代码跑完，且主事件循环空闲时再执行”。
					QTimer::singleShot(0, [&]() {
						updateVisibleImages(false);
					});
				}
				else{
					query_count = 0;
					err = QString(R"(查询失败： %1)").arg(QString::fromStdString(sqlite.getError()));
				}
				
				// 拼接操作输出信息
				QString newInfo = QString(R"(查询信息
选中的tag_id：%1

查询参数：%2

查询到的图片总数：%3

限制数量：%4

查询是否成功：%5

备注：可以在这里展示sqlite查询出来的信息
)").arg(select_tag_ids).arg(params).arg(query_count).arg(limit).arg(err);
				textInfo->setText(newInfo);
			});
		}
		
		// 由于指针未初始化，有垃圾内容但能跳过!queryWin，导致指针变为悬垂指针。所以使用QPointer进行管理
		queryWin->show();
		qDebug() << "querry_by_tags show AddTagWindow queryWin";
		queryWin->raise();
		queryWin->activateWindow();
		
	});
	
	// 5. tag_manage点击事件，打开管理标签页面
	QObject::connect(tag_manage, &QPushButton::clicked,[&](){
		// 首先判断数据库是否连接
		if (!sqlite.open()) {
			qDebug() << "sqlite not open!";
			return;
		}
		
		if(!manageWin){
			// queryWin是指针，这里修改指针内容，使用引用捕获
			manageWin = new AddTagWindow(&sqlite, -2);
			qDebug() << "tag_manage create AddTagWindow manageWin";
		}
		
		// 由于指针未初始化，有垃圾内容但能跳过!manageWin，导致指针变为悬垂指针。所以使用QPointer进行管理
		manageWin->show();
		qDebug() << "querry_by_tags show AddTagWindow manageWin";
		manageWin->raise();
		manageWin->activateWindow();
	});
	
	// 5. 滚轮变化 加载图片，非强制刷新，只有未加载标签才会loadImage
	// 设计计时器，当个快速滚动滚轮时，不需要频繁 加载/卸载 缩略图
	QTimer scrollTimer;
	scrollTimer.setSingleShot(true);
	scrollTimer.setInterval(300);
	// 计时器时间到，再触发强制刷新label的尺寸
	QObject::connect(&scrollTimer, &QTimer::timeout, [&](){
		updateVisibleImages(false);
	});
	QObject::connect(scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, [&](){
		scrollTimer.start();
	});
	
	// 6. 优化 窗口拖动边缘会连续疯狂触发 resize 信号，每次都全部重绘图片，大图会卡顿，加 100ms 防抖定时器：
	QTimer resizeTimer;
	resizeTimer.setSingleShot(true);
	resizeTimer.setInterval(100);
	// 计时器时间到，再触发强制刷新label的尺寸
	QObject::connect(&resizeTimer, &QTimer::timeout, [&](){
		updateVisibleImages(true);
	});
	// 监听窗口状态：全屏/最大化/还原都会触发
	// 窗口创建前，windowHandle()返回null，需要在窗口渲染结束后接受windowHandle()
	QMetaObject::invokeMethod(&mainWin, [&]() {
		QWindow* winHandle = mainWin.windowHandle();
		if (winHandle) {
			QObject::connect(winHandle, &QWindow::windowStateChanged, [&](Qt::WindowState state) {
				qDebug() << "windowStateChanged";
				resizeTimer.start();
			});
		} else {
			qDebug() << "windowHandle is still null!";
		}
	}, Qt::QueuedConnection);
	// 额外监听分割器拖动（左右拉分割线也刷新）
	QObject::connect(splitter, &QSplitter::splitterMoved, [&](){
		qDebug() << "splitterMoved";
		resizeTimer.start();
	});

	// 7. medialabel被点击时，由ImageGridWidget向上发送信号请求展示图片元数据，这里接受带有参数media_elem& media的信号，并作处理
	// 点击一个medialabel，则在textInfo展示媒体信息
	QObject::connect(gridWidget, &ImageGridWidget::show_clicked_media_info,[&](const media_elem& media){
		// 首先判断数据库是否连接
		if (!sqlite.open()) {
			qDebug() << "sqlite not open!";
			return;
		}
		
		// 获取图片元数据
		int media_id = media.media_id;
		QString file_path = QString::fromStdString(media.file_path);
		QString name = QString::fromStdString(media.name);
		QString source_folder = QString::fromStdString(media.source_folder);
		long long  mtime = media.mtime;
		QString thumbnail_path = QString::fromStdString(media.thumbnail_path);
		
		QString mDate;
		
		// 使用 timestampToLocalDateTime 将时间戳mtime转换为本地 年月日时分秒
		int year, month, day, hour, minute, second;
		bool isOk = timestampToLocalDateTime(mtime, year, month, day, hour, minute, second);
		if(isOk){
			// （可选）直接利用 Qt 将 UTC 时间戳转为本地时间显示（自动处理时区和跨月）
			// QDateTime localTime = QDateTime::fromSecsSinceEpoch(mtime).toLocalTime();
			// mDate = localTime.toString("yyyy年MM月dd日 HH时mm分ss秒");
			
			mDate = QString(R"(%1年%2月%3日 %4时%5分%6秒)").arg(year).arg(month).arg(day).arg(hour).arg(minute).arg(second);
		}
		else{
			mDate = QString(R"(时间戳转换失败，原时间戳:%1)").arg(mtime);
		}

		// 根据该图片的media_id查找绑定的tag
		QString tags;
		vector<tag_elem> tagVector;
		bool res = sqlite.get_tags_by_media(tagVector, media_id);
		if(res){
			if(tagVector.size() == 0){
				tags = QString("该文件没有绑定标签");
			}
			else{
				for(tag_elem tag : tagVector){
				tags += QString::fromStdString(tag.tag_name + " ");
				}
			}
		}
		else{
			tags = QString(R"(查询标签失败： %1)").arg(QString::fromStdString(sqlite.getError()));
		}
		
		// 拼接图片详细信息
		QString newInfo = QString(R"(图片信息
media_id：%1

文件路径：%2

文件名称：%3

源目录：%4

文件修改时间：%5

文件绑定的标签：%6

备注：可以在这里展示sqlite查询出来的信息
)").arg(media_id).arg(file_path).arg(name).arg(source_folder).arg(mDate).arg(tags);
		textInfo->setText(newInfo);
	});
	
	mainWin.show();
	
	qDebug() << "Starting app.exec()...";
	int ret = -1;
	try {
		ret = app.exec();
	} catch (const std::exception& e) {
		qDebug() << "Exception in app.exec(): " << e.what();
	} catch (...) {
		qDebug() << "Unknown exception in app.exec()";
	}
	qDebug() << "app.exec() ended with:" << ret;
	// app.exec() 启动 Qt 事件循环，窗口关闭之后才会执行后面代码。
	// 主窗口循环结束后，关闭数据库连接
	sqlite.close();
	return ret;
}