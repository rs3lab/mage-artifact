/*
    Mage handling routines.
*/
#define MADV_DDC_COOP_TLB 0x104
#include <stdio.h>
#include <sys/mman.h>
#include <array>
#include <osv/sched.hh>
#include <osv/mmu.hh>

/*
    Mage handler:
    TODO: Implement this function.
*/
//unsigned long long  times = 0;
void __attribute__((always_inline)) __mage_handler(void){
    unsigned cpu_id = sched::cpu::current()->id;
    //printf("%u\n", cpu_id);
    if (mmu::tlb_flush_cachelines[cpu_id].cacheline[0]){
        madvise((void*)0x300000000000ul, 0, MADV_DDC_COOP_TLB);
    }
    //__atomic_fetch_add(&times, 1, __ATOMIC_SEQ_CST);
    //if ((times + 1) % 100000000 == 0) printf("%llu\n", times);

//void  __attribute__((noinline)) __mage_handler(void){
    //asm ("");
    //printf("Mage handler\n");
    //__atomic_fetch_add(&times, 1, __ATOMIC_SEQ_CST);
    //times ++;
    //if ((times + 1) % 100000000 == 0) printf("T\n");
    //madvise((void*)0x300000000000ul, 0, MADV_DDC_COOP_TLB);
}
