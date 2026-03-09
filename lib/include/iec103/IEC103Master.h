#ifndef IEC103_IEC103MASTER_H
#define IEC103_IEC103MASTER_H

#include "types/Types.h"
#include "types/Constants.h"
#include "callback/IDataHandler.h"
#include "apci/Frame.h"
#include "apci/SeqManager.h"
#include "apci/SendQueue.h"
#include "asdu/ASDU.h"
#include <QObject>
#include <QTimer>
#include <memory>

namespace IEC103 {

// 前向声明
class TcpTransport;

// IEC103主站类
class IEC103Master : public QObject {
    Q_OBJECT

public:
    struct Config {
        QString host;
        quint16 port = 2404;
        int connectTimeout = 30000;    // T0连接超时(ms)
        int reconnectInterval = 5000;  // 重连间隔(ms)
        int timeout = 15000;           // T1超时
        int ackTimeout = 10000;        // T2超时
        int testInterval = 20000;      // T3测试间隔
        int maxUnconfirmed = 12;       // k值
        int maxAckDelay = 8;           // w值
        int maxReconnectCount = 10;    // 最大重连次数，0=无限
    };

    explicit IEC103Master(QObject* parent = nullptr);
    ~IEC103Master() override;

    // 配置
    void setConfig(const Config& config);
    const Config& config() const { return m_config; }

    // 连接管理
    void connectToServer();
    void disconnectFromServer();
    bool isConnected() const;

    // 状态
    LinkState state() const;

    // 数据回调
    void setDataHandler(IDataHandler* handler);

    // 数据召唤
    void generalInterrogation(uint16_t deviceAddr = 0);
    void readGenericGroup(uint16_t deviceAddr, uint8_t group);

    // 周期性总召唤
    void startPeriodicGI(int intervalMs);
    void stopPeriodicGI();

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& error);
    void stateChanged(LinkState state);
    void giFinished(uint16_t deviceAddr);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(const QString& error);
    void onDataReceived(const QByteArray& data);
    void onConnectTimeout();
    void onCmdTimeout();
    void onTestTimeout();
    void onAckTimeout();
    void onGITimeout();
    void onKBlockTimeout();  // k值阻塞超时

private:
    // 帧处理
    void processReceivedFrame(const Frame& frame);
    void processIFrame(const Frame& frame);
    void processSFrame(const Frame& frame);
    void processUFrame(const Frame& frame);

    // ASDU处理
    void processAsdu(const Asdu& asdu);
    void processAsdu1(const Asdu& asdu);
    void processAsdu2(const Asdu& asdu);
    void processAsdu8(const Asdu& asdu);
    void processAsdu10(const Asdu& asdu);
    void processAsdu11(const Asdu& asdu);
    void processAsdu42(const Asdu& asdu);

    // 发送方法
    void sendFrame(const Frame& frame);
    void send(const QByteArray& data);
    void sendAck();

    // 状态管理
    void setState(LinkState state);
    void startTestTimer();
    void stopTestTimer();
    void startAckTimer();
    void stopAckTimer();

private:
    Config m_config;
    class TcpTransport* m_transport;
    SeqManager m_seqManager;
    SendQueue m_sendQueue;  // 发送队列
    IDataHandler* m_handler = nullptr;

    LinkState m_state = LinkState::Disconnected;
    bool m_startDtReceived = false;
    bool m_testFrPending = false;      // TESTFR等待确认标志

    QTimer* m_connectTimer;  // T0连接超时
    QTimer* m_cmdTimer;      // T1命令超时
    QTimer* m_testTimer;
    QTimer* m_ackTimer;
    QTimer* m_giTimer;
    QTimer* m_kBlockTimer;   // k值阻塞超时

    // 总召唤状态
    struct GIState {
        bool inProgress = false;
        uint16_t deviceAddr = 0;
        uint8_t scn = 0;
    } m_giState;

    // 通用服务状态
    struct GenericState {
        bool inProgress = false;
        uint16_t deviceAddr = 0;
        uint8_t group = 0;
    } m_genericState;
};

} // namespace IEC103

#endif // IEC103_IEC103MASTER_H
