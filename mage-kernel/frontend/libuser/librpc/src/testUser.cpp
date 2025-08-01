#include <iostream>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>

#include "MindUserClient.h"
#include "MindUser.h"
#include <future>
#include <atomic>
#include <tuple>
#include <iostream>
#include <fstream>

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
const int DIMENSION = 9;
const int shard_num = 30;
const int client_num = 64;

int main(int argc, char *argv[]) {

    std::string addr = "/home/sslee/mind_internal/mind_linux/test_programs/06a_switch_launch_task/dummy_shell_script.sh";

    if (argc == 1) {
        std::cout << "Use default command: " << addr << std::endl;
    }
    else if (argc == 2) {
        addr = argv[1];
        std::cout << "Use command: " << addr << std::endl;
    }
    else {
        std::cout << "Wrong number of argument! ./testUser [command]" << std::endl;
        exit(-1);
    }
    int op_code = 5;
    auto client = connect("10.10.11.32", 9182);
    run_command(client, op_code, addr);

    return 0;
}

