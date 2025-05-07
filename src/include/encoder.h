#pragma once

#include <string.h>
#include <ctype.h>
#include "utils.h"
#include "globals.h"

void encodeBulkString(char *destination, int size, char *source, int length)
{
	/** FORMAT:
	 * "$<length>\r\n<data>\r\n"
	 */

	snprintf(
			destination,
			size,
			"$%d\r\n%s\r\n",
			length,
			source);
}

void encodeNumber(char *destination, int number)
{
	/** FORMAT:
	 * ":<number>\r\n"
	 */

	snprintf(
			destination,
			64,
			":%d\r\n",
			number);
}

void encodeSimpleString(char *destination, int size, char *source)
{
	/** FORMAT:
	 * "+OK\r\n"
	 */

	snprintf(
			destination,
			size,
			"+%s\r\n",
			source);
}

void encodeStringArray(char *destination, char **source, int length)
{
	/** FORMAT: of Redis Request => * 2 \r\n $ 4 \r\n ECHO \r\n $ 9 \r\n raspberry \r\n
	 *  													 |     |  | |        |                					|
	 * 										(*) starting CRLF \ \    Content 									Ending CRLF
	 * 																			 \ \___ Length of next item
	 * 																			  \___ '$' Used before the length of next item
	 */

	if (destination == NULL)
	{
		printf("Destination should be not NULL!\n");
		return;
	}

	int lengthTemp = length;

	int destinationIndex = 0;

	destination[destinationIndex++] = '*';

	while (lengthTemp > 9)
	{
		// Algorithm to get the last digit from a number
		// N = 199
		// digits - 1 = 2
		// 10^2 = 100
		//
		// 199 - (199 % 100) => 100
		// first digit => 100 / 100 => 1
		//
		// N = (N % 100) => 99
		//
		// second digit
		//
		// 99 - (99 % 10) => 90
		// first digit => 90 / 10 => 9
		//
		// so on...
		int digits = calculateDigits(lengthTemp) - 1;
		int tenPow = calculateExponent(10, digits);

		int trailingN_1 = lengthTemp - (lengthTemp % tenPow);
		int firstDigit = trailingN_1 / tenPow;

		printf("ItoC [1]: %d -> %c\n", firstDigit, (char)(firstDigit + 48));
		destination[destinationIndex++] = (char)(firstDigit + 48);

		lengthTemp = (lengthTemp % tenPow);
	}

	destination[destinationIndex++] = (char)(lengthTemp + 48);
	destination[destinationIndex++] = '\r';
	destination[destinationIndex++] = '\n';

	for (int idx = 0; idx < length; idx++)
	{
		if (source == NULL)
		{
			printf("Source is NULL!\n");
			break;
		}

		destination[destinationIndex++] = '$';

		char *info = source[idx];
		lengthTemp = strlen(info);

		while (lengthTemp > 9)
		{
			int digits = calculateDigits(lengthTemp) - 1;
			int tenPow = calculateExponent(10, digits);

			int trailingN_1 = lengthTemp - (lengthTemp % tenPow);
			int firstDigit = trailingN_1 / tenPow;

			printf("ItoC [2]: %d -> %c\n", firstDigit, (char)(firstDigit + 48));

			destination[destinationIndex++] = (char)(firstDigit + 48);
			lengthTemp = (lengthTemp % tenPow);
		}

		destination[destinationIndex++] = (char)(lengthTemp + 48);
		destination[destinationIndex++] = '\r';
		destination[destinationIndex++] = '\n';

		for (int infoIndex = 0; infoIndex < strlen(info); infoIndex++)
		{
			destination[destinationIndex++] = info[infoIndex];
		}

		destination[destinationIndex++] = '\r';
		destination[destinationIndex++] = '\n';
	}

	destination[destinationIndex] = '\0';
}
