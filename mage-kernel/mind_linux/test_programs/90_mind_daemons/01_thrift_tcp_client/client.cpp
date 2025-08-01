#include <iostream>
#include "client.h"
#include "gen-cpp/MindCtrl.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace mind_ctrl;

void run_client(const Command& cmd) {
    std::shared_ptr<TTransport> socket(new TSocket("10.10.11.204", 9090));
    std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
    std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

    MindCtrlClient client(protocol);
    transport->open();
    Command response;
    client.exchange(response, cmd);
    std::cout << "Received response: command ID: " << response.id << ", data: " << response.data << std::endl;
    transport->close();
}

int main (void) {
    Command cmd;
    cmd.id = 1;
    cmd.data = "Hello, world!";

    run_client(cmd);

    return 0; 
}
