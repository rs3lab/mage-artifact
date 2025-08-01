#include "MindAccess.h"
#include "list_and_hash.h"
#include <stdio.h>
#include "task_management.h"
#include "memory_management.h"
#include <stdlib.h>

void test_access(int op_code, char addr[]){
    printf("test from MindAccess, op_code: %d\n", op_code);
    forward_task_request_to_cn(addr);
    return;
}

void forward_task_request_to_cn(char addr[]) {

    int ret = 0;
    int target_node = 1;
    struct remote_thread_struct *cur_sk;
    cur_sk = get_remote_thread_socket(target_node);
    if (!cur_sk)
    {
        printf("fail to get socket to forward remote thread\n");
    }

    printf("forward_task_request_to_cn addr: %s\n", addr);
    ret = send_new_task_to_cn(target_node, cur_sk->sk, addr);
    printf("forward_task_request_to_cn: ret: %d\n", ret);
    return;
}