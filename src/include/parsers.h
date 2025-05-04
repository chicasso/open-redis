#include <string.h>

char **parseEncodedArray(char *request, int request_size, int *N)
{
	/** FORMAT: of Redis Request => * 2 \r\n $ 4 \r\n ECHO \r\n $ 9 \r\n raspberry \r\n
	 *  													 |     |  | |        |                					|
	 * 										(*) starting CRLF \ \    Content 									Ending CRLF
	 * 																			 \ \___ Length of next item
	 * 																			  \___ '$' Used before the length of next item
	 */

	int numberOfCommands = 0;
	int index = 1;

	while (index <= request_size && request[index] >= (int)'0' && request[index] <= (int)'9')
	{
		numberOfCommands *= 10;
		numberOfCommands += (int)request[index] - (int)'0';
		index += 1;
	}

	char **commands = (char **)malloc(numberOfCommands);
	*N = numberOfCommands;

	int commandIndex = 0;

	for (int idx = index + 2; idx < request_size; idx++) // +2 to skip first CRLF
	{
		int length = 0;
		
		if (request[idx] == '$') // next numeric bytes will containt the number of bytes of the content
		{
			idx += 1;
			while (request[idx] >= (int)'0' && request[idx] <= (int)'9')
			{
				length *= 10;
				length += (int)request[idx] - (int)'0';
				idx += 1;
			}

			idx += 2; // +2 to skip CRLF after size
		}

		char *command = (char *)malloc(length + 1);
		int commandCharIndex = 0;

		while (idx < request_size && request[idx] != '\r')
		{
			command[commandCharIndex] = request[idx];
			commandCharIndex += 1;
			idx += 1;
		}
		command[length] = '\0';
		idx += 1; // +1 and +1 in forloop to skip CRLF after size

		commands[commandIndex] = command;
		commandIndex += 1;
	}

	int firstElementIndex = 0;
	while (firstElementIndex < strlen(commands[0]))
	{
		commands[0][firstElementIndex] = toupper(commands[0][firstElementIndex]);
		firstElementIndex += 1;
	}

	return commands;
}
