#include <stdlib.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "frame.h"

/* MPS2 AN385 (QEMU mps2-an385): CMSDK APB UART0 and TIMER0, 25 MHz peripheral clock */
#define UART0 ((volatile uint32_t *)0x40004000)
#define TIMER0 ((volatile uint32_t *)0x40000000)
#define NVIC_ISER (*(volatile uint32_t *)0xE000E100)
#define NVIC_IPR ((volatile uint8_t *)0xE000E400)
enum { UART_DATA, UART_STATE, UART_CTRL, UART_INT, UART_BAUDDIV };
enum { TIM_CTRL, TIM_VALUE, TIM_RELOAD, TIM_INT };
enum { IRQ_UART0_RX = 0, IRQ_TIMER0 = 8, PCLK_PER_MS = 25000, BATCH = 16 };

static QueueHandle_t samples, rx;
static TaskHandle_t cmd_handle;
static volatile uint32_t produced, dropped;
static uint32_t k;

static void uart_write(const uint8_t *p, size_t n) {
    while (n--) {
        while (UART0[UART_STATE] & 1);
        UART0[UART_DATA] = *p++;
    }
}

void fw_assert(const char *file, int line) {
    (void)file;
    (void)line;
    taskDISABLE_INTERRUPTS();
    uart_write((const uint8_t *)"\0ASSERT\0", 8);
    for (;;);
}

void timer0_isr(void) {
    TIMER0[TIM_INT] = 1;
    uint16_t v = (uint16_t)((k++ * 2654435761u) >> 20);
    BaseType_t woke = pdFALSE;
    if (xQueueSendFromISR(samples, &v, &woke) == pdTRUE) produced++;
    else dropped++;
    portYIELD_FROM_ISR(woke);
}

void uart0_rx_isr(void) {
    UART0[UART_INT] = 2;
    BaseType_t woke = pdFALSE;
    while (UART0[UART_STATE] & 2) {
        uint8_t c = (uint8_t)UART0[UART_DATA];
        xQueueSendFromISR(rx, &c, &woke);
    }
    portYIELD_FROM_ISR(woke);
}

static void set_period_ms(uint32_t ms) { TIMER0[TIM_RELOAD] = ms * PCLK_PER_MS - 1; }

/* "P<ms>\n" sets the sample period */
static void command_task(void *arg) {
    (void)arg;
    char line[16];
    size_t n = 0;
    for (;;) {
        uint8_t c;
        xQueueReceive(rx, &c, portMAX_DELAY);
        if (c != '\n') {
            if (n < sizeof line - 1) line[n++] = (char)c;
            continue;
        }
        line[n] = 0;
        long ms = line[0] == 'P' ? strtol(line + 1, NULL, 10) : 0;
        if (ms >= 1 && ms <= 1000) set_period_ms((uint32_t)ms);
        n = 0;
    }
}

static void telemetry_task(void *arg) {
    (void)arg;
    uint16_t batch[BATCH], seq_s = 0, seq_st = 0;
    uint8_t wire[FRAME_MAX];
    size_t n = 0;
    uint32_t last_produced = 0;
    TickType_t next = xTaskGetTickCount() + pdMS_TO_TICKS(1000);
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        if (xQueueReceive(samples, &batch[n], (TickType_t)(next - now) < 0x80000000u ? next - now : 0) == pdTRUE &&
            ++n == BATCH) {
            uart_write(wire, frame_build(FRAME_SAMPLES, seq_s++, batch, BATCH, wire));
            n = 0;
        }
        if ((int32_t)(xTaskGetTickCount() - next) >= 0) {
            next += pdMS_TO_TICKS(1000);
            uint32_t p = produced;
            uint16_t st[5] = {(uint16_t)(p - last_produced), (uint16_t)dropped,
                              (uint16_t)uxTaskGetStackHighWaterMark(NULL),
                              (uint16_t)uxTaskGetStackHighWaterMark(cmd_handle),
                              (uint16_t)xPortGetFreeHeapSize()};
            last_produced = p;
            uart_write(wire, frame_build(FRAME_STATUS, seq_st++, st, 5, wire));
        }
    }
}

int main(void) {
    samples = xQueueCreate(64, sizeof(uint16_t));
    rx = xQueueCreate(32, 1);
    xTaskCreate(telemetry_task, "telem", 256, NULL, 2, NULL);
    xTaskCreate(command_task, "cmd", 192, NULL, 3, &cmd_handle);

    UART0[UART_BAUDDIV] = 16;
    UART0[UART_CTRL] = 1 | 2 | 8; /* TX, RX, RX interrupt */
    set_period_ms(10);
    TIMER0[TIM_CTRL] = 1 | 8; /* enable, interrupt */
    NVIC_IPR[IRQ_UART0_RX] = 6 << 5;
    NVIC_IPR[IRQ_TIMER0] = 6 << 5;
    NVIC_ISER = (1u << IRQ_UART0_RX) | (1u << IRQ_TIMER0);

    vTaskStartScheduler();
    for (;;);
}
