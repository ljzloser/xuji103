#include "iec103/IEC103Master.h"
#include "iec103/apci/Frame.h"
#include "iec103/apci/SeqManager.h"
#include "iec103/asdu/ASDU.h"
#include "iec103/asdu/ASDU1.h"
#include "iec103/asdu/ASDU7_8.h"
#include "iec103/asdu/ASDU10_21.h"
#include "iec103/asdu/ASDU11.h"
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
    , m_connectTimer(new QTimer(this))
    , m_cmdTimer(new QTimer(this))
    , m_testTimer(new QTimer(this))
    , m_ackTimer(new QTimer(this))
    , m_giTimer(new QTimer(this))
    , m_kBlockTimer(new QTimer(this))
{
    connect(m_transport, &TcpTransport::connected, this, &IEC103Master::onConnected);
    connect(m_transport, &TcpTransport::disconnected, this, &IEC103Master::onDisconnected);
    connect(m_transport, &TcpTransport::errorOccurred, this, &IEC103Master::onError);
    connect(m_transport, &TcpTransport::dataReceived, this, &IEC103Master::onDataReceived);

    m_connectTimer->setSingleShot(true);
    connect(m_connectTimer, &QTimer::timeout, this, &IEC103Master::onConnectTimeout);
    
    m_cmdTimer->setSingleShot(true);
    connect(m_cmdTimer, &QTimer::timeout, this, &IEC103Master::onCmdTimeout);
    
    m_testTimer->setSingleShot(true);
    connect(m_testTimer, &QTimer::timeout, this, &IEC103Master::onTestTimeout);
    
    m_ackTimer->setSingleShot(true);
    connect(m_ackTimer, &QTimer::timeout, this, &IEC103Master::onAckTimeout);
    
    m_giTimer->setSingleShot(false);
    connect(m_giTimer, &QTimer::timeout, this, &IEC103Master::onGITimeout);
    
    m_kBlockTimer->setSingleShot(true);
    connect(m_kBlockTimer, &QTimer::timeout, this, &IEC103Master::onKBlockTimeout);
}

IEC103Master::~IEC103Master()
{
    disconnectFromServer();
}

void IEC103Master::setConfig(const Config& config)
{
    // 参数校验 (参照南网规范附录A)
    m_config = config;
    
    // k值校验: 1~32767 (南网规范附录A图表A.5)
    if (m_config.maxUnconfirmed < 1 || m_config.maxUnconfirmed > 32767) {
        qWarning() << "IEC103Master: Invalid maxUnconfirmed (k)=" << m_config.maxUnconfirmed 
                   << ", using default 12";
        m_config.maxUnconfirmed = 12;
    }
    
    // w值校验: 1~32767, 建议 w ≤ 2k/3 (南网规范附录A图表A.5)
    if (m_config.maxAckDelay < 1 || m_config.maxAckDelay > 32767) {
        qWarning() << "IEC103Master: Invalid maxAckDelay (w)=" << m_config.maxAckDelay 
                   << ", using default 8";
        m_config.maxAckDelay = 8;
    }
    if (m_config.maxAckDelay > m_config.maxUnconfirmed * 2 / 3) {
        qWarning() << "IEC103Master: maxAckDelay (w)=" << m_config.maxAckDelay 
                   << " > 2k/3 (k=" << m_config.maxUnconfirmed << "), not recommended";
    }
    
    // T0连接超时校验: 最小1秒 (南网规范附录A图表A.4)
    if (m_config.connectTimeout < 1000) {
        qWarning() << "IEC103Master: connectTimeout (T0)=" << m_config.connectTimeout 
                   << "ms too short, using minimum 1000ms";
        m_config.connectTimeout = 1000;
    }
    
    // T1命令超时校验: 最小1秒 (南网规范附录A图表A.4)
    if (m_config.timeout < 1000) {
        qWarning() << "IEC103Master: timeout (T1)=" << m_config.timeout 
                   << "ms too short, using minimum 1000ms";
        m_config.timeout = 1000;
    }
    
    // T2确认超时校验: 最小1秒, T2 < T1 (南网规范附录A图表A.4)
    if (m_config.ackTimeout < 1000) {
        qWarning() << "IEC103Master: ackTimeout (T2)=" << m_config.ackTimeout 
                   << "ms too short, using minimum 1000ms";
        m_config.ackTimeout = 1000;
    }
    if (m_config.ackTimeout >= m_config.timeout) {
        qWarning() << "IEC103Master: ackTimeout (T2)=" << m_config.ackTimeout 
                   << "ms >= timeout (T1)=" << m_config.timeout << "ms, should be T2 < T1";
    }
    
    // T3测试间隔校验: 最小1秒 (南网规范附录A图表A.4)
    if (m_config.testInterval < 1000) {
        qWarning() << "IEC103Master: testInterval (T3)=" << m_config.testInterval 
                   << "ms too short, using minimum 1000ms";
        m_config.testInterval = 1000;
    }
    
    TcpTransport::Config transportConfig;
    transportConfig.host = m_config.host;
    transportConfig.port = m_config.port;
    transportConfig.reconnectInterval = m_config.reconnectInterval;
    transportConfig.autoReconnect = true;
    transportConfig.maxReconnectCount = m_config.maxReconnectCount;
    m_transport->setConfig(transportConfig);
}

void IEC103Master::connectToServer()
{
    m_seqManager.reset();
    m_startDtReceived = false;
    
    // 启动T0连接超时定时器
    m_connectTimer->start(m_config.connectTimeout);
    Logger::debug(QString("[T0] Connect timeout timer started: %1ms").arg(m_config.connectTimeout));
    
    m_transport->connectToServer();
}
void IEC103Master::disconnectFromServer()
{
    m_connectTimer->stop();
    m_cmdTimer->stop();
    m_kBlockTimer->stop();  // 停止k值阻塞超时定时器
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

    // k值控制：未确认帧数超过k时暂停发送
    uint16_t unconfirmed = m_seqManager.unconfirmedCount();
    if (unconfirmed >= m_config.maxUnconfirmed) {
        Logger::warning(QString("[k-Control] Send blocked: unconfirmed=%1 >= k=%2")
                        .arg(unconfirmed).arg(m_config.maxUnconfirmed));
        // 启动k值阻塞超时定时器
        if (!m_kBlockTimer->isActive()) {
            m_kBlockTimer->start(m_config.timeout);
            Logger::warning(QString("[k-Control] Block timeout timer started: %1ms").arg(m_config.timeout));
        }
        return;
    }
    
    // k值阻塞解除，停止超时定时器
    if (m_kBlockTimer->isActive()) {
        m_kBlockTimer->stop();
        Logger::info("[k-Control] Block cleared, timer stopped");
    }
    
    Logger::debug(QString("[k-Control] Before send: unconfirmed=%1, k=%2")
                  .arg(unconfirmed).arg(m_config.maxUnconfirmed));

    Asdu asdu = Asdu7Builder::build(deviceAddr, m_giState.scn);
    QByteArray asduData = asdu.encode();

    uint16_t sendSeq = m_seqManager.nextSendSeq();
    uint16_t recvSeq = m_seqManager.recvSeq();
    Frame frame = FrameCodec::createIFrame(sendSeq, recvSeq, asduData);

    m_giState.inProgress = true;
    m_giState.deviceAddr = deviceAddr;
    m_giState.scn++;

    sendFrame(frame);
    m_sendQueue.enqueue(sendSeq, frame);  // 入队待确认
    
    // 启动T1命令超时定时器
    m_cmdTimer->start(m_config.timeout);
    Logger::debug(QString("[T1] Command timeout timer started: %1ms").arg(m_config.timeout));
    
    Logger::info(QString("[I-TX] GI cmd N(S)=%1 N(R)=%2, Dev=%3 SCN=%4, unconfirmed=%5")
                 .arg(sendSeq).arg(recvSeq).arg(deviceAddr).arg(m_giState.scn)
                 .arg(m_seqManager.unconfirmedCount()));

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

    // k值控制：未确认帧数超过k时暂停发送
    uint16_t unconfirmed = m_seqManager.unconfirmedCount();
    if (unconfirmed >= m_config.maxUnconfirmed) {
        Logger::warning(QString("[k-Control] Send blocked: unconfirmed=%1 >= k=%2")
                        .arg(unconfirmed).arg(m_config.maxUnconfirmed));
        // 启动k值阻塞超时定时器
        if (!m_kBlockTimer->isActive()) {
            m_kBlockTimer->start(m_config.timeout);
            Logger::warning(QString("[k-Control] Block timeout timer started: %1ms").arg(m_config.timeout));
        }
        return;
    }
    
    // k值阻塞解除，停止超时定时器
    if (m_kBlockTimer->isActive()) {
        m_kBlockTimer->stop();
        Logger::info("[k-Control] Block cleared, timer stopped");
    }
    
    Logger::debug(QString("[k-Control] Before send: unconfirmed=%1, k=%2")
                  .arg(unconfirmed).arg(m_config.maxUnconfirmed));

    Asdu asdu = Asdu21Builder::buildReadGroup(deviceAddr, group, KOD::ActualValue);
    QByteArray asduData = asdu.encode();

    uint16_t sendSeq = m_seqManager.nextSendSeq();
    uint16_t recvSeq = m_seqManager.recvSeq();
    Frame frame = FrameCodec::createIFrame(sendSeq, recvSeq, asduData);

    m_genericState.inProgress = true;
    m_genericState.deviceAddr = deviceAddr;
    m_genericState.group = group;

    sendFrame(frame);
    m_sendQueue.enqueue(sendSeq, frame);  // 入队待确认
    
    // 启动T1命令超时定时器
    m_cmdTimer->start(m_config.timeout);
    Logger::debug(QString("[T1] Command timeout timer started: %1ms").arg(m_config.timeout));
    
    Logger::info(QString("[I-TX] ReadGroup N(S)=%1 N(R)=%2, Dev=%3 Group=%4, unconfirmed=%5")
                 .arg(sendSeq).arg(recvSeq).arg(deviceAddr).arg(group)
                 .arg(m_seqManager.unconfirmedCount()));
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
    // TCP连接成功，停止T0超时定时器
    m_connectTimer->stop();
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
    m_testFrPending = false;  // 重置 TESTFR 状态
    stopTestTimer();
    stopAckTimer();
    
    // 重置序号管理器
    m_seqManager.reset();
    m_sendQueue.clear();  // 清空发送队列
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

void IEC103Master::onConnectTimeout()
{
    QString reason = QString("Connection timeout (T0=%1ms)").arg(m_config.connectTimeout);
    Logger::error(QString("[T0] ") + reason + " - Disconnecting!");
    
    // 先通知应用层
    if (m_handler) {
        m_handler->onDisconnected(reason);
        m_handler->onError(reason);
    }
    
    disconnectFromServer();
}

void IEC103Master::onCmdTimeout()
{
    QString reason = QString("Command timeout (T1=%1ms)").arg(m_config.timeout);
    Logger::error(QString("[T1] ") + reason + " - Disconnecting!");
    
    // 先通知应用层
    if (m_handler) {
        m_handler->onDisconnected(reason);
        m_handler->onError(reason);
    }
    
    disconnectFromServer();
}

void IEC103Master::onTestTimeout()
{
    // 检查上次 TESTFR 是否收到确认
    if (m_testFrPending) {
        QString reason = QString("TESTFR timeout (T1=%1ms)").arg(m_config.timeout);
        Logger::error(QString("[TESTFR] ") + reason + " - Disconnecting!");
        
        // 先通知应用层
        if (m_handler) {
            m_handler->onDisconnected(reason);
            m_handler->onError(reason);
        }
        
        disconnectFromServer();
        return;
    }
    
    Logger::debug("Sending TESTFR");
    Frame testFr = FrameCodec::createUFrame(UControl::TestFrAct);
    sendFrame(testFr);
    m_testFrPending = true;
    startTestTimer();  // T1 超时等待确认
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

void IEC103Master::onKBlockTimeout()
{
    QString reason = QString("k-value block timeout (k=%1, timeout=%2ms)")
                     .arg(m_config.maxUnconfirmed).arg(m_config.timeout);
    Logger::error(QString("[k-Control] ") + reason + " - Disconnecting!");
    
    // 先通知应用层
    if (m_handler) {
        m_handler->onDisconnected(reason);
        m_handler->onError(reason);
    }
    
    disconnectFromServer();
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
        Logger::error(QString("[I-RX] SEQUENCE ERROR: N(S)=%1, expected=%2 - Disconnecting!")
                      .arg(recvSeq).arg(m_seqManager.recvSeq()));
        // 根据IEC 104协议，序号错误表示可能丢帧或重复，应断开重连
        disconnectFromServer();
        return;
    }
    m_seqManager.nextRecvSeq();

    // 收到I帧响应，停止T1命令超时定时器
    if (m_cmdTimer->isActive()) {
        m_cmdTimer->stop();
        Logger::debug("[T1] Command timeout timer stopped (response received)");
    }

    uint16_t ackSeq = frame.recvSeq();
    uint16_t prevAck = m_seqManager.lastAckSeq();
    m_seqManager.setLastAckSeq(ackSeq);

    // w值控制：收到w个I帧后立即发S帧确认
    m_seqManager.incrementRecvCount();
    uint8_t recvCount = m_seqManager.recvCount();
    Logger::info(QString("[w-Control] I-Frame #%1 received, w=%2, threshold=%3")
                  .arg(recvCount).arg(m_config.maxAckDelay)
                  .arg(recvCount >= m_config.maxAckDelay ? "REACHED" : "not reached"));

    if (recvCount >= m_config.maxAckDelay) {
        Logger::info(QString("[w-Control] >>> SENDING S-FRIME NOW (w=%1 reached) <<<")
                     .arg(m_config.maxAckDelay));
        sendAck();
        m_seqManager.resetRecvCount();
        stopAckTimer();
    } else {
        startAckTimer();  // T2兜底
    }

    // 打印确认信息
    if (ackSeq > prevAck || (ackSeq == 0 && prevAck > 30000)) {
        uint16_t confirmedCount = (ackSeq - prevAck) & 0x7FFF;
        Logger::debug(QString("[I-RX] Peer confirmed: N(R)=%1, confirmed %2 frames, unconfirmed=%3")
                      .arg(ackSeq).arg(confirmedCount).arg(m_seqManager.unconfirmedCount()));
    }

    Asdu asdu;
    if (asdu.parse(frame.asdu())) {
        Logger::debug(QString("[I-RX] ASDU TI=%1 COT=%2 Addr=%3")
                      .arg(asdu.ti()).arg(asdu.cot()).arg(asdu.addr()));
        processAsdu(asdu);
    } else {
        Logger::warning("Failed to parse ASDU");
    }
}
void IEC103Master::processSFrame(const Frame& frame)
{
    uint16_t ackSeq = frame.recvSeq();
    uint16_t sendSeq = m_seqManager.sendSeq();
    
    // 验证：N(R) 不能大于已发送序号
    // 注意：sendSeq 是下一个要发送的序号，所以 N(R) 应该 <= sendSeq
    if (ackSeq > sendSeq) {
        Logger::error(QString("[S-RX] SEQUENCE ERROR: N(R)=%1 > sendSeq=%2 - Disconnecting!")
                      .arg(ackSeq).arg(sendSeq));
        disconnectFromServer();
        return;
    }
    
    uint16_t prevAck = m_seqManager.lastAckSeq();
    m_seqManager.setLastAckSeq(ackSeq);

    // 从发送队列中移除已确认的帧
    m_sendQueue.confirm(ackSeq);

    uint16_t confirmedCount = 0;
    if (ackSeq >= prevAck) {
        confirmedCount = ackSeq - prevAck;
    } else {
        confirmedCount = (ackSeq + 32768) - prevAck;  // 序号回绕
    }
    Logger::info(QString("[S-RX] Ack received: N(R)=%1, confirmed %2 frames, unconfirmed=%3")
                 .arg(ackSeq).arg(confirmedCount).arg(m_seqManager.unconfirmedCount()));
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
        m_testFrPending = false;  // 清除等待确认标志
        startTestTimer();         // 重新开始 T3 计时
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
    case 11:
        processAsdu11(asdu);
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
            // 创建统一的GenericPoint
            GenericPoint point;
            point.deviceAddr = deviceAddr;
            point.group = item.gin.group;
            point.entry = item.gin.entry;
            point.dataType = item.gdd.dataType;
            
            // 根据dataType解析值
            switch (item.gdd.dataType) {
            case static_cast<uint8_t>(DataType::OS8ASCII):
                // ASCII字符串
                point.value = item.toAsciiString();
                Logger::info(QString("Generic(string): Dev=%1 Group=%2 Entry=%3 Value=\"%4\" DataType=1")
                            .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry)
                            .arg(point.value.toString()));
                break;
                
            case static_cast<uint8_t>(DataType::UI):
                // 无符号整数 (遥脉/状态量)
                point.value = item.toUInt32();
                Logger::info(QString("Generic(uint): Dev=%1 Group=%2 Entry=%3 Value=%4 DataType=3")
                            .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry)
                            .arg(point.toUInt32()));
                break;
                
            case static_cast<uint8_t>(DataType::I):
            case static_cast<uint8_t>(DataType::UF):
                // 有符号整数
                point.value = item.toInt32();
                Logger::info(QString("Generic(int): Dev=%1 Group=%2 Entry=%3 Value=%4 DataType=%5")
                            .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry)
                            .arg(point.toInt32()).arg(item.gdd.dataType));
                break;
                
            case static_cast<uint8_t>(DataType::R32_23):
            case static_cast<uint8_t>(DataType::F):
                // 浮点数 (遥测)
                point.value = item.toFloat();
                Logger::info(QString("Generic(float): Dev=%1 Group=%2 Entry=%3 Value=%4 DataType=%5")
                            .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry)
                            .arg(point.toFloat(), 0, 'f', 2).arg(item.gdd.dataType));
                break;
                
            case static_cast<uint8_t>(DataType::DPI):
                // 双点信息
                point.value = static_cast<int>(item.toDPI());
                Logger::info(QString("Generic(dpi): Dev=%1 Group=%2 Entry=%3 Value=%4 DataType=9")
                            .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry)
                            .arg(static_cast<int>(item.toDPI())));
                break;
                
            // 南网扩展数据类型 213-217
            case static_cast<uint8_t>(DataType::TimeTagMsg7):
                // DataType 213: 带绝对时间七字节时标报文
                point.value = item.absoluteTime();
                point.timestamp = item.absoluteTime();
                Logger::info(QString("Generic(time213): Dev=%1 Group=%2 Entry=%3 Time=%4 DataType=213")
                            .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry)
                            .arg(point.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz")));
                break;
                
            case static_cast<uint8_t>(DataType::TimeTagMsgRel7):
                // DataType 214: 带相对时间七字节时标报文
                point.value = item.relativeTime();
                point.timestamp = item.relativeTime();
                Logger::info(QString("Generic(time214): Dev=%1 Group=%2 Entry=%3 Time=%4 DataType=214")
                            .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry)
                            .arg(point.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz")));
                break;
                
            case static_cast<uint8_t>(DataType::FloatTimeTag7):
                // DataType 215: 带相对时间七字节时标的浮点值
                point.value = item.floatWithTime();
                point.timestamp = item.relativeTime();
                Logger::info(QString("Generic(float215): Dev=%1 Group=%2 Entry=%3 Value=%4 Time=%5 DataType=215")
                            .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry)
                            .arg(point.toFloat(), 0, 'f', 2)
                            .arg(point.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz")));
                break;
                
            case static_cast<uint8_t>(DataType::IntTimeTag7):
                // DataType 216: 带相对时间七字节时标的整形值
                point.value = item.intWithTime();
                point.timestamp = item.relativeTime();
                Logger::info(QString("Generic(int216): Dev=%1 Group=%2 Entry=%3 Value=%4 Time=%5 DataType=216")
                            .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry)
                            .arg(point.toInt32())
                            .arg(point.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz")));
                break;
                
            case static_cast<uint8_t>(DataType::CharTimeTag7):
                // DataType 217: 带相对时间七字节时标的字符值
                point.value = item.stringWithTime();
                point.timestamp = item.relativeTime();
                Logger::info(QString("Generic(char217): Dev=%1 Group=%2 Entry=%3 Value=\"%4\" Time=%5 DataType=217")
                            .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry)
                            .arg(point.value.toString())
                            .arg(point.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz")));
                break;
                
            default:
                // 其他类型: 尝试作为原始数据处理
                if (item.gdd.dataSize == 4 && item.gid.size() >= 4) {
                    // 尝试作为浮点数或整数
                    point.value = item.toFloat();
                    Logger::info(QString("Generic(raw4): Dev=%1 Group=%2 Entry=%3 DataType=%4 Size=%5")
                                .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry)
                                .arg(item.gdd.dataType).arg(item.gdd.dataSize));
                } else {
                    // 记录原始数据
                    point.value = QString("[raw:%1 bytes]").arg(item.gid.size());
                    Logger::info(QString("Generic(unknown): Dev=%1 Group=%2 Entry=%3 DataType=%4 Size=%5")
                                .arg(deviceAddr).arg(item.gin.group).arg(item.gin.entry)
                                .arg(item.gdd.dataType).arg(item.gdd.dataSize));
                }
                break;
            }
            
            // 统一回调
            if (m_handler) {
                m_handler->onGenericValue(point);
                m_handler->onGenericData(deviceAddr, item);
            }
        }
    }
}

void IEC103Master::processAsdu11(const Asdu& asdu)
{
    Asdu11Parser parser;
    if (parser.parse(asdu)) {
        uint16_t deviceAddr = asdu.addr();
        const Asdu11Data& data = parser.data();
        
        Logger::info(QString("Catalog (ASDU11): Dev=%1 Group=%2 Entry=%3 NGD=%4")
                    .arg(deviceAddr).arg(data.gin.group).arg(data.gin.entry).arg(data.ngd));
        
        for (const auto& entry : data.entries) {
            Logger::debug(QString("  KOD=%1(%2) DataType=%3 Size=%4 Number=%5")
                         .arg(entry.kod, 2, 16, QChar('0'))
                         .arg(entry.kodDescription())
                         .arg(entry.gdd.dataType)
                         .arg(entry.gdd.dataSize)
                         .arg(entry.gdd.number));
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
    m_seqManager.resetRecvCount();  // 发送确认后重置接收计数
    Logger::info(QString("[S-TX] Sent S-Frame N(R)=%1 (acknowledged peer's I-frames)")
                 .arg(recvSeq));
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
