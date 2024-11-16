#include <stdlib.h>
#include <string.h>
#include "TimeParser.h"
#include <ctype.h>

// time format: HHMMSS (6 characters)
int time_parse(char *time) {

	// how many seconds, default returns error
	int seconds = TIME_LEN_ERROR;

	// TODO: Check that string is not null
	if (time == NULL) {
		return TIME_ARRAY_ERROR;
	}
	// Check that string length is correct
	else if (strlen(time) != 6) {
		return TIME_LEN_ERROR;
	}
	// Check that all characters are digits
	if (!isdigit(time[0]) || !isdigit(time[1]) || !isdigit(time[2])) {
		return TIME_DIGIT_ERROR;
	}

	// Parse values from time string
	// For example: 124033 -> 12hour 40min 33sec
    int values[3];
	values[2] = atoi(time+4); // seconds
	time[4] = 0;
	values[1] = atoi(time+2); // minutes
	time[2] = 0;
	values[0] = atoi(time); // hours
	// Now you have:
	// values[0] hour
	// values[1] minute
	// values[2] second

	// TODO: Add boundary check time values: below zero or above limit not allowed
	// limits are 59 for minutes, 23 for hours, etc
	if (values[0] < 0 || values[0] > 23) {
		return TIME_VALUE_ERROR;
	}
	if (values[1] < 0 || values[1] > 59) {
		return TIME_VALUE_ERROR;
	}
	if (values[2] < 0 || values[2] > 59) {
		return TIME_VALUE_ERROR;
	}

	// TODO: Calculate return value from the parsed minutes and seconds
	// Otherwise error will be returned!
	// seconds = ...
	seconds = values[0] * 3600 + values[1] * 60 + values[2];

	// Check that calculated time is not 0 or negative
	if (seconds <= 0) {
		return TIME_ZERO_ERROR;
	}


	return seconds;
}
