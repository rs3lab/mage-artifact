#include <getopt.h>
#include <unistd.h>
#include <cstdlib>
#include "disagg_switch.hpp"
#include "disagg_bf_client.hpp"
#include "../libuser/librpc/src/MindUserServer.h"
#include <thread>
#include <stdio.h>
#include <ctime>
#include <csignal>
#include "disagg_switch_config.hpp"
#include "disagg_logging_keys.hpp"

#ifdef __cplusplus
extern "C"
{
#endif
#include "controller/MindAccess.h"
#include "redis_logger/redis_logger.h"
#ifdef __cplusplus
}
#endif

std::string g_config_file_path = "config.json";

extern "C"
{
    extern int network_server_init(void);
    extern void network_server_exit(void);
    extern void test_access(int, char[]);
}

void term_signal_handler(int signum)
{
    network_server_exit();
    terminate_config();
    printf("Signal received(%d): terminating process...\n", signum);
    exit(signum);
}

std::time_t get_now(void)
{
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = now_ms.time_since_epoch().count();

    return std::chrono::system_clock::to_time_t(now);
}

int main(int argc, char **argv)
{
    auto time_now = get_now();
    std::cout << "Frontend started at " << std::put_time(std::localtime(&time_now), "%Y-%m-%d %H:%M:%S.") << '\n';
    printf("Read config:\n");
    load_config(g_config_file_path, argc, argv);

    // XXX(yash): What does this thread do?
    printf("Start mind user server\n");
    std::thread user_server_thread(start_server, test_access);

    printf("Start mind redis logger\n");
    if (initialize_redis())
    {
        printf("\n\n*** WARNING :: Failed to initialize redis logger\n\n\n");
    } else {
        // Reset all messages for the given cluster
        reset_cluster(get_cluster_id());

        // Add success messages
        redis_add_message(get_cluster_id(), redis_init_class, disagg_log_frontend, "success");
        redis_add_message(get_cluster_id(), redis_init_class, disagg_log_redis_conn, "success");
    }

    // Register signal handler
    signal(SIGINT, term_signal_handler);

    // Connect to Thrift server on the control plane backend
    InitMindBfClient(g_config_file_path);

    // Start TCP server for the compute blades
    network_server_init();
    printf("TCP server started...\n");

    // Wait for the server to exit
    while (true)
    {
        sleep(1);
    }

    // Free resources and exit
    // Disconnect and free the redis context
    release_redis();

    // termination msg
    printf("Mind switch control plane frontend has been terminated\n");
    return 0;
}
