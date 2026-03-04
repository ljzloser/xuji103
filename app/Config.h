#ifndef XUJI103_CONFIG_H
#define XUJI103_CONFIG_H

#include <QString>
#include <QStringList>
#include <QList>

class Config {
public:
    static Config& instance();
    
    bool parseArgs(const QStringList& args);
    void printHelp();
    
    // 连接参数
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    int timeout() const { return m_timeout; }
    int reconnectInterval() const { return m_reconnectInterval; }
    int testInterval() const { return m_testInterval; }
    bool autoReconnect() const { return m_autoReconnect; }
    
    // 总召唤参数
    int giInterval() const { return m_giInterval; }
    
    // 遥测召唤参数
    QList<int> analogGroups() const { return m_analogGroups; }
    int analogInterval() const { return m_analogInterval; }
    
    // 遥脉召唤参数
    QList<int> counterGroups() const { return m_counterGroups; }
    int counterInterval() const { return m_counterInterval; }
    
    // 日志参数
    int logLevel() const { return m_logLevel; }
    
private:
    Config() = default;
    
    QString m_host = "127.0.0.1";
    quint16 m_port = 2404;
    int m_timeout = 15000;
    int m_reconnectInterval = 5000;
    int m_testInterval = 20000;
    bool m_autoReconnect = true;
    int m_giInterval = 60;      // 秒
    
    // 遥测/遥脉配置
    QList<int> m_analogGroups = {8, 9, 10};     // 默认遥测组
    QList<int> m_counterGroups = {16, 17};      // 默认遥脉组
    int m_analogInterval = 5;   // 遥测召唤间隔(秒)
    int m_counterInterval = 60; // 遥脉召唤间隔(秒)
    
    int m_logLevel = 1;         // 0=Debug, 1=Info, 2=Warning, 3=Error
};

#endif // XUJI103_CONFIG_H
