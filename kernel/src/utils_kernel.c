#include "utils_kernel.h"

// Libera todos los mutex(recurso) que un tcb tiene asignados
void liberar_mutex_asociados(t_tcb *tcb_a_liberar)
{
    // Obtenemos el padre del tcb a liberar
    t_pcb *pcb_padre = dictionary_get(diccionario_pcb_pid, string_itoa(tcb_a_liberar->pid_padre));

    // Iteramos los mutex del padre para ver si alguno tiene asignado el tcb a liberar
    t_list_iterator *it = list_iterator_create(pcb_padre->mutexs);
    while (list_iterator_has_next(it)) {
        t_mutex *mutex = list_iterator_next(it);
        // Si tiene asignado el tcb a liberar pasa a NULL
        if (tcb_a_liberar == mutex->tcb_asignado) {
            mutex->tcb_asignado = NULL;
        }

        // Si se encuentra en la cola de bloqueados del mutex lo saca
        for (int i = 0; i < list_size(mutex->cola_bloqueados->elements); i++){
            t_tcb *tcb_actual = list_get(mutex->cola_bloqueados->elements, i);
            uint32_t tid_actual = tcb_actual->tid;

            if(tid_actual == tcb_a_liberar->tid){
                list_remove_element(mutex->cola_bloqueados->elements, tcb_actual);
            }
        }
    }

    list_iterator_destroy(it);
}

// Destruir mutex de un PCB (!!! CUIDADO !!! SOLO SIRVE SI YA SE LIBERARON TODOS LOS HILOS ASOCIADOS AL PCB PREVIAMENTE)
void destruir_mutex_asociados(t_pcb *pcb_a_liberar)
{
    // Iteramos los mutex del padre para ver si alguno tiene asignado el tcb a liberar
    t_list_iterator *it = list_iterator_create(pcb_a_liberar->mutexs);
    while (list_iterator_has_next(it)) {
        t_mutex *mutex = list_iterator_next(it);
        
        // Ya esta en NULL porque previamente se libera a todos los hilos
        free(mutex->tcb_asignado);
        
        // La cola de bloqueados ya deberia estar libre porque previamente se libera a todos los hilos que tiene dentro
        queue_destroy(mutex->cola_bloqueados);
        
        free(mutex->nombre_recurso);
        free(mutex);
    }

    list_iterator_destroy(it);
}

// Devuelve el estado a READY de todos los hilos bloqueados por un tcb
void devolver_a_ready_joins(t_tcb *tcb)
{
    int tamanio_lista = list_size(tcb->hilos_joineados);
    for (int i = 0; i < tamanio_lista; i++) {
        t_tcb *tcb_actual = list_get(tcb->hilos_joineados, i);
        queue_push(cola_ready, tcb_actual);
        log_warning(debug_logger,"TAMANIO COLA READY: %d",(queue_size(cola_ready)));
        sem_post(&sem_elementos_en_ready);
    }
}

uint32_t obtener_tid_maximo(t_pcb *pcb)
{
    uint32_t tid_maximo = 0;
    int tamanio_lista = list_size(pcb->tcbs);

    for (int i = 0; i < tamanio_lista; i++)
    {
        t_tcb *tcb_actual = list_get(pcb->tcbs, i);
        uint32_t tid_actual = tcb_actual->tid;

        if (tid_actual > tid_maximo){
            tid_maximo = tid_actual;
        } 
    }

    return tid_maximo;
}

void *_max_prioridad(void* a, void* b) 
{
	t_tcb* tcb_a = (t_tcb*) a;
	t_tcb* tcb_b = (t_tcb*) b;
	return tcb_a->prioridad >= tcb_b->prioridad ? tcb_a : tcb_b;
}

t_tcb *obtener_tcb_maxima_prioridad(t_queue *cola)
{
    return list_get_maximum(cola->elements, _max_prioridad);
}

t_tcb *encontrar_tcb_en(t_pcb *pcb, uint32_t tid_a_encontrar)
{
    t_tcb *tcb_encontrado = NULL;

    // Iteramos los tcbs del pcb para ver si alguno tiene asignado el tcb del tid a buscar
    t_list_iterator *it = list_iterator_create(pcb->tcbs);
    while (list_iterator_has_next(it)) {
        t_tcb *tcb = list_iterator_next(it);
        // Si coincide el tid lo guardamos
        if (tcb->tid == tid_a_encontrar) {
            tcb_encontrado = tcb;
        }
    }

    list_iterator_destroy(it);
    return tcb_encontrado;
}