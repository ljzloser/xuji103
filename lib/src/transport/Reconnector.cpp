#include "Reconnector.h"
#include <QTimer>

namespace IEC103 {

Reconnector::Reconnector(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_enabled(false)
    , m_interval(5000)
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &Reconnector::onTimeout);
}

void Reconnector::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        m_timer->stop();
    }
}

void Reconnector::setInterval(int ms)
{
    m_interval = ms;
}

void Reconnector::start()
{
    if (m_enabled && m_interval > 0) {
        m_timer->start(m_interval);
    }
}

void Reconnector::stop()
{
    m_timer->stop();
}

void Reconnector::onTimeout()
{
    emit reconnect();
}

} // namespace IEC103
