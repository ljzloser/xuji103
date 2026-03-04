#ifndef IEC103_RECONNECTOR_H
#define IEC103_RECONNECTOR_H

#include <QObject>

class QTimer;

namespace IEC103 {

class Reconnector : public QObject {
    Q_OBJECT

public:
    explicit Reconnector(QObject* parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void setInterval(int ms);
    int interval() const { return m_interval; }

    void start();
    void stop();

signals:
    void reconnect();

private slots:
    void onTimeout();

private:
    QTimer* m_timer;
    bool m_enabled;
    int m_interval;
};

} // namespace IEC103

#endif // IEC103_RECONNECTOR_H
