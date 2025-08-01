#include "tcp_daemon.hpp"
#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>

using namespace std;
#define _RECV_BUFFER_FRAGMENTATION_CHECK 1024

void poke_buffer_content(struct mem_header *hdr) {
    cout << "hdr->opcode: " << hdr->opcode << ", hdr->sender_id: " << hdr->sender_id;
    cout << ", hdr->size: " << hdr->size << endl;
}

int send_msg_to_switch_from_kernel(int clientSd, char *buffer, int send_buffer_len, int recv_buffer_len, char *retbuf) {
    int sent_size = 0;
    int recv_size = 0;
    int i = 0;
    poke_buffer_content((struct mem_header *) buffer);

    sent_size = send(clientSd, (const char*) buffer, send_buffer_len, 0);
    if (sent_size < send_buffer_len) {
        return -ERR_DISAGG_NET_FAILED_TX;
    }
    do {
        int ret = recv(clientSd, (char*)retbuf + recv_size, recv_buffer_len - recv_size, 0);
        if (ret == -1) {
            cerr << "Error!\n";
            free(retbuf);
            retbuf = NULL;
            return ret;
        }
        recv_size += ret;
        cout << "recv_buffer_len: " << recv_buffer_len << ", ret: " << ret << ", recv: " << recv_size << endl;
    }while(recv_buffer_len > _RECV_BUFFER_FRAGMENTATION_CHECK && recv_size < recv_buffer_len);
out:
    return recv_size;
}

void get_buffer_length(int *send_buffer_len, int *recv_buffer_len) {
    syscall(GET_BUFFER_LEN_SYSCALL, send_buffer_len, recv_buffer_len);
}

char *read_kernel_buffer(int send_buffer_len) {
    char *buffer;
    buffer = (char *) calloc(send_buffer_len, sizeof(char));
    syscall(COPY_MSG_USER_BUFFER_SYSCALL, buffer, send_buffer_len);
    return buffer;
}

void write_kernel_buffer(char *buffer, int recv_buffer_len, int ret_len) {
    
    syscall(WRITE_MSG_TO_KERNEL_SYSCALL, buffer, recv_buffer_len, ret_len);
    return;
}

int main(int argc, char *argv[])
{
#ifndef USER_SPACE_TCP
    cout << "USER_SPACE_TCP not enabled!" << endl;
    return 0;
#endif
    pin_to_core(DISAGG_TCP_HANDLER_CPU);
    string serverIp = SWITCH_IP; 
    int port = SWITCH_PORT; 
    char *out_buffer;
    char *in_buffer;
    int clientSd;
    int status = -1;
    int send_buffer_len;
    int recv_buffer_len;

    if (argc >= 2)
    {
        serverIp = argv[1];
        cout << "Control plane frontend IP: " << serverIp << endl;
    }

    struct hostent* host = gethostbyname(serverIp.c_str()); 
    sockaddr_in sendSockAddr;   
    bzero((char*)&sendSockAddr, sizeof(sendSockAddr)); 

    sendSockAddr.sin_family = AF_INET; 
    sendSockAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list));
    sendSockAddr.sin_port = htons(port);
    clientSd = socket(AF_INET, SOCK_STREAM, 0);

    cout << "Before connecting to server!" << endl;
    while (status < 0)
    {
        status = connect(clientSd,  (sockaddr*) &sendSockAddr, sizeof(sendSockAddr));
        if (status < 0) {
            cerr << "Error connecting to socket!" << endl;
            sleep(1);
        }
    }
    cout << "Connected to the server!" << endl;

    while (1) {
        get_buffer_length(&send_buffer_len, &recv_buffer_len);
        cout << "send_buffer_len: " << send_buffer_len << ", recv_buffer_len: " << recv_buffer_len << endl;
        out_buffer = read_kernel_buffer(send_buffer_len);

        char *in_buffer = (char *) calloc(recv_buffer_len, sizeof(char));
        int ret_len = send_msg_to_switch_from_kernel(clientSd, out_buffer, send_buffer_len, recv_buffer_len, in_buffer);

        if (!in_buffer)
            exit(-1);

        write_kernel_buffer(in_buffer, recv_buffer_len, ret_len);
        free(out_buffer);
        free(in_buffer);
    }
    close(clientSd);
    return 0;    
}
