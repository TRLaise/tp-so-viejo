#include "tcb.h"

t_tcb *tcb_create(uint32_t tid, uint32_t prioridad, uint32_t pid_padre)
{
    t_list *hilos_joineados = list_create();
    t_registros *registros = malloc(sizeof(t_registros));
    t_tcb *tcb = malloc(sizeof(t_tcb));
    if (tcb == NULL) {
        log_error(debug_logger, "Error al alojar memoria para tcb");
        return NULL;
    }
    tcb->tid = tid;
    tcb->prioridad = prioridad;
    tcb->pid_padre = pid_padre;
    tcb->hilos_joineados = hilos_joineados;
    
    return tcb;
}

void tcb_destroy(t_tcb *tcb)
{
    free(tcb);
}

void tcb_send(t_tcb *tcb, int socket_conexion)
{
    t_paquete *paquete = crear_paquete(INIT_PID_TID);
    agregar_a_paquete(paquete, tcb, sizeof(t_tcb));

    enviar_paquete(paquete, socket_conexion);

    eliminar_paquete(paquete);
}

t_tcb *tcb_receive(int socket_conexion)
{
    t_list *contenido = recibir_paquete(socket_conexion);
    t_tcb *tcb = list_get(contenido, 0);
    list_destroy(contenido);
    return tcb;
}