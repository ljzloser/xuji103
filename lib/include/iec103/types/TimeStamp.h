#ifndef IEC103_TIMESTAMP_H
#define IEC103_TIMESTAMP_H

#include <QtGlobal>
#include <QDateTime>
#include <QDebug>

namespace IEC103 {

#pragma pack(push, 1)

// CP56Time2a - 七个八位位组的二进制时间
struct CP56Time2a {
    uint16_t millisecond;  // 毫秒 (0-59999)
    uint8_t minute : 6;    // 分钟 (0-59)
    uint8_t iv : 1;        // 无效标志
    uint8_t res1 : 1;      // 备用
    uint8_t hour : 5;      // 小时 (0-23)
    uint8_t su : 1;        // 夏令时
    uint8_t res2 : 2;      // 备用
    uint8_t day : 5;       // 日期 (1-31)
    uint8_t weekday : 3;   // 星期 (1-7, 0=未用)
    uint8_t month : 4;     // 月份 (1-12)
    uint8_t res3 : 4;      // 备用
    uint8_t year;          // 年 (0-99, 表示2000-2099)

    // 转换为QDateTime
    QDateTime toDateTime() const {
        int y = (year >= 100) ? year : 2000 + year;
        int m = (month >= 1 && month <= 12) ? month : 1;
        int d = (day >= 1 && day <= 31) ? day : 1;
        int h = (hour <= 23) ? hour : 0;
        int min = (minute <= 59) ? minute : 0;
        int ms = (millisecond <= 59999) ? millisecond : 0;

        return QDateTime(QDate(y, m, d), QTime(h, min, 0, ms));
    }

    // 从QDateTime创建
    static CP56Time2a fromDateTime(const QDateTime& dt) {
        CP56Time2a ts;
        ts.millisecond = dt.time().msec() + dt.time().second() * 1000;
        ts.minute = dt.time().minute();
        ts.iv = 0;
        ts.res1 = 0;
        ts.hour = dt.time().hour();
        ts.su = 0;
        ts.res2 = 0;
        ts.day = dt.date().day();
        ts.weekday = dt.date().dayOfWeek(); // 1=Monday, 7=Sunday
        ts.month = dt.date().month();
        ts.res3 = 0;
        ts.year = dt.date().year() % 100;
        return ts;
    }

    // 当前时间
    static CP56Time2a now() {
        return fromDateTime(QDateTime::currentDateTime());
    }

    // 是否无效
    bool isInvalid() const {
        return iv != 0;
    }

    // 设置无效标志
    void setInvalid(bool invalid = true) {
        iv = invalid ? 1 : 0;
    }

    // 格式化字符串
    QString toString() const {
        return toDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    }
};

// CP32Time2a - 四个八位位组的二进制时间 (用于ASDU内部时标)
struct CP32Time2a {
    uint16_t millisecond;  // 毫秒 (0-59999)
    uint8_t minute : 6;    // 分钟 (0-59)
    uint8_t iv : 1;        // 无效标志
    uint8_t res1 : 1;      // 备用
    uint8_t hour : 5;      // 小时 (0-23)
    uint8_t su : 1;        // 夏令时
    uint8_t res2 : 2;      // 备用

    QDateTime toDateTime(const QDate& date = QDate::currentDate()) const {
        int h = (hour <= 23) ? hour : 0;
        int min = (minute <= 59) ? minute : 0;
        int ms = (millisecond <= 59999) ? millisecond : 0;
        return QDateTime(date, QTime(h, min, 0, ms));
    }

    static CP32Time2a fromDateTime(const QDateTime& dt) {
        CP32Time2a ts;
        ts.millisecond = dt.time().msec() + dt.time().second() * 1000;
        ts.minute = dt.time().minute();
        ts.iv = 0;
        ts.res1 = 0;
        ts.hour = dt.time().hour();
        ts.su = 0;
        ts.res2 = 0;
        return ts;
    }
};

#pragma pack(pop)

// 确保结构体大小正确
static_assert(sizeof(CP56Time2a) == 7, "CP56Time2a must be 7 bytes");
static_assert(sizeof(CP32Time2a) == 4, "CP32Time2a must be 4 bytes");

// 调试输出
inline QDebug operator<<(QDebug dbg, const CP56Time2a& ts) {
    dbg.nospace() << "CP56Time2a(" << ts.toString() << (ts.iv ? " IV" : "") << ")";
    return dbg;
}

}

#endif // IEC103_TIMESTAMP_H
