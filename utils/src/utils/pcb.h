#ifndef PCB_H_
#define PCB_H_

#include <stdint.h>
#include <stdlib.h>
#include <utils/conexion.h>
#include <utils/serializacion.h>
#include <utils/utils.h>
#include <commons/log.h>
#include <commons/collections/list.h>

/*
** Estructuras
*/

typedef struct {
    uint32_t pid;
    t_list *tcbs;
    t_list *mutexs;
} t_pcb;

/*
typedef enum {
    NEW,
    READY,
    EXEC,
    BLOCKED,
    EXIT,
} t_estado;
*/

/*
** Definiciones de funciones
*/

t_pcb *pcb_create(uint32_t pid);
void pcb_destroy(t_pcb *);

#endif // PCB_H_