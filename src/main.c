#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <pthread.h>
#include "include/globals.h"
#include "include/parsers.h"
#include "include/encrypter.h"

#define PORT 6379
#define MAX_CLIENTS 100

void *handle_client(void *arg)
{
	struct client_info *info = (struct client_info *)arg;
	int client_fd = info->client_fd;
	free(info);

	while (1)
	{
		char request[4096];
		char response[1024];
		char default_response[] = "+PONG\r\n";

		int bytes_read = recv(client_fd, request, sizeof(request), 0);

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

		if (request[0] == '*')
		{
			commands = parseEncodedArray(request, bytes_read, &numberOfCommands);
		}

		if (commands == NULL || numberOfCommands == 0)
		{
			printf("No commands sent from client!\n");
		}

		if (memcmp(commands[0], "ECHO", strlen("ECHO")) == 0)
		{
			encodeBulkString(response, sizeof(response), commands[1], strlen(commands[1]));
			send(client_fd, response, strlen(response), 0);
		}
		else
		{
			send(client_fd, default_response, strlen(default_response), 0);
		}
	}

	printf("Client connected\n");

	close(client_fd);
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
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
		int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t*) &client_addr_len);

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
