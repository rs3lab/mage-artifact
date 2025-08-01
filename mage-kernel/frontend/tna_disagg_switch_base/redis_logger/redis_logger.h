#ifndef __MIND_REDIS_LOGGER_H__
#define __MIND_REDIS_LOGGER_H__

int initialize_redis();
void release_redis();
void reset_cluster(int cluster_id);
int redis_add_message_with_blade(int cluster_id, int blade_id, const char *key_class, const char *fmt, const char *message);
int redis_add_message(int cluster_id, const char *key_class, const char *key_item, const char *message);
int test_redis(int cluster_id);

#endif // __MIND_REDIS_LOGGER_H__
