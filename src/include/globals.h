#pragma once

#define PORT 6379
#define TABLE_SIZE 1024
#define MAX_KEY_LENGTH 100
#define MAX_RESPONSE 100
#define TRANSACTION_SIZE 1024
#define MAX_CONFIG 1024
#define NONUM -0.00000000

struct client_info
{
	int client_fd;
};

enum RedisTypes
{
	STRING,
	LIST,
	HSET,
	HASH,
	ZSET,
	NUMBER,
};

struct HashEntry
{
	char *key;
	char *value;
	struct HashEntry *next;
	enum RedisTypes Type;
	long long expiresAt;
	long long createdAt;
};

enum Operation
{
	INCR,
	SET,
	GET,
};

struct TransactionQueue
{
	enum Operation operation;
	char *key;
	char *value;
	long long expiresAt;
	struct TransactionQueue *next;
};
