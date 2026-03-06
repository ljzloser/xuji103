#include "Config.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

Config& Config::instance()
{
    static Config instance;
    return instance;
}

bool Config::parseArgs(const QStringList& args)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("南网以太网103协议主站客户端");
    parser.addHelpOption();
    parser.addVersionOption();
    
    // 连接选项
    QCommandLineOption hostOption(
        QStringList() << "H" << "host",
        "服务器地址 (默认: 127.0.0.1)",
        "host",
        "127.0.0.1"
    );
    parser.addOption(hostOption);
    
    QCommandLineOption portOption(
        QStringList() << "p" << "port",
        "服务器端口 (默认: 2404)",
        "port",
        "2404"
    );
    parser.addOption(portOption);
    
    QCommandLineOption timeoutOption(
        QStringList() << "t" << "timeout",
        "连接超时(ms) (默认: 15000)",
        "ms",
        "15000"
    );
    parser.addOption(timeoutOption);
    
    QCommandLineOption reconnectOption(
        QStringList() << "r" << "reconnect",
        "重连间隔(ms) (默认: 5000, 0=禁用)",
        "ms",
        "5000"
    );
    parser.addOption(reconnectOption);
    
    QCommandLineOption testOption(
        QStringList() << "T" << "test-interval",
        "测试帧间隔(ms) (默认: 20000)",
        "ms",
        "20000"
    );
    parser.addOption(testOption);
    
    // 总召唤选项
    QCommandLineOption giOption(
        QStringList() << "g" << "gi-interval",
        "总召唤间隔(秒) (默认: 60, 0=只执行一次)",
        "seconds",
        "60"
    );
    parser.addOption(giOption);
    
    // 遥测/遥脉组选项
    QCommandLineOption analogOption(
        QStringList() << "a" << "analog-groups",
        "遥测组列表 (默认: 8,9,10)",
        "groups",
        "8,9,10"
    );
    parser.addOption(analogOption);
    
    QCommandLineOption counterOption(
        QStringList() << "c" << "counter-groups",
        "遥脉组列表 (默认: 16,17)",
        "groups",
        "16,17"
    );
    parser.addOption(counterOption);
    
    // 日志选项
    QCommandLineOption logOption(
        QStringList() << "l" << "log-level",
        "日志级别 (0=Debug, 1=Info, 2=Warning, 3=Error, 默认: 1)",
        "level",
        "1"
    );
    parser.addOption(logOption);
    
    // 解析命令行
    if (!parser.parse(args)) {
        qCritical() << parser.errorText();
        return false;
    }
    
    if (parser.isSet("help")) {
        parser.showHelp();
        return false;
    }
    
    if (parser.isSet("version")) {
        parser.showVersion();
        return false;
    }
    
    // 应用配置
    m_host = parser.value(hostOption);
    m_port = parser.value(portOption).toUShort();
    m_timeout = parser.value(timeoutOption).toInt();
    m_reconnectInterval = parser.value(reconnectOption).toInt();
    m_autoReconnect = (m_reconnectInterval > 0);
    m_testInterval = parser.value(testOption).toInt();
    m_giInterval = parser.value(giOption).toInt();
    m_logLevel = parser.value(logOption).toInt();
    
    // 解析遥测组
    QString analogStr = parser.value(analogOption);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList analogList = analogStr.split(",", Qt::SkipEmptyParts);
#else
    QStringList analogList = analogStr.split(",", QString::SkipEmptyParts);
#endif
    m_analogGroups.clear();
    for (const QString& s : analogList) {
        m_analogGroups.append(s.trimmed().toInt());
    }
    
    // 解析遥脉组
    QString counterStr = parser.value(counterOption);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList counterList = counterStr.split(",", Qt::SkipEmptyParts);
#else
    QStringList counterList = counterStr.split(",", QString::SkipEmptyParts);
#endif
    m_counterGroups.clear();
    for (const QString& s : counterList) {
        m_counterGroups.append(s.trimmed().toInt());
    }
    
    return true;
}

void Config::printHelp()
{
    qInfo() << "南网以太网103协议主站客户端";
    qInfo() << "用法: xuji103 [选项]";
    qInfo() << "";
    qInfo() << "选项:";
    qInfo() << "  -H, --host <host>         服务器地址 (默认: 127.0.0.1)";
    qInfo() << "  -p, --port <port>         服务器端口 (默认: 2404)";
    qInfo() << "  -t, --timeout <ms>        连接超时 (默认: 15000)";
    qInfo() << "  -r, --reconnect <ms>      重连间隔 (默认: 5000, 0=禁用)";
    qInfo() << "  -T, --test-interval <ms>  测试帧间隔 (默认: 20000)";
    qInfo() << "  -g, --gi-interval <sec>   总召唤间隔 (默认: 60, 0=只执行一次)";
    qInfo() << "  -l, --log-level <level>   日志级别 (0=Debug, 1=Info, 2=Warning, 3=Error)";
    qInfo() << "  -h, --help                显示帮助信息";
    qInfo() << "  -v, --version             显示版本信息";
}
