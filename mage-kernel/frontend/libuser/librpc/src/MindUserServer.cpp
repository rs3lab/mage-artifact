#include <thrift/concurrency/ThreadManager.h>
// #include <thrift/concurrency/ThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/TToString.h>
#include <thrift/server/TNonblockingServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/transport/TNonblockingServerSocket.h>
#include <thrift/transport/TNonblockingServerTransport.h>
#include <queue>

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <future>
#include <fstream>
#include "MindUser.h"
#include "MindUserServer.h"

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;

extern void test();
using callback_t = void(*)(int, char []);
callback_t callback_func;
class MindUserHandler : public MindUserIf {
public:

  MindUserHandler(callback_t func){
    _func = func;
  };

  bool run_command(const int32_t op_code, const std::string &addr){
    std::cout << "op_code: " << op_code << std::endl;
    char addr_c[100];
    strcpy(addr_c, addr.c_str()); 
    printf("run command, addr: %s\n", addr_c);
    _func(op_code, addr_c);
    return true;
  }

protected:
  callback_t _func;

};

class MindUserCloneFactory : virtual public MindUserIfFactory {
 public:
  ~MindUserCloneFactory() override = default;
  MindUserIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo)
  {
    std::shared_ptr<TSocket> sock = std::dynamic_pointer_cast<TSocket>(connInfo.transport);
    cout << "Incoming connection\n";
    cout << "\tSocketInfo: "  << sock->getSocketInfo() << "\n";
    cout << "\tPeerHost: "    << sock->getPeerHost() << "\n";
    cout << "\tPeerAddress: " << sock->getPeerAddress() << "\n";
    cout << "\tPeerPort: "    << sock->getPeerPort() << "\n";
    return new MindUserHandler(callback_func);
  }
  void releaseHandler( MindUserIf* handler) {
    delete handler;
  }
};

void start_server(callback_t callbackFunc) {

  std::string ip_address = "10.10.10.1";
  callback_func = callbackFunc;
  int port_num = 9182;
  TThreadedServer server(
    std::make_shared<MindUserProcessorFactory>(std::make_shared<MindUserCloneFactory>()),
    std::make_shared<TServerSocket>(port_num), //port
    std::make_shared<TBufferedTransportFactory>(),
    std::make_shared<TBinaryProtocolFactory>()
  );

  cout << "Starting the server..." << endl;
  server.serve();
  cout << "Done." << endl;
  
}

/*
int main(int argc, char *argv[]){

  std::string ip_address = "10.10.10.1";
  int port_num = 9095;
  TThreadedServer server(
    std::make_shared<MindUserProcessorFactory>(std::make_shared<MindUserCloneFactory>()),
    std::make_shared<TServerSocket>(port_num), //port
    std::make_shared<TBufferedTransportFactory>(),
    std::make_shared<TBinaryProtocolFactory>()
  );

  cout << "Starting the server..." << endl;
  server.serve();
  cout << "Done." << endl;

  return 0;
}
*/
