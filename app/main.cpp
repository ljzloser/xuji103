#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QDebug>
#include "Config.h"
#include "DataPrinter.h"
#include "iec103/IEC103Master.h"

using namespace IEC103;

class Application : public QObject {
    Q_OBJECT

public:
    Application(QObject* parent = nullptr)
        : QObject(parent)
        , m_master(new IEC103Master(this))
        , m_printer(new DataPrinter(this))
    {
        m_master->setDataHandler(m_printer);
        
        connect(m_master, &IEC103Master::connected, this, &Application::onConnected);
        connect(m_master, &IEC103Master::disconnected, this, &Application::onDisconnected);
        connect(m_master, &IEC103Master::errorOccurred, this, &Application::onError);
        connect(m_master, &IEC103Master::giFinished, this, &Application::onGIFinished);
    }

    bool init(const QStringList& args) {
        if (!Config::instance().parseArgs(args)) {
            return false;
        }
        
        IEC103Master::Config config;
        config.host = Config::instance().host();
        config.port = Config::instance().port();
        config.reconnectInterval = Config::instance().reconnectInterval();
        config.timeout = Config::instance().timeout();
        config.testInterval = Config::instance().testInterval();
        
        m_master->setConfig(config);
        return true;
    }

    void start() {
        qInfo() << "Connecting to" << Config::instance().host() 
                << ":" << Config::instance().port();
        m_master->connectToServer();
    }

    void stop()
    {
        qInfo() << "Disconnecting...";
        m_master->disconnectFromServer();
    }
private slots:
    void onConnected() {
        qInfo() << "Connected! Starting general interrogation...";
        
        // 启动周期性总召唤
        int giInterval = Config::instance().giInterval();
        if (giInterval > 0) {
            m_master->startPeriodicGI(giInterval * 1000);
        }
        
        // 立即执行一次总召唤
        m_master->generalInterrogation(0);
    }

    void onDisconnected() {
        qInfo() << "Disconnected";
        if (!Config::instance().autoReconnect()) {
            QCoreApplication::quit();
        }
    }

    void onError(const QString& error) {
        qWarning() << "Error:" << error;
    }

    void onGIFinished(uint16_t deviceAddr) {
        qInfo() << "GI finished for device" << deviceAddr;
        
        // GI完成后，召唤遥测数据
        readAnalogGroups(deviceAddr);
        
        // 召唤遥脉数据
        readCounterGroups(deviceAddr);
    }

    void readAnalogGroups(uint16_t deviceAddr) {
        // 召唤遥测组
        for (int group : Config::instance().analogGroups()) {
            qInfo() << "Reading analog group" << group;
            m_master->readGenericGroup(deviceAddr, group);
        }
    }
    
    void readCounterGroups(uint16_t deviceAddr) {
        // 召唤遥脉组
        for (int group : Config::instance().counterGroups()) {
            qInfo() << "Reading counter group" << group;
            m_master->readGenericGroup(deviceAddr, group);
        }
    }

private:
    IEC103Master* m_master;
    DataPrinter *m_printer;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("xuji103");
    QCoreApplication::setApplicationVersion("1.0.0");

    Application application;
    QTimer timer;

    if (!application.init(app.arguments()))
    {
        return 1;
    }
    auto quit = [&]()
    {
        application.stop();
        app.quit(); };
    QObject::connect(&timer, &QTimer::timeout, quit);
    application.start();
    timer.start(10000);
    return app.exec();
}

#include "main.moc"
