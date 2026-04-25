#include "pcb.h"

t_pcb *pcb_create(uint32_t pid)
{
    t_pcb *pcb = malloc(sizeof(t_pcb));
    if (pcb == NULL) {
        log_error(debug_logger, "Error al alojar memoria para PCB");
        return NULL;
    }
    
    pcb->pid = pid;
    pcb->tcbs = list_create();
    pcb->mutexs = list_create();

    return pcb;
}

void pcb_destroy(t_pcb *pcb)
{
    free(pcb);
}