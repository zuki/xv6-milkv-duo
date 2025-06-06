#include <types.h>
#include <param.h>
#include <memlayout.h>
#include <riscv.h>
#include <defs.h>
#include <mmc.h>
#include <printf.h>

volatile static int started = 0;
volatile static unsigned long main_hartid = ~0UL;

extern volatile unsigned long uart_base;
extern char _bss_start[], _bss_end[];
// start() jumps here in supervisor mode on all CPUs.

//static struct mmc mmc;
void
main()
{
    if (main_hartid == ~0UL) {
        memset(_bss_start, 0, _bss_end - _bss_start);
        main_hartid = cpuid();
        consoleinit();
        devnull_init();
        printfinit();
        printf("\n");
        printf("xv6 kernel is booting in hart %d\n", cpuid());
        printf("\n");
        sbiinit();
        //kinit();          // physical page allocator
        page_init();        // page system (buddy + slab cache)
        kvminit();          // create kernel page table
        kvminithart();      // turn on paging
        uart_base = UART0;
        __sync_synchronize();
        procinit();         // process table
        trapinit();         // trap vectors
        rtc_init();
        clockinit();        // clock system
        trapinithart();     // install kernel trap vector
        plicinit();         // set up interrupt controller
        plicinithart();     // ask PLIC for device interrupts
        timer_init();       // timer
        binit();            // buffer cache
        iinit();            // inode table
        fileinit();         // file table
        randinit();         // random lock
        //virtio_disk_init(); // emulated hard disk
        //ramdiskinit();
        sd_init();
        userinit();      // first user process
        __sync_synchronize();
        started = 1;
    } else {
        while(started == 0) ;
        printf("hart %d started\n", cpuid());
        __sync_synchronize();
        kvminithart();    // turn on paging
        trapinithart();   // install kernel trap vector
        plicinithart();   // ask PLIC for device interrupts
        printf("hart %d init ok", cpuid());
    }

    scheduler();
}
