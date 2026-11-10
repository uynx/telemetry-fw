#include <stdint.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
extern int main(void);
extern void vPortSVCHandler(void), xPortPendSVHandler(void), xPortSysTickHandler(void);
extern void uart0_rx_isr(void), timer0_isr(void);

void Reset_Handler(void) {
    for (uint32_t *s = &_sidata, *d = &_sdata; d < &_edata;) *d++ = *s++;
    for (uint32_t *d = &_sbss; d < &_ebss;) *d++ = 0;
    main();
    for (;;);
}

void Fault_Handler(void) { for (;;); }

__attribute__((section(".vectors"), used)) static void (*const vectors[])(void) = {
    (void (*)(void))&_estack, Reset_Handler, Fault_Handler, Fault_Handler,
    Fault_Handler, Fault_Handler, Fault_Handler, 0, 0, 0, 0,
    vPortSVCHandler, 0, 0, xPortPendSVHandler, xPortSysTickHandler,
    [16 + 0] = uart0_rx_isr,
    [16 + 8] = timer0_isr,
};
