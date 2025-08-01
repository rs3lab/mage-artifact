#include <iostream>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>

#include "MindUser.h"
#include <future>

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

static MindUserClient connect(const std::string &host, int port) {

  std::shared_ptr<TTransport> socket(new TSocket(host, port));  
  std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
  std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
  MindUserClient client(protocol);

  transport->open();
  return client;
}

bool run_command(MindUserClient client, const int32_t op_code, const std::string addr){

  client.send_run_command(op_code, addr);
  return client.recv_run_command();;
}
