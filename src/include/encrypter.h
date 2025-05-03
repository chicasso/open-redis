#include <string.h>

void encodeBulkString(char *destination, int size, char *source, int length)
{
	/** FORMAT:
	 * $<length>\r\n<data>\r\n
	 */

	snprintf(
			destination,
			size,
			"$%u\r\n%s\r\n",
			length,
			source);
}
