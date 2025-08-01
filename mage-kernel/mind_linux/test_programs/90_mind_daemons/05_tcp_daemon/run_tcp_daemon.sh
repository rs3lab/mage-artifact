#$1: ip address of the control plane frontend

make clean
make tcp_daemon
unbuffer taskset -c 0 ./tcp_daemon "$1" >tcp_daemon.log 2>&1
