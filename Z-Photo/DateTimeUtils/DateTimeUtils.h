#ifndef DATETIMEUTILS_H
#define DATETIMEUTILS_H

#include <ctime>

/*
	提供 UTC时间戳转换为日期 和 日期转换为UTC时间戳 两种方法
	
	所有主流操作系统（Windows、Linux、macOS）文件的时间保存的是 标准UTC时间
	
	在查看时，系统自动将时间转换为本地时区时间
*/

/**
 * 将本地日期（年、月、日）转换为 标准UTC 秒级时间戳。
 * 输入日期被视为本地时区的当天 00:00:00，返回对应的 UTC 时间戳。
 *
 * @param year   年份（如 2026）
 * @param month  月份（1-12）
 * @param day    日期（1-31）
 * @param start  输出当天 00:00:00（本地时间）对应的 UTC 时间戳
 * @param end    输出当天 23:59:59（本地时间）对应的 UTC 时间戳
 * @return true  转换成功，false 日期无效
 */
bool dateToUtcTimestamp(int year, int month, int day, time_t& start, time_t& end);

/**
 * 将 UTC 秒级时间戳转换为 标准UTC 日期。 年月日
 *
 * @param timestamp  UTC 时间戳（秒）
 * @param year       输出年份（如 2026）
 * @param month      输出月份（1-12）
 * @param day        输出日期（1-31）
 * @return true      转换成功，false 时间戳无效
 */
bool timestampToUtcDate(time_t timestamp, int& year, int& month, int& day);

/**
 * 将 UTC 秒级时间戳转换为 标准UTC 日期，存在时区问题。年月日时分秒
 *
 * @param timestamp  UTC 时间戳（秒）
 * @param year       输出年份（如 2026）
 * @param month      输出月份（1-12）
 * @param day        输出日期（1-31）
 * @param hour       输出小时
 * @param minute     输出分钟
 * @param second     输出秒
 * @return true      转换成功，false 时间戳无效
 */
bool timestampToUtcDateTime(time_t timestamp, int& year, int& month, int& day,
                            int& hour, int& minute, int& second);
							
/**
 * 将 UTC 秒级时间戳转换为 本地时区 日期。年月日时分秒
 *
 * @param timestamp  UTC 时间戳（秒）
 * @param year       输出年份（如 2026）
 * @param month      输出月份（1-12）
 * @param day        输出日期（1-31）
 * @param hour       输出小时
 * @param minute     输出分钟
 * @param second     输出秒
 * @return true      转换成功，false 时间戳无效
 */
bool timestampToLocalDateTime(time_t timestamp, int& year, int& month, int& day,
                              int& hour, int& minute, int& second);
#endif // DATETIMEUTILS_H