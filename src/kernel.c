/** \file kernel.c
 *	\brief Kernel entry point
 *
 * 	One goal: set up devices and peripherals before launching the first program.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include "serial.h"
#include "timer.h"
#include "interrupts.h"
#include "gpio.h"
#include "debug.h"
#include "ext2.h"
#include "vfs.h"
#include "storage_driver.h"
#include "process.h"
#include "scheduler.h"
#include "mmu.h"
#include "dev.h"
#include "mailbox.h"
#include "kernel.h"
#include "procfs.h"
#include "fdsyscalls.h"
#include "framebuffer.h"

extern void start_mmu(uint32_t ttl_address, uint32_t flags);

/** \var extern uint32_t __ramfs_start
 * 	\brief Start of the in-RAM filesystem, as defined by the linker script.
 */
extern uint32_t __ramfs_start;

/** \var volatile uint32_t __ram_size
 * 	\brief Total available RAM
 * 	This is set up in initsys.c by reading the ATAGS.
 */
volatile uint32_t __ram_size;

extern char __kernel_bss_start;
extern char __kernel_bss_end;

extern int __kernel_phy_end;

/**	\fn int memory_read(uint32_t address, void* buffer, uint32_t size)
 *	\brief Read driver for the in-memory filesystem.
 *	\param address Ramdisk offset to read.
 * 	\param buffer Destination buffer.
 *	\param size Size to read.
 *	\return 0
 */
int memory_read(uint32_t address, void* buffer, uint32_t size) {
    kdebug(D_KERNEL, 0, "Disk read  request at address %p of size %d to %p\n", address, size, buffer);

    dmb();
	intptr_t base = (intptr_t)&__ramfs_start; // The FS is concatenated with the kernel image.
	memcpy(buffer, (void*) (address + base), size);

    return 0;
}

/**	\fn int memory_write(uint32_t address, void* buffer, uint32_t size)
 *	\brief Write driver for the in-memory filesystem.
 *	\param address Ramdisk offset to write.
 * 	\param buffer Source buffer.
 *	\param size Size to write.
 *	\return 0
 */
int memory_write(uint32_t address, void* buffer, uint32_t size) {
	intptr_t base = (intptr_t)&__ramfs_start;
    kdebug(D_KERNEL, 0, "Disk write request at address %#08x of size %d\n", address, size);
	dmb();
	memcpy((void*) (address + base), buffer, size);
	return 0;
}

/** \fn void blink(int n)
 * 	\brief ACT LED blinking
 *	\param n The number of blinks.
 */
void blink(int n) {
	GPIO_setPinValue(GPIO_LED_PIN, true);
	Timer_WaitCycles(1000000);
	GPIO_setPinValue(GPIO_LED_PIN, false);
	Timer_WaitCycles(1000000);
	for(int i=0;i<n;i++) {
		GPIO_setPinValue(GPIO_LED_PIN, true);
		Timer_WaitCycles(250000);
		GPIO_setPinValue(GPIO_LED_PIN, false);
		Timer_WaitCycles(250000);
	}
	Timer_WaitCycles(750000);
	GPIO_setPinValue(GPIO_LED_PIN, true);
}

/** \fn void kernel_main(uint32_t memory)
 * 	\brief Kernel main function.
 *
 *	Called by initsys.c, this function initialize high level kernel features.
 * 	It:
 * 	- clears the BSS segment.
 *	- Remove the linear mapping on kernel's TTB.
 * 	- Initialize serial output, scheduler, page allocation, USPi library.
 *	- Initialize filesystem features.
 *  - Set up the timer.
 *	- Load the 'init' process and context switch.
 */
void kernel_main(uint32_t memory) {
	// Initialize BSS segment
	for (char* val = (char*)&__kernel_bss_start; val < (char*)&__kernel_bss_end; val++) {
		(*val)=0;
	}

	__ram_size = memory;
	GPIO_setOutputPin(GPIO_LED_PIN);

	// Remove linear mapping of the first 2GB
	for (uint32_t i=0;i<0x80000000;i+=PAGE_SECTION) {
		mmu_delete_section(0xf0004000, i);
	}

    serial_init();
    setup_scheduler();

    enable_interrupts();


	// TTB1 is already set up on boot (-> 0x4000)
	// TTB0 is set up on each context switch
	mmu_setup_ttbcr(TTBCR_ALIGN);

	paging_init((__ram_size >> 20) - 1, 2+((((uintptr_t)&__kernel_phy_end) + PAGE_SECTION - 1) >> 20));

	kernel_printf("[INFO][SERIAL] Serial output is hopefully ON.\r");


    unsigned char mac[6];
    mbxGetMACAddress(mac);
    kernel_printf("Mac address : %X:%X:%X:%X:%X:%X\n",
                  mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);




    //We use a buffer to store the frameBuffer informations
    //Using malloc makes qemu crash
    frameBuffer fb __attribute__((aligned (16)));

    if(fb_initialize(&fb,640,480,24) == FB_SUCCESS) {
        kernel_printf("Frame Buffer was correctly initialized\n");
    }
	kernel_printf("Ramsize: %x\n", __ram_size);
	kernel_printf("B: %p\n", fb.bufferPtr);
	kernel_printf("S: %x\n", fb.bufferPtr >> 20);
	fb.bufferPtr &= ~0xC0000000;
    uintptr_t section_to = (uintptr_t)fb.bufferPtr >> 20;
    mmu_add_section(0xf0004000,0x7FE00000,section_to << 20,0,0,AP_PRW_URW);
    mmu_add_section(0xf0004000,0x7FF00000,(section_to+1) << 20,0,0,AP_PRW_URW);

    fb.bufferPtr = (fb.bufferPtr & 0x000FFFFF) | 0x7FE00000;
	kernel_printf("V: %x\n", fb.bufferPtr);

    volatile uint16_t* frame = (volatile uint16_t*)(intptr_t)fb.bufferPtr;
	kernel_printf("SZ: %d\n", fb.bufferSize);
	kernel_printf("P: %d\n", fb.pitch);
    for(int i = 0; i<fb.bufferSize; i++) {
		((uint8_t*) fb.bufferPtr)[i] = i%255;
    }

	dmb();





  	asm volatile("mrs r0,cpsr\n"
  				 "orr r0,r0,#0x80\n"
  				 "msr cpsr_c,r0\n");

	Timer_Setup();
	Timer_SetLoad(TIMER_LOAD);
    Timer_Enable();

    //USPiInitialize();


	storage_driver memorydisk;
	memorydisk.read    = memory_read;
	memorydisk.write   = memory_write;


	superblock_t* fsroot = ext2fs_initialize(&memorydisk);
	if (fsroot == NULL) {
        kernel_printf("Error while initializing fsroot\n");
		while(1) {}
	}
	superblock_t* devroot = dev_initialize(10);
	if (devroot == NULL) {
        kernel_printf("Error while initializing devroot\n");
		while(1) {}
	}

	superblock_t* procroot = proc_initialize(20);
	if (procroot == NULL) {
		kernel_printf("Error while initializing procroot\n");
		while(1) {}
	}


	vfs_setup();
	vfs_mount(fsroot,"/");


    vfs_mkdir("/","dev",0x1FF);
	vfs_mount(devroot,"/dev");


    vfs_mkdir("/","proc",0x1FF);
    vfs_mount(procroot,"/proc");


	const char* param[] = {"/bin/init",0};
    const char* env[] = {NULL};

	process* p = process_load("/bin/init", vfs_path_to_inode(NULL, "/"), param, env); // init program

	p->fd[0].inode 		= load_inode(vfs_path_to_inode(NULL, "/dev/serial")); // creates an unique inode for serial.
	p->fd[0].position   = 0;

	p->fd[1].inode      = load_inode(vfs_path_to_inode(NULL, "/dev/serial"));
	p->fd[1].position   = 0;


	if (p != NULL) {

		sheduler_add_process(p);
        get_next_process();

	    RPI_GetIRQController()->Enable_Basic_IRQs = RPI_BASIC_ARM_TIMER_IRQ;
		dmb();


		p->parent_id 	= p->asid; // (Badass process)
		kernel_printf("%p\n", p);
		kernel_printf("%p %p\n", p->ttb_address, mmu_vir2phy(p->ttb_address));
	    mmu_set_ttb_0(mmu_vir2phy(p->ttb_address), TTBCR_ALIGN);
		asm volatile(
			"mov 	r0, %0\n"
			"ldmfd 	r0!, {r1, lr}\n"
			"msr 	spsr, r1\n"
			"ldmfd 	r0, {r0-r14}^\n"
			"movs 	pc, lr\n"
			:
			: "r" (&p->ctx)
			:);

	} else {
		kdebug(D_KERNEL, 10, "[ERROR] Could not load init");
		while(1) {}
	}
}
