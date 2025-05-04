#define PORT 6379
#define TABLE_SIZE 1024

struct client_info
{
	int client_fd;
};

struct HashEntry
{
	char *key;
	char *value;
	struct HashEntry *next;
	long long expiresAt;
	long long createdAt;
};
