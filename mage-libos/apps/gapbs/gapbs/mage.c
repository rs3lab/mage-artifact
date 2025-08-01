extern "C" {
    /*
        Mage handling routines.
    */
#define MADV_DDC_COOP_TLB 0x104
#include <stdio.h>
#include <sys/mman.h>

__thread unsigned long long  times = 0;

    /*
        Mage handler:
        TODO: Implement this function.
    */
    void __attribute__((always_inline)) __mage_handler(void){
    //void  __attribute__((noinline)) __mage_handler(void){
        //asm ("");
        //printf("Mage handler\n");

        // NOTE: This is a dummy test for the number of invocations
        //__atomic_fetch_add(&times, 1, __ATOMIC_SEQ_CST);
        //times ++;
        //asm volatile ("" : : "r" (*(unsigned long long*)(&times)));
        //if ((times + 1) % 100000000 == 0) printf("T\n");

        // syscall for cooperative TLB flush
        madvise((void*)0x300000000000ul, 0, MADV_DDC_COOP_TLB);
    }
}
