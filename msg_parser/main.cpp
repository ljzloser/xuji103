#include <QApplication>
#include "MsgParserWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    MsgParserWindow window;
    window.show();

    // 如果命令行指定了文件，自动加载
    if (argc > 1) {
        window.loadFile(QString::fromLocal8Bit(argv[1]));
    }

    return app.exec();
}
