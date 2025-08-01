#include <iostream>
#include "server.h"
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;
using namespace mind_ctrl;

void MindCtrlHandler::exchange(Command& _return, const Command& cmd) {
    std::cout << "Received command ID: " << cmd.id << ", data: " << cmd.data << std::endl;
    _return.id = cmd.id;
    _return.data = cmd.data;
}

void start_server() {
    std::shared_ptr<MindCtrlHandler> handler(new MindCtrlHandler());
    std::shared_ptr<TProcessor> processor(new MindCtrlProcessor(handler));
    std::shared_ptr<TServerTransport> serverTransport(new TServerSocket(9090));
    std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

    TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
    std::cout << "Starting the server..." << std::endl;
    server.serve();
    std::cout << "Server stopped." << std::endl;
}
