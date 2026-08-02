#include "StartupDialog.h"
#include <QVBoxLayout>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include <QIntValidator>
#include <QFileDialog>
#include <QHBoxLayout>

StartupDialog::StartupDialog(QWidget* parent)
    : QDialog(parent), m_isNetwork(false)
{
    setWindowTitle("选择图片加载模式");
    QVBoxLayout* layout = new QVBoxLayout(this);

    QRadioButton* rbLocal = new QRadioButton("本地模式");
    QRadioButton* rbNetwork = new QRadioButton("网络模式");
    rbLocal->setChecked(true);

    // 网络设置控件
    QWidget* netWidget = new QWidget;
    QHBoxLayout* netLayout = new QHBoxLayout(netWidget);
    QLabel* ipLabel = new QLabel("服务器IP:");
    m_ipEdit = new QLineEdit("127.0.0.1");
    QLabel* portLabel = new QLabel("端口:");
    m_portEdit = new QLineEdit("8080");
    m_portEdit->setValidator(new QIntValidator(1, 65535, this));
    netLayout->addWidget(ipLabel);
    netLayout->addWidget(m_ipEdit);
    netLayout->addWidget(portLabel);
    netLayout->addWidget(m_portEdit);
    netWidget->setVisible(false);

    layout->addWidget(rbLocal);
    layout->addWidget(rbNetwork);
    layout->addWidget(netWidget);
	
	// Data Root 路径输入（仅在本地模式显示）放在网络设置下方
	QWidget* dataRootWidget = new QWidget;
	QHBoxLayout* dataRootLayout = new QHBoxLayout(dataRootWidget);
	QLabel* dataRootLabel = new QLabel("Data Root:");
	m_dataRootEdit = new QLineEdit("D:/Z_project/");
	QPushButton* browseDataRootBtn = new QPushButton("浏览...");
	dataRootLayout->addWidget(dataRootLabel);
	dataRootLayout->addWidget(m_dataRootEdit);
	dataRootLayout->addWidget(browseDataRootBtn);
	layout->addWidget(dataRootWidget);

    // 数据库路径选择（始终显示，放在底部）
    QWidget* dbWidget = new QWidget;
    QHBoxLayout* dbLayout = new QHBoxLayout(dbWidget);
    QLabel* dbLabel = new QLabel("数据库路径:");
    m_dbPathEdit = new QLineEdit("media.db");
    QPushButton* browseDBPathBtn = new QPushButton("浏览...");
    dbLayout->addWidget(dbLabel);
    dbLayout->addWidget(m_dbPathEdit);
    dbLayout->addWidget(browseDBPathBtn);
    layout->addWidget(dbWidget);

    QPushButton* okBtn = new QPushButton("确定");
    layout->addWidget(okBtn);

    // 网络模式切换：显示/隐藏 netWidget 和 dataRootWidget
    connect(rbNetwork, &QRadioButton::toggled, [netWidget, dataRootWidget](bool checked) {
        netWidget->setVisible(checked);          // 网络模式显示网络设置
        dataRootWidget->setVisible(!checked);    // 本地模式显示 Data Root
    });
	
	// 连接browseDataRootBtn按钮，弹出目录选择对话框，选择根目录
	connect(browseDataRootBtn, &QPushButton::clicked, [this]() {
		// 使用当前编辑框中的路径作为起始目录
		QString startDir = m_dataRootEdit->text();
		QString dir = QFileDialog::getExistingDirectory(this, "选择 Data Root 目录", startDir);
		if (!dir.isEmpty()) {
			// 转换为正斜杠（Qt 内部使用 /）
			// 转换系统路径分隔符为 URL 风格，且 Qt 官方保证兼容 Unix 和 Windows 的边界情况（如网络路径 \\server\share）。
			QString normalized = QDir::fromNativeSeparators(dir);
			// 确保以 / 结尾
			if (!normalized.endsWith('/')) normalized += '/';
			m_dataRootEdit->setText(normalized);
		}
	});

	// 连接browseBtn按钮
    connect(browseDBPathBtn, &QPushButton::clicked, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "选择数据库文件", ".", "SQLite Database (*.db);;All Files (*)");
        if (!filePath.isEmpty()) {
            m_dbPathEdit->setText(filePath);
        }
    });

    connect(okBtn, &QPushButton::clicked, [this, rbNetwork]() {
        m_isNetwork = rbNetwork->isChecked();
        accept();
    });

    setFixedSize(450, 350);
}

bool StartupDialog::isNetworkMode() const
{
    return m_isNetwork;
}

QString StartupDialog::serverHost() const
{
    return m_ipEdit->text();
}

int StartupDialog::serverPort() const
{
    return m_portEdit->text().toInt();
}

QString StartupDialog::dbPath() const
{
    return m_dbPathEdit->text();
}

QString StartupDialog::dataRoot() const
{
    return m_dataRootEdit->text();
}