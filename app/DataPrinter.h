#ifndef XUJI103_DATAPRINTER_H
#define XUJI103_DATAPRINTER_H

#include "iec103/callback/IDataHandler.h"
#include <QObject>
#include <QFile>
#include <QTextStream>

class DataPrinter : public QObject, public IEC103::IDataHandler {
    Q_OBJECT

public:
    explicit DataPrinter(QObject* parent = nullptr);
    ~DataPrinter();
    
    // 设置日志文件路径
    void setLogFile(const QString& path);
    
    // IDataHandler 接口实现
    void onRawFrame(const QByteArray& data, bool direction) override;
    void onConnected() override;
    void onDisconnected() override;
    void onError(const QString& error) override;
    void onLinkStateChanged(IEC103::LinkState state) override;
    
    // 遥信回调
    void onDoublePoint(const IEC103::DigitalPoint& point) override;
    
    // 通用服务回调 (遥测/遥脉统一)
    void onGenericValue(const IEC103::GenericPoint& point) override;
    void onGenericData(uint16_t asduAddr, const IEC103::GenericDataItem& item) override;
    
    // 总召唤回调
    void onGIStarted(uint8_t deviceAddr) override;
    void onGICompleted(uint8_t deviceAddr) override;

private:
    QString formatQuality(const IEC103::Quality& quality);
    QString formatTimestamp(const IEC103::CP56Time2a& ts);
    void writeLog(const QString& line);
    
    QFile m_logFile;
    QTextStream m_logStream;
};

#endif // XUJI103_DATAPRINTER_H