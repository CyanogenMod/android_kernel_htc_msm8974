
#include "isofs.h"


int iso_date(char * p, int flag)
{
	int year, month, day, hour, minute, second, tz;
	int crtime, days, i;

	year = p[0] - 70;
	month = p[1];
	day = p[2];
	hour = p[3];
	minute = p[4];
	second = p[5];
	if (flag == 0) tz = p[6]; 
	else tz = 0;
	
	if (year < 0) {
		crtime = 0;
	} else {
		int monlen[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

		days = year * 365;
		if (year > 2)
			days += (year+1) / 4;
		for (i = 1; i < month; i++)
			days += monlen[i-1];
		if (((year+2) % 4) == 0 && month > 2)
			days++;
		days += day - 1;
		crtime = ((((days * 24) + hour) * 60 + minute) * 60)
			+ second;

		
		if (tz & 0x80)
			tz |= (-1 << 8);
		
		if (-52 <= tz && tz <= 52)
			crtime -= tz * 15 * 60;
	}
	return crtime;
}		
