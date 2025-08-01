// Logging
#ifndef __DISAGG_LOGGING_KEYS_HPP__
#define __DISAGG_LOGGING_KEYS_HPP__

static const char *redis_init_class = "init";
static const char *redis_run_class = "run";

// for control plane frontend
static const char *disagg_log_frontend = "frontend";
static const char *disagg_log_redis_conn = "frontend_redis";
static const char *disagg_log_thrift = "frontend_backend_comm";

// for compute blade init
static const char *disagg_log_compute_roce = "compute_%d_roce";
static const char *disagg_log_compute_daemon = "compute_%d_daemon";
static const char *disagg_log_compute_kshmem = "compute_%d_kernel_shmem";

// for memory blade init
static const char *disagg_log_memory_roce = "memory_%d_roce";

#endif // __DISAGG_LOGGING_KEYS_HPP__
