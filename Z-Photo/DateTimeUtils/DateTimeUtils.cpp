#include "DateTimeUtils.h"
#include <cstring>

// time_t 大多数现代 64 位系统（Windows、Linux）上被定义为 64 位有符号整数（long long)
// 提供某日，转换出该日 0时 和 23时59分59秒 的时间戳。且完成 本地时间 到 标准时间 的转换
bool dateToUtcTimestamp(int year, int month, int day, time_t& start, time_t& end)
{
    struct tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon  = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = 0;
    tm.tm_min  = 0;
    tm.tm_sec  = 0;
    tm.tm_isdst = -1;   // 让系统自动判断夏令时

	// mktime 是将传入的 struct tm 视为“本地时间（Local Time）” 转换为标准UTC时间戳
    start = mktime(&tm);
    if (start == -1) {
        return false;
    }
    // 当天最后一秒（UTC+0）
    end = start + 86400 - 1;
    return true;
}

// 根据 UTC时间戳，转换出年月日
bool timestampToUtcDate(time_t timestamp, int& year, int& month, int& day)
{
    struct tm tm_buf;
#ifdef _WIN32
    if (gmtime_s(&tm_buf, &timestamp) != 0) {
        return false;
    }
#else
    if (gmtime_r(&timestamp, &tm_buf) == nullptr) {
        return false;
    }
#endif
    year  = tm_buf.tm_year + 1900;
    month = tm_buf.tm_mon + 1;
    day   = tm_buf.tm_mday;
    return true;
}

// 根据 UTC时间戳，转换出标准 年月日时分秒
bool timestampToUtcDateTime(time_t timestamp, int& year, int& month, int& day,
                            int& hour, int& minute, int& second)
{
    struct tm tm_buf;
#ifdef _WIN32
    if (gmtime_s(&tm_buf, &timestamp) != 0) {
        return false;
    }
#else
    if (gmtime_r(&timestamp, &tm_buf) == nullptr) {
        return false;
    }
#endif
    year   = tm_buf.tm_year + 1900;
    month  = tm_buf.tm_mon + 1;
    day    = tm_buf.tm_mday;
    hour   = tm_buf.tm_hour;
    minute = tm_buf.tm_min;
    second = tm_buf.tm_sec;
    return true;
}

// 根据 UTC时间戳，转换出本地时区 年月日时分秒
bool timestampToLocalDateTime(time_t timestamp, int& year, int& month, int& day,
                              int& hour, int& minute, int& second)
{
    struct tm tm_buf;
#ifdef _WIN32
    if (localtime_s(&tm_buf, &timestamp) != 0) {
        return false;
    }
#else
    if (localtime_r(&timestamp, &tm_buf) == nullptr) {
        return false;
    }
#endif
    year   = tm_buf.tm_year + 1900;
    month  = tm_buf.tm_mon + 1;
    day    = tm_buf.tm_mday;
    hour   = tm_buf.tm_hour;
    minute = tm_buf.tm_min;
    second = tm_buf.tm_sec;
    return true;
}

