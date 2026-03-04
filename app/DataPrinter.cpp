#include "DataPrinter.h"
#include <QDebug>
#include <QDateTime>

DataPrinter::DataPrinter(QObject* parent)
    : QObject(parent)
{
}

void DataPrinter::onConnected()
{
    qInfo() << "[EVENT] Connected";
}

void DataPrinter::onDisconnected()
{
    qInfo() << "[EVENT] Disconnected";
}

void DataPrinter::onError(const QString& error)
{
    qWarning() << "[ERROR]" << error;
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
    qInfo() << "[STATE]" << stateStr;
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

    qInfo() << "[DIGITAL] Dev=" << point.deviceAddr
            << "FUN=" << QString::number(point.fun, 16).toUpper()
            << "INF=" << QString::number(point.inf, 16).toUpper()
            << "Value=" << valueStr
            << "Quality=" << formatQuality(point.quality)
            << "EventTime=" << point.eventTime.toString("yyyy-MM-dd hh:mm:ss.zzz")
            << "RecvTime=" << point.recvTime.toString("yyyy-MM-dd hh:mm:ss.zzz");
}

void DataPrinter::onAnalogValue(const IEC103::AnalogPoint& point)
{
    qInfo() << "[ANALOG] Dev=" << point.deviceAddr
            << "Group=" << point.group
            << "Entry=" << point.entry
            << "Value=" << point.value
            << "Quality=" << formatQuality(point.quality)
            << "Time=" << point.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
}

void DataPrinter::onCounterValue(const IEC103::CounterPoint& point)
{
    qInfo() << "[COUNTER] Dev=" << point.deviceAddr
            << "Group=" << point.group
            << "Entry=" << point.entry
            << "Value=" << point.value
            << "Quality=" << formatQuality(point.quality)
            << "Time=" << point.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
}

void DataPrinter::onGenericData(uint16_t deviceAddr, const IEC103::GenericDataItem& item)
{
    qInfo() << "[GENERIC] Dev=" << deviceAddr
            << "GIN=" << item.gin.group << "/" << item.gin.entry
            << "DataType=" << static_cast<int>(item.gdd.dataType)
            << "Size=" << item.gdd.dataSize;
}

void DataPrinter::onGIStarted(uint16_t deviceAddr)
{
    qInfo() << "[GI] Started for device" << deviceAddr;
}

void DataPrinter::onGICompleted(uint16_t deviceAddr)
{
    qInfo() << "[GI] Completed for device" << deviceAddr;
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
