#include <string.h>

void encodeBulkString(char *destination, int size, char *source, int length)
{
	/** FORMAT:
	 * "$<length>\r\n<data>\r\n"
	 */

	snprintf(
			destination,
			size,
			"$%u\r\n%s\r\n",
			length,
			source);
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
}
