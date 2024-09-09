#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;
volatile static unsigned long main_hartid = ~0UL;

extern volatile unsigned long uart_base;
extern char _bss_start[], _bss_end[];
// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(main_hartid == ~0UL){
    memset(_bss_start, 0, _bss_end - _bss_start);
    main_hartid = cpuid();
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    sbiinit();
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    uart_base = UART0;
    __sync_synchronize();
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    //virtio_disk_init(); // emulated hard disk
    ramdiskinit();
#ifdef GPIO_DRIVER
    gpioinit();
#endif
#ifdef PWM_DRIVER
    pwminit();
#endif
#ifdef ADC_DRIVER
    adcinit();
#endif
#ifdef I2C_DRIVER
    i2cinit();
#endif
#ifdef SPI_DRIVER
    spiinit();
#endif
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
    printf("hart %d started\n", cpuid());
  }

  scheduler();        
}
