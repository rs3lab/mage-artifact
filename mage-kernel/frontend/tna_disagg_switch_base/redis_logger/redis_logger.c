#include <stdio.h>
#include <time.h>
#include "hiredis/hiredis.h"
#include "redis_logger.h"

#define REDIS_TIMEOUT_IN_SEC 3
#define REDIS_HOST_IP "127.0.0.1"
#define REDIS_HOST_PORT 6379
#define MAX_KEY_LENGTH 128
#define MAX_VALUE_LENGTH 256
#define TIME_STAMP_LENGTH 32

static redisContext *redix_ctx = NULL;

static int _check_redis_ctx(void)
{
    if (redix_ctx == NULL)
    {
        printf("Redis context is not initialized\n");
        return -1;
    }
    return 0;
}

int initialize_redis()
{
    struct timeval timeout = {REDIS_TIMEOUT_IN_SEC, 0};
    redix_ctx = redisConnectWithTimeout(REDIS_HOST_IP, REDIS_HOST_PORT, timeout);
    if (redix_ctx == NULL || redix_ctx->err)
    {
        if (redix_ctx)
        {
            printf("Connection error: %s\n", redix_ctx->errstr);
            redisFree(redix_ctx);
        }
        else
        {
            printf("Connection error: can't allocate redis context\n");
        }
        redix_ctx = NULL;
        return -1;
    }
    return 0;
}

void release_redis()
{
    // make sure that redis is connected
    if (_check_redis_ctx())
        return ;

    redisFree(redix_ctx);
}

void reset_cluster(int cluster_id)
{
    redisReply *reply;

    // Construct the key pattern
    char key_pattern[MAX_KEY_LENGTH];
    sprintf(key_pattern, "cluster%d::*", cluster_id);

    // make sure that redis is connected
    if (_check_redis_ctx())
        return ;


    // Delete all keys that match the pattern
    reply = redisCommand(redix_ctx, "KEYS %s", key_pattern);
    if (reply->type == REDIS_REPLY_ARRAY)
    {
        for (int j = 0; j < reply->elements; j++)
        {
            redisCommand(redix_ctx, "DEL %s", reply->element[j]->str);
        }
    }
    freeReplyObject(reply);
}

static void redis_build_message_for_blade(int blade_id, const char *fmt, char *buf, size_t max_len)
{
    if (buf)
        snprintf(buf, max_len, fmt, blade_id);
}

int redis_add_message_with_blade(int cluster_id, int blade_id, const char *key_class, const char *fmt, const char *message)
{
    char buf[MAX_KEY_LENGTH] = {0};
    redis_build_message_for_blade(blade_id, fmt, buf, MAX_KEY_LENGTH);
    return redis_add_message(cluster_id, key_class, buf, message);
}

int redis_add_message(int cluster_id, const char *key_class, const char *key_item, const char *message)
{
    redisReply *reply;
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
    time_t now;
    struct tm *tm_info;
    char timestamp[TIME_STAMP_LENGTH];

    // Construct the key
    sprintf(key, "cluster%d::%s::%s", cluster_id, key_class, key_item);

    // make sure that redis is connected
    if (_check_redis_ctx())
        return -1;

    // Construct the value with a timestamp
    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, TIME_STAMP_LENGTH, "%Y/%m/%d %H:%M:%S", tm_info);
    sprintf(value, "%s - %s", timestamp, message);

    // Store the log message
    reply = redisCommand(redix_ctx, "SET %s %s", key, value);
    if (reply->type == REDIS_REPLY_ERROR)
    {
        printf("SET error: %s\n", reply->str);
        freeReplyObject(reply);
        return -1;
    }
    freeReplyObject(reply);

    return 0;
}

int test_redis(int cluster_id)
{
    // make sure that redis is connected
    if (_check_redis_ctx())
        return -1;

    // Add a log message
    if (redis_add_message(cluster_id, "init", "start_log", "Initialization started") != 0)
    {
        printf("Failed to add message\n");
        release_redis();
        return -1;
    }

    return 0;
}
