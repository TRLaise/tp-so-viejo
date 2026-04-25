#ifndef ESTRUCTURAS_KERNEL_H_
#define ESTRUCTURAS_KERNEL_H_

#include "kernel.h"
#include "utils/mensajes.h"
#include "utils/pcb.h"
#include "utils/tcb.h"
#include <semaphore.h>
#include <stdint.h>
#include <commons/collections/queue.h>

typedef struct {
    uint32_t pid;
    char *path;
    uint32_t tamanio_en_memoria;
} t_proceso_nuevo;

typedef struct {
    uint32_t tid;
    char *path;
} t_hilo_nuevo;

typedef struct {
    t_tcb *tcb;  // TCB para hacer io
    uint32_t milisegundos;  // milisegundos que el tcb va a hacer io
} t_hilo_io;

typedef struct {
    char* nombre_recurso;
    uint32_t pid;
    t_tcb *tcb_asignado;
    t_queue *cola_bloqueados;
} t_mutex;

typedef enum { FIFO, PRIORIDADES, CMN } t_algoritmo_planificacion;

#endif // ESTRUCTURAS_KERNEL_H_