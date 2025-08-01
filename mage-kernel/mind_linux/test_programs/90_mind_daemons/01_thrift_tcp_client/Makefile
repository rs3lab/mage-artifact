all:
	@thrift --gen cpp mind_ctrl_thrift/mind_ctrl.thrift
	@rm gen-cpp/MindCtrl_server.skeleton.cpp
	g++ -std=c++11 -o tcp_client client.cpp gen-cpp/*.cpp -lthrift
