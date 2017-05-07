#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"
#include "kernel.h"
#include "errno.h"
#include "mmu.h"
#include "debug.h"
#include "scheduler.h"
#include "syscalls.h"

/**
 * Theses functions handle interrupts made by the processor
 */



/**
 * The base adress of the interrupt controller
 */

#define     SVC_EXIT        0x01
#define     SVC_FORK        0x02
#define     SVC_READ        0x03
#define     SVC_WRITE       0x04
#define     SVC_CLOSE       0x06
#define 	SVC_WAITPID		0x07
#define 	SVC_EXECVE 		0x0b
#define 	SVC_TIME 		0x0d
#define     SVC_LSEEK       0x13
#define     SVC_FSTAT       0x1c
#define     SVC_SBRK        0x2d

#define RPI_INTERRUPT_CONTROLLER_BASE (PERIPHERALS_BASE + 0xB200UL)

/**
 * The structure of the interrupt controller
 */
typedef struct {
  volatile uint32_t IRQ_basic_pending;
  volatile uint32_t IRQ_pending_1;
  volatile uint32_t IRQ_pending_2;
  volatile uint32_t FIQ_control;
  volatile uint32_t Enable_IRQs_1;
  volatile uint32_t Enable_IRQs_2;
  volatile uint32_t Enable_Basic_IRQs;
  volatile uint32_t Disable_IRQs_1;
  volatile uint32_t Disable_IRQs_2;
  volatile uint32_t Disable_Basic_IRQs;
} rpi_irq_controller_t;

/**
 * Bits in the Enable_Basic_IRQs register to enable various interrupts.
 * See the BCM2835 ARM Peripherals manual, section 7.5
 */
#define RPI_BASIC_ARM_TIMER_IRQ         (1 << 0)
#define RPI_BASIC_ARM_MAILBOX_IRQ       (1 << 1)
#define RPI_BASIC_ARM_DOORBELL_0_IRQ    (1 << 2)
#define RPI_BASIC_ARM_DOORBELL_1_IRQ    (1 << 3)
#define RPI_BASIC_GPU_0_HALTED_IRQ      (1 << 4)
#define RPI_BASIC_GPU_1_HALTED_IRQ      (1 << 5)
#define RPI_BASIC_ACCESS_ERROR_1_IRQ    (1 << 6)
#define RPI_BASIC_ACCESS_ERROR_0_IRQ    (1 << 7)

/**
 * Return a pointer to the interrupt controller
 */
rpi_irq_controller_t* RPI_GetIRQController(void);


/**
 * enable interrupts
 */
void enable_interrupts(void);

/**
 * disable interrupts
 */
void disable_interrupts(void);

#endif //INTERRUPTS_H
