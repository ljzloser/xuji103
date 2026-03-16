#ifndef IEC103_IEC103MASTER_H
#define IEC103_IEC103MASTER_H

#include "types/Types.h"
#include "types/Constants.h"
#include "callback/IDataHandler.h"
#include "callback/ILogHandler.h"
#include "apci/Frame.h"
#include "apci/SeqManager.h"
#include "apci/SendQueue.h"
#include "asdu/ASDU.h"
#include <QObject>
#include <QTimer>
#include <memory>

namespace IEC103
{

    // 前向声明
    class TcpTransport;
    class Logger;

    // IEC103主站类
    class IEC103Master : public QObject
    {
        Q_OBJECT

    public:
        struct Config
        {
            QString host;
            quint16 port = 2404;
            int connectTimeout = 30000;   // T0连接超时(ms)
            int reconnectInterval = 5000; // 重连间隔(ms)
            int timeout = 15000;          // T1超时
            int ackTimeout = 10000;       // T2超时
            int testInterval = 20000;     // T3测试间隔
            int maxUnconfirmed = 12;      // k值
            int maxAckDelay = 8;          // w值
            int maxReconnectCount = 10;   // 最大重连次数，0=无限
        };

        explicit IEC103Master(QObject *parent = nullptr);
        ~IEC103Master() override;

        // 配置
        void setConfig(const Config &config);
        const Config &config() const { return m_config; }

        // 连接管理
        void connectToServer();
        void disconnectFromServer();
        bool isConnected() const;

        // 状态
        LinkState state() const;

        // 数据回调
        void setDataHandler(IDataHandler *handler);

        // 日志回调（每个IEC103Master实例独立）
        void setLogHandler(ILogHandler *handler);
        void setLogLevel(LogLevel level);
        LogLevel logLevel() const;

        // 数据召唤
        // deviceAddr: 设备编号 (0=子站本身, 1-254=装置编号, 255=广播)
        // cpuNo: CPU号 (0-7, 默认0)
        void generalInterrogation(uint8_t deviceAddr = 255);
        void generalInterrogationWithCpu(uint8_t deviceAddr, uint8_t cpuNo);
        void readGenericGroup(uint8_t deviceAddr, uint8_t group);
        void readGenericGroupWithCpu(uint8_t deviceAddr, uint8_t cpuNo, uint8_t group);

        // 发送原始报文AC
        // data: 完整APDU帧数据 (包含68H开头、长度域、APCI、ASDU)
        // 返回: true=发送成功, false=发送失败(未连接或帧格式错误)
        bool sendRawFrame(const QByteArray &data);

        // 发送原始ASDU (自动封装I格式帧)
        // asduData: ASDU原始数据
        // 返回: true=发送成功, false=发送失败
        bool sendRawAsdu(const QByteArray &asduData);

    signals:
        void connected();
        void disconnected();
        void errorOccurred(const QString &error);
        void stateChanged(LinkState state);
        void giFinished(uint8_t deviceAddr); // 设备编号

    private slots:
        void onConnected();
        void onDisconnected();
        void onError(const QString &error);
        void onDataReceived(const QByteArray &data);
        void onConnectTimeout();
        void onCmdTimeout();
        void onTestTimeout();
        void onAckTimeout();
        void onKBlockTimeout(); // k值阻塞超时

    private:
        // 帧处理
        void processReceivedFrame(const Frame &frame);
        void processIFrame(const Frame &frame);
        void processSFrame(const Frame &frame);
        void processUFrame(const Frame &frame);

        // ASDU处理
        void processAsdu(const Asdu &asdu);
        void processAsdu1(const Asdu &asdu);
        void processAsdu2(const Asdu &asdu);
        void processAsdu8(const Asdu &asdu);
        void processAsdu10(const Asdu &asdu);
        void processAsdu11(const Asdu &asdu);
        void processAsdu42(const Asdu &asdu);

        // 发送方法
        void sendFrame(const Frame &frame);
        void send(const QByteArray &data);
        void sendAck();

        // 状态管理
        void setState(LinkState state);
        void startTestTimer();
        void stopTestTimer();
        void startAckTimer();
        void stopAckTimer();

    private:
        Config m_config;
        class TcpTransport *m_transport;
        SeqManager m_seqManager;
        SendQueue m_sendQueue; // 发送队列
        IDataHandler *m_handler = nullptr;
        Logger *m_logger = nullptr; // 日志器（实例独立）

        LinkState m_state = LinkState::Disconnected;
        bool m_startDtReceived = false;
        bool m_testFrPending = false; // TESTFR等待确认标志

        QTimer *m_connectTimer; // T0连接超时
        QTimer *m_cmdTimer;     // T1命令超时
        QTimer *m_testTimer;
        QTimer *m_ackTimer;
        QTimer *m_kBlockTimer; // k值阻塞超时

        // 总召唤状态
        struct GIState
        {
            bool inProgress = false;
            uint16_t asduAddr = 0; // 完整ASDU地址 (高字节=设备编号)
            uint8_t scn = 0;
        } m_giState;

        // 通用服务状态
        struct GenericState
        {
            bool inProgress = false;
            uint16_t asduAddr = 0; // 完整ASDU地址 (高字节=设备编号)
            uint8_t group = 0;
        } m_genericState;
    };

} // namespace IEC103

#endif // IEC103_IEC103MASTER_H