#pragma once

#include "globals.h"
#include <string.h>
#include "encoder.h"
#include <sys/time.h>

int calculateExponent(int e, int p);

int calculateDigits(int number);

unsigned int hash(const char *);

void setValue(struct HashEntry **, enum RedisTypes, const char *, const char *, const long long);

char *get(struct HashEntry **, const char *);

long long increment(struct HashEntry **, const char *);

char **execute(struct HashEntry **, struct TransactionQueue *, int *, int *);

int compare(const char *, const char *);

/** FUNCTIONS */
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

void setValue(struct HashEntry **hash_table, enum RedisTypes type, const char *key, const char *value, const long long expiry)
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
    setValue(hash_table, NUMBER, key, (const char *)"1", -1);
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

    setValue(hash_table, NUMBER, key, newValue, -1);
    return number;
  }
}

char **execute(struct HashEntry **hash_table, struct TransactionQueue *transactionQueue, int *queueSize, int *items)
{
  printf("Execute Transaction [*%d]\n", *queueSize);

  char **responses = (char **)malloc((*queueSize) * sizeof(char));
  struct TransactionQueue *ptr = transactionQueue;

  int idx = 0;
  while (ptr != NULL && (*queueSize) > 0)
  {
    printf("[EXEC = %d] %s %s [OPR = ", idx + 1, ptr->key, ptr->value);

    switch (ptr->operation)
    {
    case SET:
      printf("SET]\n");

      setValue(hash_table, STRING, ptr->key, ptr->value, ptr->expiresAt);

      responses[idx] = (char *)malloc(MAX_RESPONSE);

      // encodeSimpleString(responses[idx], MAX_RESPONSE, "OK");
      strcpy(responses[idx], "OK");

      (*items) += 1;

      break;
    case INCR:
      printf("INCR]\n");

      long long number = increment(hash_table, ptr->key);

      if (number == NONUM)
      {
        char ERROR_RESP[] = "-ERR value is not an integer or out of range\r\n";
        responses[idx] = (char *)malloc(strlen(ERROR_RESP));

        snprintf(responses[idx], sizeof(responses[idx]), ERROR_RESP);
      }
      else
      {
        responses[idx] = (char *)malloc(MAX_RESPONSE);
        // snprintf(responses[idx], sizeof(responses[idx]), "%lld", number);

        snprintf(
            responses[idx],
            MAX_RESPONSE,
            ":%lld\r\n" /* ":%lld\r\n" */,
            number);
      }

      (*items) += 1;

      break;
    case GET:
      printf("GET]\n");

      char *getResp = get(hash_table, ptr->key);

      responses[idx] = (char *)malloc(MAX_RESPONSE);

      // encodeSimpleString(responses[idx], MAX_RESPONSE, getResp);

      encodeNumberFromChar(responses[idx], getResp);

      // strcpy(responses[idx], getResp);

      (*items) += 1;

      break;
    default:
      break;
    }

    struct TransactionQueue *nodeToBeDeleted = ptr;
    ptr = ptr->next;

    (*queueSize) -= 1;
    free(nodeToBeDeleted);

    idx += 1;
  }

  for (int i = 0; i < (*items); i++)
  {
    printf("[RESP = %d] %s\n", i + 1, responses[i]);
  }

  return responses;
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
