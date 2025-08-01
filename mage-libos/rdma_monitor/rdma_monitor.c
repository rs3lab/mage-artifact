#include "rdma_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

int rx_packets_fd;
int rx_bytes_fd;
int tx_packets_fd;
int tx_bytes_fd;
int read_reqs_fd;
int write_reqs_fd;

FILE *rx_packets_fp;
FILE *rx_bytes_fp;
FILE *tx_packets_fp;
FILE *tx_bytes_fp;
FILE *read_reqs_fp;
FILE *write_reqs_fp;

uint64_t rx_packets;
uint64_t rx_bytes;
uint64_t tx_packets;
uint64_t tx_bytes;
uint64_t read_reqs;
uint64_t write_reqs;

uint64_t new_rx_packets;
uint64_t new_rx_bytes;
uint64_t new_tx_packets;
uint64_t new_tx_bytes;
uint64_t new_read_reqs;
uint64_t new_write_reqs;

double rx_packets_tput;
double rx_bytes_tput;
double tx_packets_tput;
double tx_bytes_tput;
double read_reqs_tput;
double write_reqs_tput;

uint64_t read_sysfs_with_open(const char *file_name){
    FILE *sysfs_fp = fopen(file_name, "r");
    if (!sysfs_fp){
        fprintf(stderr, "fopen error\n");
        exit(EXIT_FAILURE);
    }
    uint64_t ret = 0;
    if (fscanf(sysfs_fp, "%lu", &ret) != 1){
	fprintf(stderr, "fscanf error\n");
        exit(EXIT_FAILURE);
    }
    fclose(sysfs_fp);
    printf("%lu\n", ret);
    
    return ret;
}

uint64_t read_sysfs(FILE *sysfs_fp){
    uint64_t ret = 0;
    if (fscanf(sysfs_fp, "%lu", &ret) != 1){
	fprintf(stderr, "fscanf error\n");
        exit(EXIT_FAILURE);
    }
    fflush(sysfs_fp);
    rewind(sysfs_fp);
    /*
    if (fseek(sysfs_fp, 0, SEEK_SET) != 0){
	fprintf(stderr, "fseek error\n");
        exit(EXIT_FAILURE);
    }
    */
    printf("%lu\n", ret);
    
    return ret;
}

uint64_t read_sysfs_with_fd(int sysfs_fd){
    uint64_t ret = 0;
    if (pread(sysfs_fd, &ret, sizeof(ret), 0) == -1){
        fprintf(stderr, "pread error\n");
    }
    printf("%lu\n", ret);
    return ret;

}

void process(){

    /*
    rx_packets_fp = fopen(RX_PACKETS_STR, "r");
    rx_bytes_fp = fopen(RX_BYTES_STR, "r");
    tx_packets_fp = fopen(TX_PACKETS_STR, "r");
    tx_bytes_fp = fopen(TX_BYTES_STR, "r");
    read_reqs_fp = fopen(READ_REQ_STR, "r");
    write_reqs_fp = fopen(WRITE_REQ_STR, "r");

    if ( !rx_packets_fp  ||
        !rx_bytes_fp  ||
        !tx_packets_fp  ||
        !tx_bytes_fp  ||
        !read_reqs_fp  ||
        !write_reqs_fp ){
        exit(EXIT_FAILURE);
    }
    */

    /*
    rx_packets_fd = open(RX_PACKETS_STR, O_RDONLY);
    rx_bytes_fd = open(RX_BYTES_STR, O_RDONLY);
    tx_packets_fd = open(TX_PACKETS_STR, O_RDONLY);
    tx_bytes_fd = open(TX_BYTES_STR, O_RDONLY);
    read_reqs_fd = open(READ_REQ_STR, O_RDONLY);
    write_reqs_fd = open(WRITE_REQ_STR, O_RDONLY);

    if ( rx_packets_fd==-1  ||
        rx_bytes_fd==-1  ||
        tx_packets_fd==-1  ||
        tx_bytes_fd==-1  ||
        read_reqs_fd==-1  ||
        write_reqs_fd==-1 ){
        exit(EXIT_FAILURE);
    }
    */

    rx_packets = read_sysfs_with_open(RX_PACKETS_STR);
    rx_bytes = read_sysfs_with_open(RX_BYTES_STR);
    tx_packets = read_sysfs_with_open(TX_PACKETS_STR);
    tx_bytes = read_sysfs_with_open(TX_PACKETS_STR);
    read_reqs = read_sysfs_with_open(READ_REQ_STR);
    write_reqs = read_sysfs_with_open(WRITE_REQ_STR);
    
    /*
    rx_packets = read_sysfs(rx_packets_fp);
    rx_bytes = read_sysfs(rx_bytes_fp);
    tx_packets = read_sysfs(tx_packets_fp);
    tx_bytes = read_sysfs(tx_bytes_fp);
    read_reqs = read_sysfs(read_reqs_fp);
    write_reqs = read_sysfs(write_reqs_fp);
    */
    
    /*
    rx_packets = read_sysfs_with_fd(rx_packets_fd);
    rx_bytes = read_sysfs_with_fd(rx_bytes_fd);
    tx_packets = read_sysfs_with_fd(tx_packets_fd);
    tx_bytes = read_sysfs_with_fd(tx_bytes_fd);
    read_reqs = read_sysfs_with_fd(read_reqs_fd);
    write_reqs = read_sysfs_with_fd(write_reqs_fd);
    */

    struct timeval st, et;
    while (1) {
	gettimeofday(&st, NULL);
        new_rx_packets = read_sysfs_with_open(RX_PACKETS_STR);
        new_rx_bytes = read_sysfs_with_open(RX_BYTES_STR);
        new_tx_packets = read_sysfs_with_open(TX_PACKETS_STR);
        new_tx_bytes = read_sysfs_with_open(TX_BYTES_STR);
        new_read_reqs = read_sysfs_with_open(READ_REQ_STR);
        new_write_reqs = read_sysfs_with_open(WRITE_REQ_STR);

        /*
        new_rx_packets = read_sysfs(rx_packets_fp);
        new_rx_bytes = read_sysfs(rx_bytes_fp);
        new_tx_packets = read_sysfs(tx_packets_fp);
        new_tx_bytes = read_sysfs(tx_bytes_fp);
        new_read_reqs = read_sysfs(read_reqs_fp);
        new_write_reqs = read_sysfs(write_reqs_fp);
        */
	
	/*
        new_rx_packets = read_sysfs_with_fd(rx_packets_fd);
        new_rx_bytes = read_sysfs_with_fd(rx_bytes_fd);
        new_tx_packets = read_sysfs_with_fd(tx_packets_fd);
        new_tx_bytes = read_sysfs_with_fd(tx_bytes_fd);
        new_read_reqs = read_sysfs_with_fd(read_reqs_fd);
        new_write_reqs = read_sysfs_with_fd(write_reqs_fd);
	*/

        rx_packets_tput = (new_rx_packets - rx_packets) * 1.0 / 1e6 / MONITOR_INT;
        rx_bytes_tput = (new_rx_bytes - rx_bytes) * 4.0 / 1e6 / MONITOR_INT;
        tx_packets_tput = (new_tx_packets - tx_packets) * 1.0 / 1e6 / MONITOR_INT;
        tx_bytes_tput = (new_tx_bytes - tx_bytes) * 4.0 / 1e6 / MONITOR_INT;
        read_reqs_tput = (new_read_reqs - read_reqs) * 1.0 / 1e6 / MONITOR_INT;
        write_reqs_tput = (new_write_reqs - write_reqs) * 1.0 / 1e6 / MONITOR_INT;

        rx_packets = new_rx_packets;
        rx_bytes = new_rx_bytes;
        tx_packets = new_tx_packets;
        tx_bytes = new_tx_bytes;
        read_reqs = new_read_reqs;
        write_reqs = new_write_reqs;
	gettimeofday(&et, NULL);

	printf("rx_packets_tput: %lf mpkts/s \n\
rx_bytes_tput: %lf MB/s \n\
tx_packets_tput: %lf mpkts/s \n\
tx_bytes_tput: %lf MB/s \n\
read_reqs_tput: %lf mreqs/s \n\
write_reqs_tput: %lf mreqs/s \n\n",
		rx_packets_tput, rx_bytes_tput,
		tx_packets_tput, tx_bytes_tput,
		read_reqs_tput, write_reqs_tput);

	int elapsed = ((et.tv_sec - st.tv_sec) * 1000000) + (et.tv_usec - st.tv_usec);
	printf("%d us\n",elapsed);

        sleep(MONITOR_INT);
    }


}

int main(int argc, char **argv){
    process();
}
