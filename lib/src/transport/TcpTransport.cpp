#include "iec103/transport/TcpTransport.h"
#include "iec103/apci/Frame.h"
#include "iec103/types/Constants.h"
#include <QDebug>

namespace IEC103
{

    TcpTransport::TcpTransport(QObject *parent)
        : QObject(parent), m_socket(new QTcpSocket(this)), m_reconnectTimer(new QTimer(this))
    {
        connect(m_socket, &QTcpSocket::connected, this, &TcpTransport::onSocketConnected);
        connect(m_socket, &QTcpSocket::disconnected, this, &TcpTransport::onSocketDisconnected);
        connect(m_socket, &QTcpSocket::readyRead, this, &TcpTransport::onSocketReadyRead);
        connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
                this, &TcpTransport::onSocketError);

        m_reconnectTimer->setSingleShot(true);
        connect(m_reconnectTimer, &QTimer::timeout, this, &TcpTransport::onReconnectTimer);
    }

    TcpTransport::~TcpTransport()
    {
        disconnectFromServer();
    }

    void TcpTransport::setConfig(const Config &config)
    {
        m_config = config;
    }

    void TcpTransport::connectToServer()
    {
        if (m_state == LinkState::Connected || m_state == LinkState::Connecting)
        {
            return;
        }

        setState(LinkState::Connecting);
        m_socket->connectToHost(m_config.host, m_config.port);
    }

    void TcpTransport::disconnectFromServer()
    {
        stopReconnectTimer();
        if (m_socket->state() != QAbstractSocket::UnconnectedState)
        {
            m_socket->disconnectFromHost();
        }
        m_receiveBuffer.clear();
        setState(LinkState::Disconnected);
    }

    bool TcpTransport::isConnected() const
    {
        return m_socket->state() == QAbstractSocket::ConnectedState;
    }

    bool TcpTransport::send(const QByteArray &data)
    {
        return send(reinterpret_cast<const uint8_t *>(data.constData()), data.size());
    }

    bool TcpTransport::send(const uint8_t *data, size_t len)
    {
        if (!isConnected())
        {
            qWarning() << "TcpTransport: Cannot send, not connected";
            return false;
        }

        qint64 written = m_socket->write(reinterpret_cast<const char *>(data), len);
        if (written != static_cast<qint64>(len))
        {
            qWarning() << "TcpTransport: Write error, wrote" << written << "of" << len << "bytes";
            return false;
        }

        m_socket->flush();
        return true;
    }

    QString TcpTransport::errorString() const
    {
        return m_socket->errorString();
    }

    void TcpTransport::onSocketConnected()
    {
        setState(LinkState::Connected);
        emit connected();
    }

    void TcpTransport::onSocketDisconnected()
    {
        setState(LinkState::Disconnected);
        m_receiveBuffer.clear();
        emit disconnected();

        // 自动重连
        if (m_config.autoReconnect)
        {
            startReconnectTimer();
        }
    }

    void TcpTransport::onSocketReadyRead()
    {
        m_receiveBuffer.append(m_socket->readAll());
        processReceivedData();
    }

    void TcpTransport::onSocketError(QAbstractSocket::SocketError error)
    {
        Q_UNUSED(error)
        QString err = m_socket->errorString();
        qWarning() << "TcpTransport: Socket error:" << err;
        setState(LinkState::Disconnected); // 改为Disconnected状态以便重连
        emit errorOccurred(err);

        // 连接失败后自动重连
        if (m_config.autoReconnect)
        {
            startReconnectTimer();
        }
    }

    void TcpTransport::onReconnectTimer()
    {
        if (m_state == LinkState::Disconnected)
        {
            qDebug() << "TcpTransport: Attempting reconnect...";
            connectToServer();
        }
    }

    void TcpTransport::setState(LinkState state)
    {
        if (m_state != state)
        {
            m_state = state;
            emit stateChanged(state);
        }
    }

    void TcpTransport::startReconnectTimer()
    {
        if (m_config.reconnectInterval > 0)
        {
            m_reconnectTimer->start(m_config.reconnectInterval);
        }
    }

    void TcpTransport::stopReconnectTimer()
    {
        if (m_reconnectTimer->isActive())
        {
            m_reconnectTimer->stop();
        }
    }

    void TcpTransport::processReceivedData()
    {
        // 南网规范帧格式 (参照104标准):
        // 68H | 长度L(2字节) | APCI(4字节) | ASDU
        // 无校验和，无结束字节
        
        // 检查是否有完整帧 (最小帧: 启动(1) + 长度(2) + APCI(4) = 7字节)
        while (m_receiveBuffer.size() >= 7)
        {
            // 检查帧起始字节
            if (static_cast<uint8_t>(m_receiveBuffer[0]) != FRAME_START_BYTE)
            {
                qWarning() << "TcpTransport: Invalid frame start byte, discarding";
                m_receiveBuffer.remove(0, 1);
                continue;
            }

            // 获取帧长度 (APCI + ASDU)
            uint16_t frameLen = static_cast<uint8_t>(m_receiveBuffer[1]) |
                                (static_cast<uint8_t>(m_receiveBuffer[2]) << 8);
            // 总长度 = 启动(1) + 长度(2) + APCI+ASDU(frameLen) = 3 + frameLen
            int totalLen = 3 + frameLen;

            // 检查是否收到完整帧
            if (m_receiveBuffer.size() < totalLen)
            {
                break; // 数据不完整，等待更多数据
            }

            // 提取完整帧
            QByteArray frame = m_receiveBuffer.left(totalLen);
            m_receiveBuffer.remove(0, totalLen);

            // 发送帧数据
            emit dataReceived(frame);
        }
    }

} // namespace IEC103
