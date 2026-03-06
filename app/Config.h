#ifndef XUJI103_CONFIG_H
#define XUJI103_CONFIG_H

#include <QString>
#include <QList>

// 组配置结构
struct GroupConfig {
    int group = 0;          // 组号
    QString name;           // 组名称
    // 注：数据类型由响应报文 GDD.DataType 动态确定，无需预先配置
};

class Config {
public:
    static Config& instance();
    
    // 从JSON配置文件加载
    bool loadFromFile(const QString& filePath);
    bool loadFromJson(const QString& jsonContent);
    
    // 检查配置是否有效
    bool isValid() const { return m_valid; }
    
    // 连接参数
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    int timeout() const { return m_timeout; }
    int reconnectInterval() const { return m_reconnectInterval; }
    int testInterval() const { return m_testInterval; }
    bool autoReconnect() const { return m_autoReconnect; }
    
    // 流量控制参数
    int maxUnconfirmed() const { return m_maxUnconfirmed; }   // k值
    int maxAckDelay() const { return m_maxAckDelay; }         // w值
    
    // 总召唤参数
    int giInterval() const { return m_giInterval; }
    
    // 通用服务组配置 (遥测/遥脉统一)
    QList<GroupConfig> groups() const { return m_groups; }
    int groupInterval() const { return m_groupInterval; }
    
    // 日志参数
    int logLevel() const { return m_logLevel; }
    QString logFile() const { return m_logFile; }
    
    // 配置文件路径
    QString configFilePath() const { return m_configFilePath; }
    
    // 默认配置文件路径
    static QString defaultConfigPath();
    
private:
    Config() = default;
    
    bool m_valid = false;       // 配置是否有效
    QString m_host;
    quint16 m_port = 0;
    int m_timeout = 0;
    int m_reconnectInterval = 0;
    int m_testInterval = 0;
    bool m_autoReconnect = false;
    int m_maxUnconfirmed = 12;  // k值，默认12
    int m_maxAckDelay = 8;      // w值，默认8
    int m_giInterval = 0;
    
    // 通用服务组配置
    QList<GroupConfig> m_groups;
    int m_groupInterval = 0;
    
    int m_logLevel = 1;         // 0=Debug, 1=Info, 2=Warning, 3=Error
    QString m_logFile;
    QString m_configFilePath;
};

#endif // XUJI103_CONFIG_H
