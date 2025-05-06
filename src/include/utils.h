#pragma once

#include "globals.h"
#include <string.h>
#include <sys/time.h>

int compare(const char *, const char *);

int calculateExponent(int e, int p)
{
  int exponent = e;
  for (int idx = 1; idx < p; idx++)
  {
    exponent *= e;
  }

  return exponent;
}

int calculateDigits(int number)
{
  int num = number;

  int digits = 0;
  while (number > 0)
  {
    digits += 1;
    number /= 10;
  }
  return digits;
}

unsigned int hash(const char *key)
{
  unsigned long int hash = 0;
  for (int i = 0; key[i] != '\0'; i++)
  {
    hash = hash * 31 + key[i];
  }
  return hash % TABLE_SIZE;
}

void set(struct HashEntry **hash_table, enum RedisTypes type, const char *key, const char *value, const long long expiry)
{
  struct timeval time; /* Getting time of the day from "sys/time.h" */
  gettimeofday(&time, NULL);

  const int currentTimestamp = time.tv_usec / 1000;         // miliseconds | current timestamp
  const int expiryTimestamp = time.tv_usec / 1000 + expiry; // miliseconds | expiry timestamp

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

    newHashEntry->Type = type;
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
    if (expiry != -1)
    {
      newHashEntry->expiresAt = expiryTimestamp;
    }
    else
    {
      newHashEntry->expiresAt = -1;
    }
    newHashEntry->createdAt = currentTimestamp;

    hash_table[hashedVal] = newHashEntry;
  }
  else
  {
    struct HashEntry *pointer = hashEntry;
    while (pointer->next != NULL && compare(pointer->key, key) != 0)
    {
      pointer = pointer->next;
    }

    if (pointer->next == NULL && compare(pointer->key, key) != 0)
    {
      // Add new HashEntry
      struct HashEntry *newHashEntry = (struct HashEntry *)malloc(sizeof(struct HashEntry));
      if (newHashEntry == NULL)
      {
        printf("SET command failed to execute - out of memory!\n");
        return;
      }

      newHashEntry->Type = type;
      newHashEntry->key = (char *)malloc(strlen(key) + 1);
      if (newHashEntry->key == NULL)
      {
        printf("SET command failed to execute - out of memory!\n");
        free(newHashEntry);
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
      if (expiry != -1)
      {
        newHashEntry->expiresAt = expiryTimestamp;
      }
      else
      {
        newHashEntry->expiresAt = -1;
      }
      newHashEntry->createdAt = currentTimestamp;

      pointer->next = newHashEntry;
    }
    else
    {
      // Update Pointer value with value
      strcpy(pointer->value, value);
    }
  }
}

char *get(struct HashEntry **hash_table, const char *key)
{
  struct timeval time; /* Getting time of the day from "sys/time.h" */
  gettimeofday(&time, NULL);

  const int currentTimestamp = time.tv_usec / 1000;

  unsigned int hashedVal = hash(key);
  struct HashEntry *hashEntry = hash_table[hashedVal];

  if (hashEntry == NULL)
  {
    return NULL;
  }
  else
  {
    struct HashEntry *pointer = hashEntry;
    struct HashEntry *prevPointer = NULL;

    while (pointer->next != NULL && compare(pointer->key, key) != 0)
    {
      prevPointer = pointer;
      pointer = pointer->next;
    }

    if (pointer != NULL)
    {
      if (compare(pointer->key, key) == 0)
      {
        if (pointer->expiresAt != -1 && pointer->expiresAt < currentTimestamp)
        {
          if (prevPointer != NULL)
          {
            prevPointer->next = pointer->next;
          }

          free(pointer);
          return NULL;
        }
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

long long increment(struct HashEntry **hash_table, const char *key)
{
  char *value = get(hash_table, key);

  if (value == NULL)
  { // Key is not present
    set(hash_table, NUMBER, key, (const char *)"1", -1);
    return 1;
  }
  else
  { // Key is present
    char *end = NULL;
    long long number = strtoll(value, &end, 10);

    if (*end != '\0')
    {
      printf("Type Error, Cannot INCR non-numerical values!\n");
      return NONUM;
    }

    number += 1;
    char newValue[32];

    snprintf(
        newValue,
        sizeof(newValue),
        "%lld",
        number);

    set(hash_table, NUMBER, key, newValue, -1);
    return number;
  }
}

int compare(const char *primary, const char *secondary)
{
  if (strlen(primary) != strlen(secondary))
  {
    return strlen(primary) > strlen(secondary) ? 1 : -1;
  }

  for (int idx = 0; idx < strlen(primary); idx++)
  {
    if (toupper(primary[idx]) != toupper(secondary[idx]))
    {
      return toupper(primary[idx]) > toupper(secondary[idx]) ? 1 : -1;
    }
  }

  return 0;
}
