#ifndef UTILS_KERNEL_H_
#define UTILS_KERNEL_H_

// Libs
#include <kernel.h>

void liberar_mutex_asociados(t_tcb *tcb_a_liberar);
void destruir_mutex_asociados(t_pcb *pcb_a_liberar);
void devolver_a_ready_joins(t_tcb *tcb);
uint32_t obtener_tid_maximo(t_pcb *pcb);
void *_max_prioridad(void* a, void* b);
t_tcb *obtener_tcb_maxima_prioridad(t_queue *cola);
t_tcb *encontrar_tcb_en(t_pcb *pcb, uint32_t tid_a_encontrar);

#endif // UTILS_H_