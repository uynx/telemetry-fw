#pragma once
#define configCPU_CLOCK_HZ 25000000
#define configTICK_RATE_HZ 1000
#define configUSE_PREEMPTION 1
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configMAX_PRIORITIES 4
#define configMINIMAL_STACK_SIZE 128
#define configTOTAL_HEAP_SIZE (8 * 1024)
#define configMAX_TASK_NAME_LEN 8
#define configUSE_16_BIT_TICKS 0
#define configUSE_MUTEXES 0
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
#define configCHECK_FOR_STACK_OVERFLOW 0
#define INCLUDE_vTaskDelay 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define configPRIO_BITS 3
#define configKERNEL_INTERRUPT_PRIORITY (7 << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << (8 - configPRIO_BITS))
void fw_assert(const char *file, int line);
#define configASSERT(x) do { if (!(x)) fw_assert(__FILE__, __LINE__); } while (0)
