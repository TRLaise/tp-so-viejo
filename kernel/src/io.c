#include "kernel.h"

void *entrada_salida(void *params)
{
    while(true) {
        // Esperar que haya elementos en espera para hacer io
        sem_wait(&sem_elementos_en_io);

        t_hilo_io *hilo_io = queue_pop(cola_io);

        // Simula un IO
        usleep(hilo_io->milisegundos * 1000);

        // Devuelve el hilo a ready
        queue_push(cola_ready, hilo_io->tcb);
        log_info(logger_kernel, "## (%d:%d) finalizó IO y pasa a READY", hilo_io->tcb->pid_padre, hilo_io->tcb->tid);
    }
}