CXX := g++
TCP_DAEMON := tcp_daemon

tcp_daemon: tcp_daemon.cpp tcp_daemon.hpp
	$(CXX) tcp_daemon.cpp -o $(TCP_DAEMON) -lpthread

clean:
	rm -f $(TCP_DAEMON)
