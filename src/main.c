#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>

#include "include/globals.h"
#include "include/parsers.h"
#include "include/encoder.h"
#include "include/utils.h"

static int queueing = -1;

struct HashEntry *hash_table[TABLE_SIZE];
struct TransactionQueue *transactionQueue = NULL;
struct TransactionQueue *transactionQueueTail = NULL;

struct RDBConfig
{
	char *fileName;
	char *path;
};

struct RDBConfig *config;

void *handle_client(void *arg)
{
	struct client_info *info = (struct client_info *)arg;
	int client_fd = info->client_fd;
	free(info);

	while (1)
	{
		char request[4096];
		char response[1024];
		char default_response[] = "PONG";

		int bytes_read = recv(client_fd, request, sizeof(request), 0);

		// Printing request
		printf("Request: %s\n", request);

		if (bytes_read == 0)
		{
			printf("Client disconnected\n");
			break;
		}
		else if (bytes_read < 0)
		{
			printf("Error reading from client: %s\n", strerror(errno));
			break;
		}

		request[bytes_read] = '\0';

		char **commands;
		int numberOfCommands = 0;

		// Just logging commands for debugging
		for (int idx = 0; idx < numberOfCommands; idx++)
		{
			printf("[Command %d] %s\n", idx + 1, commands[idx]);
		}

		if (request[0] == '*')
		{
			commands = parseEncodedArray(request, bytes_read, &numberOfCommands);
		}

		if (commands == NULL || numberOfCommands == 0)
		{
			printf("No commands sent from client!\n");
		}

		if (compare(commands[0], "ECHO") == 0)
		{
			// ECHO command
			if (numberOfCommands != 2)
			{
				printf("[WARN] One agrument for 'ECHO' is required, got %d!\n", numberOfCommands);
			}
			encodeBulkString(response, sizeof(response), commands[1], strlen(commands[1]));
			send(client_fd, response, strlen(response), 0);
		}
		else if (compare(commands[0], "SET") == 0)
		{ // SET command
			if (numberOfCommands != 3)
			{
				printf("[WARN] Two agrument for 'SET' are required, got %d!\n", numberOfCommands);
			}

			long long expiry = -1;

			if (numberOfCommands == 5)
			{
				expiry = 0;
				int pxIndex = -1;

				for (int idx = 0; idx < numberOfCommands; idx++)
				{
					if (compare(commands[idx], "px") == 0)
					{
						pxIndex = idx;
						break;
					}
				}

				if (pxIndex != -1)
				{
					for (int idx = 0; idx < strlen(commands[pxIndex + 1]); idx++)
					{
						expiry *= 10;
						expiry += commands[pxIndex + 1][idx] - '0';
					}
				}
			}

			if (queueing != -1)
			{ // Active transaction
				struct TransactionQueue *transaction =
						(struct TransactionQueue *)malloc(
								sizeof(struct TransactionQueue));

				transaction->expiresAt = expiry;
				transaction->operation = SET;
				transaction->next = NULL;

				transaction->key = (char *)malloc(strlen(commands[1]));

				if (transaction->key == NULL)
				{
					printf("Out of memory!\n");
					free(transaction);
					return NULL;
				}

				strcpy(transaction->key, commands[1]);

				transaction->value = (char *)malloc(strlen(commands[2]));
				if (transaction->value == NULL)
				{
					printf("Out of memory!\n");
					free(transaction->key);
					free(transaction);
					return NULL;
				}

				strcpy(transaction->value, commands[2]);

				if (transactionQueue == NULL)
				{ // First entry in the queue
					transactionQueue = transaction;
					transactionQueueTail = transactionQueue;
				}
				else
				{ // add new entry
					transactionQueueTail->next = transaction;
					transactionQueueTail = transactionQueueTail->next;
				}

				queueing += 1;

				encodeSimpleString(response, sizeof(response), "QUEUED");
				send(client_fd, response, strlen(response), 0);
			}
			else
			{
				setValue(hash_table, STRING, commands[1], commands[2], expiry);
				encodeSimpleString(response, sizeof(response), "OK");
				send(client_fd, response, strlen(response), 0);
			}
		}
		else if (compare(commands[0], "GET") == 0)
		{
			// GET command
			if (numberOfCommands != 2)
			{
				printf("[WARN] One agrument for 'GET' is required, got %d!\n", numberOfCommands);
			}

			if (queueing != -1)
			{ // Active transaction
				struct TransactionQueue *transaction =
						(struct TransactionQueue *)malloc(
								sizeof(struct TransactionQueue));

				transaction->expiresAt = -1;
				transaction->operation = GET;
				transaction->next = NULL;
				transaction->value = NULL;

				transaction->key = (char *)malloc(strlen(commands[1]));

				if (transaction->key == NULL)
				{
					printf("Out of memory!\n");
					free(transaction);
					return NULL;
				}

				strcpy(transaction->key, commands[1]);

				if (transactionQueue == NULL)
				{ // First entry in the queue
					transactionQueue = transaction;
					transactionQueueTail = transactionQueue;
				}
				else
				{ // add new entry
					transactionQueueTail->next = transaction;
					transactionQueueTail = transactionQueueTail->next;
				}

				queueing += 1;

				encodeSimpleString(response, sizeof(response), "QUEUED");
				send(client_fd, response, strlen(response), 0);
			}
			else
			{
				char *value = get((struct HashEntry **)hash_table, commands[1]);

				if (value == NULL)
				{
					free(value);

					send(client_fd, "$-1\r\n", strlen("$-1\r\n"), 0);
				}
				else
				{
					encodeBulkString(response, sizeof(response), value, strlen(value));
					free(value);

					send(client_fd, response, strlen(response), 0);
				}
			}
		}
		else if (compare(commands[0], "CONFIG") == 0)
		{ // CONFIG command
			printf("CONFIG\n");

			if (compare(commands[1], "GET") == 0 && numberOfCommands > 2)
			{ // CONFIG GET
				char *option = commands[2];

				if (compare(option, "dir") == 0)
				{
					int configs = 2;

					char **source = (char **)malloc(configs);
					source[0] = (char *)malloc(strlen(option));
					source[1] = (char *)malloc(strlen(config->path));

					strncpy(source[0], option, strlen(option));
					strncpy(source[1], config->path, strlen(config->path));

					encodeStringArray(response, source, configs);

					printf("Encoded String Array: %s\n", response);
					send(client_fd, response, strlen(response), 0);
				}
			}
			else
			{ // SET
				// send(client_fd, response, strlen(response), 0);
			}
		}
		else if (compare(commands[0], "INCR") == 0)
		{ // INCR: Increment value by 1 if number
			char *key = commands[1];

			if (queueing != -1)
			{ // Active transaction
				struct TransactionQueue *transaction =
						(struct TransactionQueue *)malloc(
								sizeof(struct TransactionQueue));

				transaction->expiresAt = -1;
				transaction->operation = INCR;
				transaction->next = NULL;
				transaction->value = NULL;

				transaction->key = (char *)malloc(strlen(key));

				if (transaction->key == NULL)
				{
					printf("Out of memory!\n");
					free(transaction);
					return NULL;
				}

				strcpy(transaction->key, key);

				if (transactionQueue == NULL)
				{ // First entry in the queue
					transactionQueue = transaction;
					transactionQueueTail = transactionQueue;
				}
				else
				{ // add new entry
					transactionQueueTail->next = transaction;
					transactionQueueTail = transactionQueueTail->next;
				}

				queueing += 1;

				encodeSimpleString(response, sizeof(response), "QUEUED");
				send(client_fd, response, strlen(response), 0);
			}
			else
			{
				long long number = increment(hash_table, key);

				if (number == NONUM)
				{
					// returning error because error occurred!
					snprintf(response, sizeof(response), "-ERR value is not an integer or out of range\r\n");
					send(client_fd, response, strlen(response), 0);
				}
				else
				{
					snprintf(
							response,
							sizeof(response),
							":%lld\r\n",
							number);

					send(client_fd, response, strlen(response), 0);
				}
			}
		}
		else if (compare(commands[0], "MULTI") == 0)
		{
			queueing += 1;
			encodeSimpleString(response, sizeof(response), "OK");
			printf("Sending: %s\n", response);

			send(client_fd, response, strlen(response), 0);
		}
		else if (compare(commands[0], "EXEC") == 0)
		{ // Execute the transaction
			if (queueing == -1)
			{ // No active transaction
				snprintf(response, sizeof(response), "-ERR EXEC without MULTI\r\n");
				send(client_fd, response, strlen(response), 0);
			}
			else
			{
				if (queueing == 0)
				{
					char **source = (char **)malloc(queueing);
					encodeStringArray(response, source, queueing);
					free(source);

					send(client_fd, response, strlen(response), 0);
				}
				else
				{
					int size = 0;
					char **source = (char **)execute(hash_table, transactionQueue, &queueing, &size);

					for (int i = 0; i < size; i++)
					{
						printf("[RESP OUT = %d] %s\n", i + 1, source[i]);
					}

					encodeStringArray(response, source, size);

					printf("Response: %s\n", response);

					send(client_fd, response, strlen(response), 0);
				}
				queueing = -1;
			}
		}
		else
		{
			encodeSimpleString(response, sizeof(response), default_response);
			send(client_fd, response, strlen(response), 0);
		}
	}

	printf("Client connected\n");

	close(client_fd);
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{

	config = (struct RDBConfig *)malloc(sizeof(struct RDBConfig));

	for (int idx = 0; idx < argc; idx++)
	{
		printf("[argv-%d]: %s\n", idx + 1, argv[idx]);

		if (compare(argv[idx], "--dir") == 0)
		{
			config->path = (char *)malloc(strlen(argv[idx + 1]));

			if (config->path == NULL)
			{
				printf("Out of memory!\n");
				free(config);
				return 1;
			}

			strcpy(config->path, argv[idx + 1]);
		}
		else if (compare(argv[idx], "--dbfilename") == 0)
		{
			config->fileName = (char *)malloc(strlen(argv[idx + 1]));

			if (config->fileName == NULL)
			{
				printf("Out of memory!\n");
				free(config);
				free(config->path);
				return 1;
			}

			strcpy(config->fileName, argv[idx + 1]);
		}
	}

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (server_fd == -1)
	{
		printf("Socket creation failed: %s\n", strerror(errno));
		return 1;
	}

	/*
	 * Since the tester restarts your program quite often, setting SO_REUSEADDR
	 * ensures that we don't run into 'Address already in use' errors
	 */
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
	{
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}

	struct sockaddr_in serv_addr = {
			.sin_family = AF_INET,
			.sin_port = htons(PORT),
			.sin_addr.s_addr = htonl(INADDR_ANY),
	};

	if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	int connection_backlog = 5;

	if (listen(server_fd, connection_backlog) != 0)
	{
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Waiting for a client to connect...\n");

	client_addr_len = sizeof(client_addr);

	while (1)
	{
		int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);

		if (client_fd < 0)
		{
			printf("Accept failed: %s \n", strerror(errno));
			continue;
		}

		struct client_info *info = malloc(sizeof(struct client_info));
		if (info == NULL)
		{
			printf("Memory allocation failed\n");
			close(client_fd);
			continue;
		}

		info->client_fd = client_fd;

		pthread_t thread_id;

		if (pthread_create(&thread_id, NULL, handle_client, (void *)info) != 0)
		{
			printf("Thread creation failed: %s\n", strerror(errno));
			free(info);
			close(client_fd);
			continue;
		}

		pthread_detach(thread_id);
	}

	close(server_fd);

	return 0;
}
