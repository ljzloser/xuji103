#ifndef IEC103_TCPTRANSPORT_H
#define IEC103_TCPTRANSPORT_H

#include "../types/Types.h"
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>

namespace IEC103 {

// TCP传输层
class TcpTransport : public QObject {
    Q_OBJECT

public:
    struct Config {
        QString host;
        quint16 port = 2404;
        int connectTimeout = 30000;
        int readTimeout = 15000;
        int reconnectInterval = 5000;
        bool autoReconnect = true;
    };

    explicit TcpTransport(QObject* parent = nullptr);
    ~TcpTransport() override;

    void setConfig(const Config& config);
    const Config& config() const { return m_config; }

    void connectToServer();
    void disconnectFromServer();
    bool isConnected() const;

    bool send(const QByteArray& data);
    bool send(const uint8_t* data, size_t len);

    LinkState state() const { return m_state; }
    QString errorString() const;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& error);
    void stateChanged(LinkState state);
    void dataReceived(const QByteArray& data);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReconnectTimer();

private:
    void setState(LinkState state);
    void startReconnectTimer();
    void stopReconnectTimer();
    void processReceivedData();

    Config m_config;
    QTcpSocket* m_socket;
    QTimer* m_reconnectTimer;
    QByteArray m_receiveBuffer;
    LinkState m_state = LinkState::Disconnected;
};

} // namespace IEC103

#endif // IEC103_TCPTRANSPORT_H
