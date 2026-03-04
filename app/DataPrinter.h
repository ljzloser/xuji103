#ifndef XUJI103_DATAPRINTER_H
#define XUJI103_DATAPRINTER_H

#include "iec103/callback/IDataHandler.h"
#include <QObject>

class DataPrinter : public QObject, public IEC103::IDataHandler {
    Q_OBJECT

public:
    explicit DataPrinter(QObject* parent = nullptr);
    
    // IDataHandler 接口实现
    void onConnected() override;
    void onDisconnected() override;
    void onError(const QString& error) override;
    void onLinkStateChanged(IEC103::LinkState state) override;
    
    void onDoublePoint(const IEC103::DigitalPoint& point) override;
    void onAnalogValue(const IEC103::AnalogPoint& point) override;
    void onCounterValue(const IEC103::CounterPoint& point) override;
    void onGenericData(uint16_t deviceAddr, const IEC103::GenericDataItem& item) override;
    
    void onGIStarted(uint16_t deviceAddr) override;
    void onGICompleted(uint16_t deviceAddr) override;

private:
    QString formatQuality(const IEC103::Quality& quality);
    QString formatTimestamp(const IEC103::CP56Time2a& ts);
};

#endif // XUJI103_DATAPRINTER_H
