#include "DataPrinter.h"
#include <QDebug>
#include <QDateTime>

namespace {
    QString ts() {
        return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    }
}

DataPrinter::DataPrinter(QObject* parent)
    : QObject(parent)
{
}

void DataPrinter::onConnected()
{
    qInfo().noquote() << QString("[%1] [EVENT] Connected").arg(ts());
}

void DataPrinter::onDisconnected()
{
    qInfo().noquote() << QString("[%1] [EVENT] Disconnected").arg(ts());
}

void DataPrinter::onError(const QString& error)
{
    qWarning().noquote() << QString("[%1] [ERROR] %2").arg(ts(), error);
}

void DataPrinter::onLinkStateChanged(IEC103::LinkState state)
{
    QString stateStr;
    switch (state) {
        case IEC103::LinkState::Disconnected:
            stateStr = "Disconnected";
            break;
        case IEC103::LinkState::Connecting:
            stateStr = "Connecting";
            break;
        case IEC103::LinkState::Connected:
            stateStr = "Connected";
            break;
        case IEC103::LinkState::Started:
            stateStr = "Started";
            break;
        case IEC103::LinkState::Stopped:
            stateStr = "Stopped";
            break;
        case IEC103::LinkState::Error:
            stateStr = "Error";
            break;
        default:
            stateStr = "Unknown";
            break;
    }
    qInfo().noquote() << QString("[%1] [STATE] %2").arg(ts(), stateStr);
}

void DataPrinter::onDoublePoint(const IEC103::DigitalPoint& point)
{
    QString valueStr;
    switch (point.value) {
        case IEC103::DoublePointValue::Indeterminate0:
            valueStr = "INDETERMINATE0";
            break;
        case IEC103::DoublePointValue::Off:
            valueStr = "OFF";
            break;
        case IEC103::DoublePointValue::On:
            valueStr = "ON";
            break;
        case IEC103::DoublePointValue::Indeterminate3:
            valueStr = "INDETERMINATE3";
            break;
        default:
            valueStr = "UNKNOWN";
            break;
    }

    qInfo().noquote() << QString("[%1] [DIGITAL] Dev=%2 FUN=%3 INF=%4 Value=%5 Quality=%6 EventTime=%7 RecvTime=%8")
        .arg(ts())
        .arg(point.deviceAddr())
        .arg(QString::number(point.fun, 16).toUpper())
        .arg(QString::number(point.inf, 16).toUpper())
        .arg(valueStr)
        .arg(formatQuality(point.quality))
        .arg(point.eventTime.toString("yyyy-MM-dd hh:mm:ss.zzz"))
        .arg(point.recvTime.toString("yyyy-MM-dd hh:mm:ss.zzz"));
}

void DataPrinter::onGenericValue(const IEC103::GenericPoint& point)
{
    // 根据数据类型格式化值
    QString typeStr;
    QString valueStr;
    
    if (point.isFloat()) {
        typeStr = "浮点";
        valueStr = QString::number(point.toFloat(), 'f', 2);
    } else if (point.isInteger()) {
        typeStr = "整数";
        valueStr = QString::number(point.toUInt32());
    } else {
        typeStr = QString("类型%1").arg(point.dataType);
        valueStr = point.valueString();
    }
    
    qInfo().noquote() << QString("[%1] [GENERIC] Dev=%2 Group=%3 Entry=%4 Value=%5 Unit=%6 Type=%7 Quality=%8 Desc=%9")
        .arg(ts())
        .arg(point.deviceAddr())
        .arg(point.group)
        .arg(point.entry)
        .arg(valueStr)
        .arg(point.unit)
        .arg(typeStr)
        .arg(formatQuality(point.quality))
        .arg(point.description);
}

void DataPrinter::onGenericData(uint16_t asduAddr, const IEC103::GenericDataItem& item)
{
    uint8_t devAddr = (asduAddr >> 8) & 0xFF;
    qInfo().noquote() << QString("[%1] [GENERIC_RAW] Dev=%2 GIN=%3/%4 DataType=%5 Size=%6")
        .arg(ts())
        .arg(devAddr)
        .arg(item.gin.group)
        .arg(item.gin.entry)
        .arg(static_cast<int>(item.gdd.dataType))
        .arg(item.gdd.dataSize);
}

void DataPrinter::onGIStarted(uint8_t deviceAddr)
{
    qInfo().noquote() << QString("[%1] [GI] Started for device %2").arg(ts()).arg(deviceAddr);
}

void DataPrinter::onGICompleted(uint8_t deviceAddr)
{
    qInfo().noquote() << QString("[%1] [GI] Completed for device %2").arg(ts()).arg(deviceAddr);
}

QString DataPrinter::formatQuality(const IEC103::Quality& quality)
{
    QStringList flags;
    if (quality.invalid) flags << "IV";
    if (quality.notTopical) flags << "NT";
    if (quality.substituted) flags << "SB";
    if (quality.blocked) flags << "BL";
    if (quality.overflow) flags << "OV";
    
    return flags.isEmpty() ? "OK" : flags.join(",");
}

QString DataPrinter::formatTimestamp(const IEC103::CP56Time2a& ts)
{
    QDateTime dt = ts.toDateTime();
    return dt.toString("yyyy-MM-dd hh:mm:ss.zzz");
}