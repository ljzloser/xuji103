#include "Config.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>

Config& Config::instance()
{
    static Config instance;
    return instance;
}

QString Config::defaultConfigPath()
{
    return QString("config.json");
}

bool Config::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "无法打开配置文件:" << filePath << file.errorString();
        return false;
    }
    
    QByteArray content = file.readAll();
    file.close();
    
    m_configFilePath = filePath;
    return loadFromJson(QString::fromUtf8(content));
}

bool Config::loadFromJson(const QString& jsonContent)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonContent.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qCritical() << "JSON解析错误:" << error.errorString();
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // 解析连接配置 (必填)
    if (!root.contains("connection")) {
        qCritical() << "配置文件缺少 connection 节点";
        return false;
    }
    
    QJsonObject conn = root["connection"].toObject();
    m_host = conn.value("host").toString();
    if (m_host.isEmpty()) {
        qCritical() << "配置文件缺少 connection.host";
        return false;
    }
    m_port = static_cast<quint16>(conn.value("port").toInt(0));
    if (m_port == 0) {
        qCritical() << "配置文件缺少 connection.port";
        return false;
    }
    m_timeout = conn.value("timeout").toInt(15000);
    m_reconnectInterval = conn.value("reconnect_interval").toInt(5000);
    m_autoReconnect = conn.value("auto_reconnect").toBool(true);
    m_testInterval = conn.value("test_interval").toInt(20000);
    m_maxUnconfirmed = conn.value("max_unconfirmed").toInt(12);
    m_maxAckDelay = conn.value("max_ack_delay").toInt(8);
    
    // 解析轮询配置 (必填)
    if (!root.contains("polling")) {
        qCritical() << "配置文件缺少 polling 节点";
        return false;
    }
    
    QJsonObject polling = root["polling"].toObject();
    m_giInterval = polling.value("gi_interval").toInt(0);
    if (m_giInterval <= 0) {
        qCritical() << "配置文件缺少或无效 polling.gi_interval";
        return false;
    }
    m_groupInterval = polling.value("group_interval").toInt(5);
    
    // 解析组配置 (必填)
    if (!polling.contains("groups") || polling["groups"].toArray().isEmpty()) {
        qCritical() << "配置文件缺少或空的 polling.groups";
        return false;
    }
    
    QJsonArray groupsArray = polling["groups"].toArray();
    m_groups.clear();
    
    for (const QJsonValue& val : groupsArray) {
        QJsonObject groupObj = val.toObject();
        GroupConfig config;
        config.group = groupObj.value("group").toInt(0);
        if (config.group == 0) {
            qCritical() << "组配置缺少有效的 group 值";
            return false;
        }
        config.name = groupObj.value("name").toString();
        m_groups.append(config);
    }
    
    // 解析日志配置 (可选)
    if (root.contains("logging")) {
        QJsonObject logging = root["logging"].toObject();
        QString levelStr = logging.value("level").toString("info").toLower();
        if (levelStr == "debug") m_logLevel = 0;
        else if (levelStr == "info") m_logLevel = 1;
        else if (levelStr == "warning") m_logLevel = 2;
        else if (levelStr == "error") m_logLevel = 3;
        else m_logLevel = 1;
        
        m_logFile = logging.value("file").toString();
    }
    
    m_valid = true;
    
    qInfo() << "配置加载成功:";
    qInfo() << "  主机:" << m_host << ":" << m_port;
    qInfo() << "  总召唤间隔:" << m_giInterval << "秒";
    qInfo() << "  组数量:" << m_groups.size();
    for (const auto& g : m_groups) {
        qInfo() << "    组" << g.group << ":" << g.name;
    }
    
    return true;
}
