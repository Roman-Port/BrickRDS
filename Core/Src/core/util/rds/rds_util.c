/* RDS utilities */

#include <core/util/rds.h>

/*
 * Copies an RDS field to destination for display, fltering illegal chars and adding a terminator.
 * src: Source to copy from
 * dst: Destination to copy to
 * len: The length of items to copy; Destination should be len+1, as a null terminator is written.
 */
void copy_display_rds_field(const char* src, char* dst, int len) {
	//Copy characters, filtering illegal ones
	for (int i = 0; i < len; i++) {
		//Copy anything >= ' '
		if (src[i] >= ' ')
			dst[i] = src[i];
		else
			dst[i] = ' ';
	}

	//Set terminator
	dst[len] = 0;
}

static int floor_to_value(int input, int divisor, int* divided) {
	input /= divisor;
	*divided = input;
	input *= divisor;
	return input;
}

static int floor_to_divisor(int input) {
	int remainder = input % 10000;
	return input - remainder;
}

/*
 * Utility function to convert from timestamp to a date/time.
 */
rds_datetime_t rds_convert_timestamp(int timestamp) {
	//Timestamp is expressed as combined date+time; Split
	int date = timestamp / (24 * 60);
	int time = timestamp % (24 * 60);

	//Convert date to year/month/day
	/*int year = (int)((date - 15078.2) / 365.25);
	int month = (int)((date - 14956.1 - ((int)(year * 365.25))) / 30.6001);
	int day = date - 14956 - ((int)(year * 365.25)) - ((int)(month * 30.6001));
	month--;
	if (month == 14 || month == 15) {
		year++;
		month -= 12;
	}*/

	//Since the STM32 has no FPU (and doing the above function is not only slow but eats 3 kB of flash) it has been implemented using integers, scaled by 10000.
	//This has been tested to be equivalent to the reference function above up to 1^17 without overflowing.
	int dateScaled = date * 10000;
	int year;
	int month;
	int yearScaled = floor_to_value(dateScaled - 150782000, 3652500, &year);
	int monthScaled = floor_to_value(dateScaled - 149561000 - floor_to_divisor(yearScaled), 306001, &month);
	int day = (dateScaled - 149560000 - (floor_to_divisor(yearScaled)) - (floor_to_divisor(monthScaled))) / 10000;
	month--;
	if (month == 14 || month == 15) {
		year++;
		month -= 12;
	}

	//Convert to hours/minutes
	int hour = time / 60;
	int min = time % 60;

	//Create response
	rds_datetime_t result = {
			.year = year + 1900,
			.month = month,
			.day = day,
			.hour = hour,
			.minute = min
	};
	return result;
}

/*
 * Calculates a percentage of BER between 0-max
 */
int rds_calc_ber_percent(int max) {
	//Get BER
	int rdsTotalBits;
	int rdsErrorBits;
	rds_calc_ber(&rdsTotalBits, &rdsErrorBits);

	//Divide to get a ratio 0-100
	if (rdsTotalBits)
		return (rdsErrorBits * max) / rdsTotalBits;
	return 100;
}
