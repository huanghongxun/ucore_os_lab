#include <x86.h>
#include <intr.h>

void
intr_enable(void) {
    sti();
}

void
intr_disable(void) {
    cli();
}

