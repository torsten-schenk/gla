#include <cstring>
#include <ctime>
#include <iostream>
#include "date.h"
#include "date_interface.h"

using namespace date;
using namespace std::chrono;

extern "C" {
	int64_t date_to_epoch(const date_time *dt) {
		int y = dt->year;
		unsigned int m = dt->month;
		unsigned int d = dt->day;
		return (sys_days(year{y}/month{m}/day{d}) - sys_days(0000_y/01/01)).count();
	}

	void date_from_epoch(int64_t offset, date_time *dt) {
//		auto correct = offset < 0 ? -1 : 0;
		auto tmp = year_month_day{sys_days(0000_y/01/01) + days(offset)};
		dt->year = static_cast<int>(tmp.year());
		dt->month = static_cast<unsigned int>(tmp.month());
		dt->day = static_cast<unsigned int>(tmp.day());
	}

	int64_t date_today(date_time *dt) {
		time_t t = time(0);
		struct tm *now = localtime(&t);
		date_time tmp;
		tmp.year = now->tm_year + 1900;
		tmp.month = now->tm_mon + 1;
		tmp.day = now->tm_mday;
		if(dt != nullptr)
			memcpy(dt, &tmp, sizeof(date_time));
		return date_to_epoch(&tmp);
	}

/*	int64_t time_to_epoch(const apr_time_exp_t *exp) { //TODO fix or leave it and use something else for time
		int y = exp->tm_year;
		unsigned int m = exp->tm_mon + 1;
		unsigned int d = exp->tm_mday;
		int64_t toffset = 0;

		auto epochd = sys_days(year{y}/month{m}/day{d}) - sys_days(1970_y/01/01);
		toffset += exp->tm_hour * 3600000;
		toffset += exp->tm_min * 60000;
		toffset += exp->tm_sec * 1000;
		toffset += exp->tm_usec;
		return static_cast<microseconds>(epochd).count() + toffset;
	}

	void time_from_epoch(int64_t offset, apr_time_exp_t *exp) {
		auto doffset = offset / static_cast<microseconds>(days(1)).count();
		auto toffset = offset % static_cast<microseconds>(days(1)).count();
		if(offset < 0) {
			doffset -= 1;
			toffset += static_cast<microseconds>(days(1)).count();
		}
		date_from_epoch(doffset, exp);
		exp->tm_hour = toffset / 3600000;
		toffset %= 3600000;
		exp->tm_min = toffset / 60000;
		toffset %= 60000;
		exp->tm_sec = toffset / 1000;
		toffset %= 1000;
		exp->tm_usec = toffset;
	}*/
}

/*int main() {
	date_time dt;
	int64_t today = date_today(&dt);
	printf("TODAY: %d\n", (int)today);

	return 0;
}*/

/*int main() {
	apr_time_exp_t exp;
	exp.tm_year = 1800;
	exp.tm_mon = 11;
	exp.tm_mday = 31;
	exp.tm_hour = 23;
	exp.tm_min = 59;
	exp.tm_sec = 3;
	exp.tm_usec = 4;

	auto asd = date_to_epoch(&exp);

	auto tasd = time_to_epoch(&exp);
	printf("IS: %ld %ld\n", asd, tasd);

	time_from_epoch(tasd, &exp);
	printf("GOT: %d %d %d  %d %d %d %d\n", exp.tm_year, exp.tm_mon, exp.tm_mday, exp.tm_hour, exp.tm_min, exp.tm_sec, exp.tm_usec);
	return 0;
}*/
/*int main_test() {
	auto d = 2016_y/01/01;
	auto a = 0000_y/01/01;
	auto delta = sys_days(d) - sys_days(a);
	std::cout << d << "  " << delta.count() << "\n";

//	printf("IS: %d\n", date_to_epoch(2016, 6, 1));

	int y;
	unsigned int mon;
	unsigned int mday;

//	date_from_epoch(736481, &y, &mon, &mday);
//	printf("GOT: %d %d %d\n", y, mon, mday);

	return 0;
}
*/
