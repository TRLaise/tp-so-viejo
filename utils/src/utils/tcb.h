#ifndef TCB_H_
#define TCB_H_

#include <stdint.h>
#include <stdlib.h>
#include <utils/conexion.h>
#include <utils/serializacion.h>
#include <commons/log.h>

/*
** Estructuras
*/

typedef struct {
    uint32_t tid;
    uint32_t prioridad;
    t_registros *registros;
    uint32_t pid_padre;
    t_list *hilos_joineados; // Hilos_joineados son todos tcbs
} t_tcb;

/*
** Definiciones de funciones
*/

t_tcb *tcb_create(uint32_t tid, uint32_t prioridad, uint32_t pid_padre);
void tcb_destroy(t_tcb *tcb);

void tcb_send(t_tcb *tcb, int socket_conexion);
t_tcb *tcb_receive(int socket_conexion);

#endif // TCB_H_