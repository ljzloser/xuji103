#include "MsgParserWindow.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QComboBox>
#include <QLabel>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QPalette>

MsgParserWindow::MsgParserWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_currentFrame(nullptr)
{
    setupUI();
    setWindowTitle("南网以太网103协议报文解析器");
    resize(1400, 800);
    setAcceptDrops(true);  // 启用拖放
}

MsgParserWindow::~MsgParserWindow() {
}

void MsgParserWindow::setupUI() {
    // 菜单栏
    QMenuBar* menuBar = this->menuBar();
    QMenu* fileMenu = menuBar->addMenu("文件(&F)");
    QAction* openAction = fileMenu->addAction("打开(&O)...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MsgParserWindow::openFile);

    QAction* exitAction = fileMenu->addAction("退出(&X)");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    // 工具栏
    QToolBar* toolbar = addToolBar("工具");
    toolbar->addAction(openAction);

    // 过滤器
    QWidget* filterWidget = new QWidget;
    QHBoxLayout* filterLayout = new QHBoxLayout(filterWidget);
    filterLayout->setContentsMargins(0, 0, 0, 0);

    // 方向筛选
    filterLayout->addWidget(new QLabel("方向:"));
    m_dirFilter = new QComboBox;
    m_dirFilter->addItem("全部", "all");
    m_dirFilter->addItem("接收 ←", "recv");
    m_dirFilter->addItem("发送 →", "send");
    connect(m_dirFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MsgParserWindow::filterChanged);
    filterLayout->addWidget(m_dirFilter);

    filterLayout->addSpacing(10);

    // 帧类型筛选
    filterLayout->addWidget(new QLabel("帧类型:"));
    m_typeFilter = new QComboBox;
    m_typeFilter->addItem("全部", -1);
    m_typeFilter->addItem("I帧", 0);
    m_typeFilter->addItem("S帧", 1);
    m_typeFilter->addItem("U帧", 3);
    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MsgParserWindow::filterChanged);
    filterLayout->addWidget(m_typeFilter);

    filterLayout->addSpacing(10);

    // ASDU类型筛选
    filterLayout->addWidget(new QLabel("ASDU:"));
    m_asduFilter = new QComboBox;
    m_asduFilter->addItem("全部", 0);
    m_asduFilter->addItem("解析失败", -2);  // 特殊值表示解析失败
    m_asduFilter->addItem("ASDU1 遥信突发", 1);
    m_asduFilter->addItem("ASDU2 保护事件", 2);
    m_asduFilter->addItem("ASDU7 总召唤命令", 7);
    m_asduFilter->addItem("ASDU8 总召唤结束", 8);
    m_asduFilter->addItem("ASDU10 通用数据", 10);
    m_asduFilter->addItem("ASDU11 通用标识", 11);
    m_asduFilter->addItem("ASDU21 通用读命令", 0x15);
    m_asduFilter->addItem("ASDU42 双点遥信", 0x2A);
    connect(m_asduFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MsgParserWindow::filterChanged);
    filterLayout->addWidget(m_asduFilter);

    toolbar->addSeparator();
    toolbar->addWidget(filterWidget);

    // 主分割器
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);

    // 左侧 - 帧列表
    QWidget* leftWidget = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(5, 5, 5, 5);

    QLabel* listLabel = new QLabel("帧列表");
    listLabel->setStyleSheet("font-weight: bold;");
    leftLayout->addWidget(listLabel);

    m_frameList = new QTreeWidget;
    m_frameList->setHeaderLabels({"序号", "方向", "类型", "ASDU", "时间"});
    m_frameList->setRootIsDecorated(false);
    m_frameList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_frameList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_frameList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_frameList->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_frameList->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_frameList->header()->setStretchLastSection(true);
    connect(m_frameList, &QTreeWidget::itemSelectionChanged, this, &MsgParserWindow::onFrameSelected);
    leftLayout->addWidget(m_frameList);

    mainSplitter->addWidget(leftWidget);

    // 右侧 - 详情
    QWidget* rightWidget = new QWidget;
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(5, 5, 5, 5);

    // 原始数据
    QLabel* rawLabel = new QLabel("原始数据 (点击字段高亮对应字节)");
    rawLabel->setStyleSheet("font-weight: bold;");
    rightLayout->addWidget(rawLabel);

    m_rawView = new QTextEdit;
    m_rawView->setReadOnly(true);
    m_rawView->setFont(QFont("Consolas", 10));
    m_rawView->setMaximumHeight(150);
    rightLayout->addWidget(m_rawView);

    // 字段树
    QLabel* fieldLabel = new QLabel("报文结构 (点击字段查看对应字节)");
    fieldLabel->setStyleSheet("font-weight: bold;");
    rightLayout->addWidget(fieldLabel);

    m_fieldTree = new QTreeWidget;
    m_fieldTree->setHeaderLabels({"字段名称", "值", "字节位置"});
    m_fieldTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_fieldTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_fieldTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_fieldTree->setColumnWidth(0, 200);
    connect(m_fieldTree, &QTreeWidget::itemSelectionChanged, this, &MsgParserWindow::onFieldSelected);
    rightLayout->addWidget(m_fieldTree);

    mainSplitter->addWidget(rightWidget);

    // 设置分割比例
    mainSplitter->setSizes({350, 1050});

    setCentralWidget(mainSplitter);

    // 状态栏
    m_statusLabel = new QLabel("就绪");
    statusBar()->addWidget(m_statusLabel);
}

void MsgParserWindow::openFile() {
    QString filepath = QFileDialog::getOpenFileName(this, 
        "打开报文文件", 
        QString(), 
        "文本文件 (*.txt);;所有文件 (*.*)");

    if (!filepath.isEmpty()) {
        loadFile(filepath);
    }
}

void MsgParserWindow::loadFile(const QString& filepath) {
    m_frames = FrameParser().parseLogFile(filepath);
    m_currentFrame = nullptr;

    refreshFrameList();

    m_statusLabel->setText(QString("已加载 %1 个帧").arg(m_frames.size()));

    // 选中第一个
    if (m_frameList->topLevelItemCount() > 0) {
        m_frameList->setCurrentItem(m_frameList->topLevelItem(0));
    }
}

void MsgParserWindow::refreshFrameList() {
    m_frameList->clear();

    int idx = 0;
    int filteredCount = 0;

    for (const FrameInfo& frame : m_frames) {
        if (!matchFilter(frame)) {
            continue;
        }

        QTreeWidgetItem* item = new QTreeWidgetItem(m_frameList);
        item->setText(0, QString::number(idx + 1));
        item->setText(1, frame.direction == "recv" ? "←" : "→");
        item->setText(2, frame.typeName());

        // 获取ASDU类型名称
        QString asduName;
        if (frame.failed) {
            asduName = frame.error;
        } else {
            for (const FieldInfo& f : frame.fields) {
                if (f.name == "ASDU") {
                    for (const FieldInfo& c : f.children) {
                        if (c.name.startsWith("类型标识TI")) {
                            asduName = c.value;
                            // 提取括号内容
                            int start = asduName.indexOf('(');
                            int end = asduName.indexOf(')');
                            if (start >= 0 && end > start) {
                                asduName = asduName.mid(start + 1, end - start - 1);
                            }
                            break;
                        }
                    }
                    break;
                }
            }
        }
        item->setText(3, asduName);
        item->setText(4, frame.timestamp);

        // 设置背景色
        if (frame.failed) {
            // 解析失败 - 整行红色背景
            for (int col = 0; col < 5; ++col) {
                item->setBackground(col, QColor(255, 200, 200));
            }
        } else if (frame.direction == "recv") {
            item->setBackground(1, QColor(220, 255, 220));  // 浅绿
        } else {
            item->setBackground(1, QColor(255, 220, 220));  // 浅红
        }

        idx++;
        filteredCount++;
    }

    m_statusLabel->setText(QString("显示 %1 / %2 个帧").arg(filteredCount).arg(m_frames.size()));
}

bool MsgParserWindow::matchFilter(const FrameInfo& frame) {
    // ASDU类型筛选 - 特殊处理解析失败
    int asduFilter = m_asduFilter->currentData().toInt();
    if (asduFilter == -2) {
        // 只显示解析失败的帧
        return frame.failed;
    }

    // 如果帧解析失败，只在"全部"或"解析失败"时显示
    if (frame.failed) {
        return asduFilter == 0;  // 全部时显示失败帧
    }

    // 方向筛选
    QString dirFilter = m_dirFilter->currentData().toString();
    if (dirFilter != "all" && frame.direction != dirFilter) {
        return false;
    }

    // 帧类型筛选
    int typeFilter = m_typeFilter->currentData().toInt();
    if (typeFilter >= 0 && frame.frameType != typeFilter) {
        return false;
    }

    // ASDU类型筛选
    if (asduFilter > 0) {
        // 只有I帧有ASDU
        if (frame.frameType != 0) {
            return false;
        }
        // 从字段中查找ASDU类型
        bool found = false;
        for (const FieldInfo& f : frame.fields) {
            if (f.name == "ASDU") {
                for (const FieldInfo& c : f.children) {
                    if (c.name.startsWith("类型标识TI")) {
                        // 解析TI值
                        QString tiStr = c.value.split(' ').first();
                        bool ok;
                        int ti = tiStr.toInt(&ok, 16);
                        if (ok && ti == asduFilter) {
                            found = true;
                        }
                        break;
                    }
                }
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

void MsgParserWindow::onFrameSelected() {
    QList<QTreeWidgetItem*> selected = m_frameList->selectedItems();
    if (selected.isEmpty()) return;

    int idx = selected.first()->text(0).toInt() - 1;

    // 根据过滤计算实际索引
    int actualIdx = 0;
    int filteredIdx = 0;

    for (int i = 0; i < m_frames.size(); ++i) {
        if (!matchFilter(m_frames[i])) {
            continue;
        }
        if (filteredIdx == idx) {
            actualIdx = i;
            break;
        }
        filteredIdx++;
    }

    if (actualIdx >= 0 && actualIdx < m_frames.size()) {
        m_currentFrame = &m_frames[actualIdx];
        showFrameDetail(*m_currentFrame);
    }
}

void MsgParserWindow::showFrameDetail(const FrameInfo& frame) {
    // 清空
    m_rawView->clear();
    m_fieldTree->clear();

    // 显示原始数据
    QString hexStr;
    for (int i = 0; i < frame.raw.size(); ++i) {
        if (i > 0 && i % 16 == 0) {
            hexStr += "\n";
        }
        hexStr += QString("%1 ").arg((uint8_t)frame.raw[i], 2, 16, QChar('0')).toUpper();
    }
    m_rawView->setPlainText(hexStr.trimmed());

    // 显示字段树
    QTreeWidgetItem* root = new QTreeWidgetItem(m_fieldTree);
    
    if (frame.failed) {
        // 解析失败的帧
        root->setText(0, QString("[%1] 解析失败").arg(frame.timestamp));
        root->setBackground(0, QColor(255, 200, 200));
        
        QTreeWidgetItem* errorItem = new QTreeWidgetItem(root);
        errorItem->setText(0, "错误信息");
        errorItem->setText(1, frame.error);
        errorItem->setBackground(1, QColor(255, 200, 200));
        
        if (!frame.rawLine.isEmpty()) {
            QTreeWidgetItem* lineItem = new QTreeWidgetItem(root);
            lineItem->setText(0, "原始日志行");
            lineItem->setText(1, frame.rawLine);
        }
    } else {
        QString direction = frame.direction == "recv" ? "← 接收" : "→ 发送";
        root->setText(0, QString("[%1] %2").arg(frame.timestamp).arg(direction));
    }
    root->setExpanded(true);

    for (const FieldInfo& field : frame.fields) {
        addFieldToTree(root, field);
    }

    m_fieldTree->expandAll();
}

void MsgParserWindow::addFieldToTree(QTreeWidgetItem* parent, const FieldInfo& field) {
    QTreeWidgetItem* item = new QTreeWidgetItem(parent);
    item->setText(0, field.name);
    item->setText(1, field.value);
    if (field.end > field.start) {
        item->setText(2, QString("[%1:%2]").arg(field.start).arg(field.end));
    }

    // 存储字节位置用于高亮
    item->setData(0, Qt::UserRole, field.start);
    item->setData(0, Qt::UserRole + 1, field.end);

    for (const FieldInfo& child : field.children) {
        addFieldToTree(item, child);
    }
}

void MsgParserWindow::onFieldSelected() {
    QList<QTreeWidgetItem*> selected = m_fieldTree->selectedItems();
    if (selected.isEmpty()) return;

    QTreeWidgetItem* item = selected.first();
    int start = item->data(0, Qt::UserRole).toInt();
    int end = item->data(0, Qt::UserRole + 1).toInt();

    if (end > start) {
        highlightBytes(start, end);
    }
}

void MsgParserWindow::highlightBytes(int start, int end) {
    QString text = m_rawView->toPlainText();

    // 计算位置
    // 每个字节占3个字符(2位hex+空格)，每16字节换行
    auto getPosition = [](int byteIdx) -> int {
        int line = byteIdx / 16;
        int col = (byteIdx % 16) * 3;
        // 每行有 16*3 = 48 个字符 + 1 个换行符
        return line * 49 + col;
    };

    int startPos = getPosition(start);
    // endPos指向end字节的起始位置，需要减1去掉最后一个字节后面的空格
    // 每个字节占3字符(XX空格)，所以end字节起始位置-1就是最后一个字节的空格
    int endPos = getPosition(end) - 1;

    // 边界检查
    if (startPos >= text.length() || endPos > text.length()) {
        return;
    }

    // 设置高亮调色板，确保无焦点时也显示黄色
    QPalette pal = m_rawView->palette();
    pal.setColor(QPalette::Highlight, QColor(255, 255, 0));      // 黄色背景
    pal.setColor(QPalette::HighlightedText, QColor(0, 0, 0));    // 黑色文字
    m_rawView->setPalette(pal);

    // 使用ExtraSelection实现只读文本的高亮
    QTextEdit::ExtraSelection selection;
    QTextCursor cursor = m_rawView->textCursor();
    cursor.setPosition(startPos);
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);

    selection.cursor = cursor;
    selection.format.setBackground(QColor(255, 255, 0));  // 黄色背景
    selection.format.setForeground(QColor(0, 0, 0));      // 黑色文字
    selection.format.setProperty(QTextFormat::FullWidthSelection, false);

    QList<QTextEdit::ExtraSelection> extraSelections;
    extraSelections.append(selection);
    m_rawView->setExtraSelections(extraSelections);

    // 滚动到可见
    m_rawView->setTextCursor(cursor);
    m_rawView->ensureCursorVisible();
}

void MsgParserWindow::filterChanged(int index) {
    Q_UNUSED(index);
    refreshFrameList();
    m_rawView->clear();
    m_fieldTree->clear();
    m_currentFrame = nullptr;
}

void MsgParserWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MsgParserWindow::dropEvent(QDropEvent* event) {
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString filepath = urls.first().toLocalFile();
        if (!filepath.isEmpty()) {
            loadFile(filepath);
        }
    }
}
