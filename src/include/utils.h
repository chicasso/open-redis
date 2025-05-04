#include "globals.h"
#include <string.h>

unsigned int hash(const char *key)
{
  unsigned long int hash = 0;
  for (int i = 0; key[i] != '\0'; i++)
  {
    hash = hash * 31 + key[i];
  }
  return hash % TABLE_SIZE;
}

void set(struct HashEntry **hash_table, const char *key, const char *value)
{
  unsigned int hashedVal = hash(key);

  struct HashEntry *hashEntry = hash_table[hashedVal];

  if (hashEntry == NULL)
  {
    struct HashEntry *newHashEntry = (struct HashEntry *)malloc(sizeof(struct HashEntry));
    if (newHashEntry == NULL)
    {
      printf("SET command failed to execute - out of memory!\n");
      return;
    }

    newHashEntry->key = (char *)malloc(strlen(key) + 1);

    if (newHashEntry->key == NULL)
    {
      free(newHashEntry);
      printf("SET command failed to execute - out of memory!\n");
      return;
    }

    newHashEntry->value = (char *)malloc(strlen(value) + 1);

    if (newHashEntry->value == NULL)
    {
      printf("SET command failed to execute - out of memory!\n");
      free(newHashEntry->key);
      free(newHashEntry);
      return;
    }

    strcpy(newHashEntry->key, key);
    strcpy(newHashEntry->value, value);
    newHashEntry->next = NULL;

    hash_table[hashedVal] = newHashEntry;
  }
  else
  {
    struct HashEntry *pointer = hashEntry;
    while (pointer->next != NULL && strcmp(pointer->key, key) != 0)
    {
      pointer = pointer->next;
    }

    if (pointer->next == NULL && strcmp(pointer->key, key) != 0)
    {
      // Add new HashEntry
      struct HashEntry *newHashEntry = (struct HashEntry *)malloc(sizeof(struct HashEntry));
      if (newHashEntry == NULL)
      {
        printf("SET command failed to execute - out of memory!\n");
        return;
      }

      newHashEntry->key = (char*) malloc(strlen(key) + 1);
      if (newHashEntry->key == NULL)
      {
        printf("SET command failed to execute - out of memory!\n");
        free(newHashEntry);
        return;
      }

      newHashEntry->value = (char*) malloc(strlen(value) + 1);
      if (newHashEntry->value == NULL)
      {
        printf("SET command failed to execute - out of memory!\n");
        free(newHashEntry->key);
        free(newHashEntry);
        return;
      } 

      strcpy(newHashEntry->key, key);
      strcpy(newHashEntry->value, value);
      newHashEntry->next = NULL;

      pointer->next = newHashEntry;
    }
    else
    {
      // Update Pointer value with value
      strcpy(pointer->value, value);
    }
  }
}

char *get(const struct HashEntry **hash_table, const char *key)
{
  unsigned int hashedVal = hash(key);
  const struct HashEntry *hashEntry = hash_table[hashedVal];

  if (hashEntry == NULL)
  {
    return NULL;
  }
  else
  {
    const struct HashEntry *pointer = hashEntry;
    while (pointer->next != NULL && strcmp(pointer->key, key) != 0)
    {
      pointer = pointer->next;
    }

    if (pointer != NULL)
    {
      if (strcmp(pointer->key, key) == 0)
      {
        return pointer->value;
      }
      else
      {
        return NULL;
      }
    }
    else
    {
      return NULL;
    }
  }
}
