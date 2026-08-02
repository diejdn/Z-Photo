#include "DateTimeUtils.h"
#include <iostream>
#include <ctime>
#include <cassert>

void printLocalTime(time_t ts) {
    struct tm* local = localtime(&ts);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", local);
    std::cout << "  Local time: " << buf << std::endl;
}

int main() {
	
	int y, m, d;
	time_t ts = 1664829438;
    timestampToUtcDate(ts, y, m, d);
	time_t start, end;
    dateToUtcTimestamp(y, m, d, start, end);
	std::cout << "year: " << y << " m: "<< m << " d: " << d << " time_t: " << start << std::endl;  
  	 
    return 0;
}