#include "iec103/IEC103Master.h"
#include "iec103/apci/Frame.h"
#include "iec103/apci/SeqManager.h"
#include "iec103/asdu/ASDU.h"
#include "iec103/asdu/ASDU1.h"
#include "iec103/asdu/ASDU7_8.h"
#include "iec103/asdu/ASDU10_21.h"
#include "iec103/asdu/ASDU42.h"
#include "iec103/transport/TcpTransport.h"
#include "iec103/types/Constants.h"
#include "utils/Logger.h"
#include <QDebug>
#include <QThread>

namespace IEC103 {

IEC103Master::IEC103Master(QObject* parent)
    : QObject(parent)
    , m_transport(new TcpTransport(this))
    , m_testTimer(new QTimer(this))
    , m_ackTimer(new QTimer(this))
    , m_giTimer(new QTimer(this))
{
    connect(m_transport, &TcpTransport::connected, this, &IEC103Master::onConnected);
    connect(m_transport, &TcpTransport::disconnected, this, &IEC103Master::onDisconnected);
    connect(m_transport, &TcpTransport::errorOccurred, this, &IEC103Master::onError);
    connect(m_transport, &TcpTransport::dataReceived, this, &IEC103Master::onDataReceived);

    
    m_testTimer->setSingleShot(true);
    connect(m_testTimer, &QTimer::timeout, this, &IEC103Master::onTestTimeout);
    
    m_ackTimer->setSingleShot(true);
    connect(m_ackTimer, &QTimer::timeout, this, &IEC103Master::onAckTimeout);
    
    m_giTimer->setSingleShot(false);
    connect(m_giTimer, &QTimer::timeout, this, &IEC103Master::onGITimeout);
}

IEC103Master::~IEC103Master()
{
    disconnectFromServer();
}

void IEC103Master::setConfig(const Config& config)
{
    m_config = config;
    
    TcpTransport::Config transportConfig;
    transportConfig.host = config.host;
    transportConfig.port = config.port;
    transportConfig.reconnectInterval = config.reconnectInterval;
    transportConfig.autoReconnect = true;
    m_transport->setConfig(transportConfig);
}

void IEC103Master::connectToServer()
{
    m_seqManager.reset();
    m_startDtReceived = false;
    m_transport->connectToServer();
}
void IEC103Master::disconnectFromServer()
{
    stopTestTimer();
    stopAckTimer();
    m_giTimer->stop();
    
    // 发送STOPDT命令后再断开连接
    if (m_startDtReceived && m_transport->isConnected()) {
        Logger::info("Sending STOPDT before disconnect");
        Frame stopDt = FrameCodec::createUFrame(UControl::StopDtAct);
        sendFrame(stopDt);
    }
    
    m_transport->disconnectFromServer();
}
bool IEC103Master::isConnected() const
{
    return m_transport->isConnected() && m_startDtReceived;
}
LinkState IEC103Master::state() const
{
    return m_state;
}
void IEC103Master::setDataHandler(IDataHandler* handler)
{
    m_handler = handler;
}
void IEC103Master::generalInterrogation(uint16_t deviceAddr)
{
    if (!isConnected()) {
        Logger::warning("Cannot send GI, not connected");
        return;
    }
    
    Asdu asdu = Asdu7Builder::build(deviceAddr, m_giState.scn);
    QByteArray asduData = asdu.encode();
    
    uint16_t sendSeq = m_seqManager.nextSendSeq();
    uint16_t recvSeq = m_seqManager.recvSeq();
    Frame frame = FrameCodec::createIFrame(sendSeq, recvSeq, asduData);
    
    m_giState.inProgress = true;
    m_giState.deviceAddr = deviceAddr;
    m_giState.scn++;
    
    sendFrame(frame);
    Logger::info(QString("Sent GI to device %1, SCN=%2").arg(deviceAddr).arg(m_giState.scn));
    
    if (m_handler) {
        m_handler->onGIStarted(deviceAddr);
    }
}
void IEC103Master::readGenericGroup(uint16_t deviceAddr, uint8_t group)
{
    if (!isConnected()) {
        Logger::warning("Cannot read generic group, not connected");
        return;
    }
    
    Asdu asdu = Asdu21Builder::buildReadGroup(deviceAddr, group, KOD::ActualValue);
    QByteArray asduData = asdu.encode();
    
    uint16_t sendSeq = m_seqManager.nextSendSeq();
    uint16_t recvSeq = m_seqManager.recvSeq();
    Frame frame = FrameCodec::createIFrame(sendSeq, recvSeq, asduData);
    
    m_genericState.inProgress = true;
    m_genericState.deviceAddr = deviceAddr;
    m_genericState.group = group;
    
    sendFrame(frame);
    Logger::info(QString("Sent read group %1 to device %2").arg(group).arg(deviceAddr));
}
void IEC103Master::startPeriodicGI(int intervalMs)
{
    m_giTimer->start(intervalMs);
}
void IEC103Master::stopPeriodicGI()
{
    m_giTimer->stop();
}
// ========== Slot handlers ==========

void IEC103Master::onConnected()
{
    Logger::info("TCP connected, sending STARTDT");
    setState(LinkState::Connected);
    
    Frame startDt = FrameCodec::createUFrame(UControl::StartDtAct);
    sendFrame(startDt);
}
void IEC103Master::onDisconnected()
{
    Logger::info("Disconnected");
    setState(LinkState::Disconnected);
    m_startDtReceived = false;
    stopTestTimer();
    stopAckTimer();
    
    // 重置序号管理器
    m_seqManager.reset();
    m_giState.inProgress = false;
    m_genericState.inProgress = false;
    
    if (m_handler) {
        m_handler->onDisconnected();
    }
}
void IEC103Master::onError(const QString& error)
{
    Logger::error(QString("Transport error: %1").arg(error));
    if (m_handler) {
        m_handler->onError(error);
    }
}
void IEC103Master::onDataReceived(const QByteArray& data)
{
    Frame frame = FrameCodec::decode(data);
    if (!frame.isValid()) {
        Logger::warning("Received invalid frame");
        return;
    }
    
    processReceivedFrame(frame);
}
void IEC103Master::onTestTimeout()
{
    Logger::debug("Sending TESTFR");
    Frame testFr = FrameCodec::createUFrame(UControl::TestFrAct);
    sendFrame(testFr);
    startTestTimer();
}
void IEC103Master::onAckTimeout()
{
    sendAck();
}
void IEC103Master::onGITimeout()
{
    if (isConnected()) {
        generalInterrogation(0);
    }
}
// ========== Private methods ==========

void IEC103Master::processReceivedFrame(const Frame& frame)
{
    switch (frame.type()) {
    case ApduType::I_FORMAT:
        processIFrame(frame);
        break;
    case ApduType::S_FORMAT:
        processSFrame(frame);
        break;
    case ApduType::U_FORMAT:
        processUFrame(frame);
        break;
    }
}
void IEC103Master::processIFrame(const Frame& frame)
{
    uint16_t recvSeq = frame.sendSeq();
    if (!m_seqManager.validateRecvSeq(recvSeq)) {
        Logger::warning(QString("Invalid receive sequence: %1, expected: %2")
                        .arg(recvSeq).arg(m_seqManager.recvSeq()));
    }
    m_seqManager.nextRecvSeq();
    
    uint16_t ackSeq = frame.recvSeq();
    m_seqManager.setLastAckSeq(ackSeq);
    
    startAckTimer();
    
    Asdu asdu;
    if (asdu.parse(frame.asdu())) {
        processAsdu(asdu);
    } else {
        Logger::warning("Failed to parse ASDU");
    }
}
void IEC103Master::processSFrame(const Frame& frame)
{
    uint16_t ackSeq = frame.recvSeq();
    m_seqManager.setLastAckSeq(ackSeq);
    Logger::debug(QString("Received S-frame, ack=%1").arg(ackSeq));
}
void IEC103Master::processUFrame(const Frame& frame)
{
    if (frame.isStartDtCon()) {
        Logger::info("Received STARTDT confirmation");
        m_startDtReceived = true;
        setState(LinkState::Started);
        startTestTimer();
        
        // 发出连接成功信号，触发 GI 命令
        emit connected();
        
        if (m_handler) {
            m_handler->onConnected();
        }
    } else if (frame.isStopDtCon()) {
        Logger::info("Received STOPDT confirmation");
        m_startDtReceived = false;
        setState(LinkState::Stopped);
    } else if (frame.isTestFrAct()) {
        Logger::debug("Received TESTFR act, sending con");
        Frame testFrCon = FrameCodec::createUFrame(UControl::TestFrCon);
        sendFrame(testFrCon);
    } else if (frame.isTestFrCon()) {
        Logger::debug("Received TESTFR con");
        startTestTimer();
    }
}
void IEC103Master::processAsdu(const Asdu& asdu)
{
    uint8_t ti = asdu.ti();
    Logger::debug(QString("Processing ASDU TI=%1").arg(ti));
    
    switch (ti) {
    case 1:
        processAsdu1(asdu);
        break;
    case 2:
        processAsdu2(asdu);
        break;
    case 8:
        processAsdu8(asdu);
        break;
    case 10:
        processAsdu10(asdu);
        break;
    case 42:
        processAsdu42(asdu);
        break;
    default:
        Logger::warning(QString("Unhandled ASDU TI=%1").arg(ti));
    }
}
void IEC103Master::processAsdu1(const Asdu& asdu)
{
    Asdu1Parser parser;
    if (parser.parse(asdu)) {
        uint16_t deviceAddr = asdu.addr();
        for (const auto& result : parser.results()) {
            DigitalPoint point = parser.toDigitalPoint(result, deviceAddr);
            Logger::info(QString("Digital (ASDU1): Dev=%1 FUN=%2 INF=%3 DPI=%4")
                        .arg(deviceAddr).arg(point.fun).arg(point.inf)
                        .arg(static_cast<int>(point.value)));
            
            if (m_handler) {
                m_handler->onDoublePoint(point);
            }
        }
    }
}
void IEC103Master::processAsdu2(const Asdu& asdu)
{
    Asdu2Parser parser;
    if (parser.parse(asdu)) {
        uint16_t deviceAddr = asdu.addr();
        for (const auto& result : parser.results()) {
            Logger::info(QString("Event (ASDU2): Dev=%1 FUN=%2 INF=%3 DPI=%4 FaultNo=%5")
                        .arg(deviceAddr).arg(result.fun).arg(result.inf)
                        .arg(static_cast<int>(result.dpi)).arg(result.faultNo));
        }
    }
}
void IEC103Master::processAsdu8(const Asdu& asdu)
{
    Asdu8Parser parser;
    if (parser.parse(asdu)) {
        Logger::info(QString("GI terminated for device %1, SCN=%2")
                    .arg(parser.result().deviceAddr).arg(parser.result().scn));
        
        // 检查GI状态：地址0(子站本身)匹配任何响应地址，或者地址精确匹配
        bool addrMatch = (m_giState.deviceAddr == 0) || 
                         (m_giState.deviceAddr == parser.result().deviceAddr);
        
        if (m_giState.inProgress && addrMatch) {
            m_giState.inProgress = false;
            
            // 发送总召唤完成信号，触发遥测/遥脉召唤
            emit giFinished(parser.result().deviceAddr);
            
            if (m_handler) {
                m_handler->onGICompleted(parser.result().deviceAddr);
            }
        }
    }
}
void IEC103Master::processAsdu10(const Asdu& asdu)
{
    Asdu10Parser parser;
    if (parser.parse(asdu)) {
        uint16_t deviceAddr = asdu.addr();
        
        for (const auto& item : parser.data().items) {
            if (item.gdd.dataType == static_cast<uint8_t>(DataType::R32_23)) {
                float value = item.toFloat();
                Logger::info(QString("Analog: Dev=%1 Group=%2 Entry=%3 Value=%4")
                            .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry).arg(value));
                
                if (m_handler) {
                    AnalogPoint point;
                    point.deviceAddr = deviceAddr;
                    point.group = item.gin.group;
                    point.entry = item.gin.entry;
                    point.value = value;
                    m_handler->onAnalogValue(point);
                }
            } else if (item.gdd.dataType == static_cast<uint8_t>(DataType::UI)) {
                uint32_t value = item.toUInt32();
                Logger::info(QString("Counter: Dev=%1 Group=%2 Entry=%3 Value=%4")
                            .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry).arg(value));
                
                if (m_handler) {
                    CounterPoint point;
                    point.deviceAddr = deviceAddr;
                    point.group = item.gin.group;
                    point.entry = item.gin.entry;
                    point.value = value;
                    m_handler->onCounterValue(point);
                }
            }
            
            if (m_handler) {
                m_handler->onGenericData(deviceAddr, item);
            }
        }
    }
}
void IEC103Master::processAsdu42(const Asdu& asdu)
{
    Asdu42Parser parser;
    if (parser.parse(asdu)) {
        uint16_t deviceAddr = asdu.addr();
        auto points = parser.toDigitalPoints(deviceAddr);
        
        for (const auto& point : points) {
            Logger::info(QString("Digital (ASDU42): Dev=%1 FUN=%2 INF=%3 DPI=%4")
                        .arg(deviceAddr).arg(point.fun).arg(point.inf)
                        .arg(static_cast<int>(point.value)));
            
            if (m_handler) {
                m_handler->onDoublePoint(point);
            }
        }
    }
}
void IEC103Master::setState(LinkState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
        
        if (m_handler) {
            m_handler->onLinkStateChanged(state);
        }
    }
}
void IEC103Master::sendFrame(const Frame& frame)
{
    QByteArray data = frame.encode();
    send(data);
}
void IEC103Master::send(const QByteArray& data)
{
    if (!m_transport->send(data)) {
        Logger::error("Failed to send frame");
    }
}
void IEC103Master::sendAck()
{
    uint16_t recvSeq = m_seqManager.recvSeq();
    Frame sFrame = FrameCodec::createSFrame(recvSeq);
    sendFrame(sFrame);
    Logger::debug(QString("Sent S-frame with N(R)=%1").arg(recvSeq));
}
void IEC103Master::startTestTimer()
{
    m_testTimer->start(m_config.testInterval);
}
void IEC103Master::stopTestTimer()
{
    m_testTimer->stop();
}
void IEC103Master::startAckTimer()
{
    m_ackTimer->start(m_config.ackTimeout);
}
void IEC103Master::stopAckTimer()
{
    m_ackTimer->stop();
}

} // namespace IEC103
