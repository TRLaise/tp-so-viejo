#include "kernel.h"

// Pasa procesos en NEW a READY cuando la cola esta vacia.
void *planificador_largo_plazo(void *param){
    while (true) {
        // Esperar hasta que haya elementos en NEW.
        sem_wait(&sem_elementos_en_new);

        if(flag_espacio_proceso) {
            t_proceso_nuevo *proceso_inicial = malloc(sizeof(t_proceso_nuevo));
            if (queue_is_empty(cola_new)) {
                proceso_inicial = NULL;
            } else {
                proceso_inicial = queue_peek(cola_new);
            }

            // Si no tenemos ningun proceso, no hacer nada
            if (proceso_inicial == NULL) {
                // Liberar el permiso para agregar procesos a ready
                sem_post(&sem_entrada_a_ready);
                continue;
            }
            
            int new_fd = enviar_proceso_nuevo_a_memoria(proceso_inicial);

            // Esperar que memoria nos avise que cargo el proceso.

            op_code respuesta_memoria = recibir_operacion((int)new_fd);
            log_info(debug_logger, "OPERACION RECIBIDA: %d", respuesta_memoria);
            close(new_fd);

            if (respuesta_memoria == MENSAJE_NO_HAY_ESPACIO) {
                log_error(debug_logger, "No se pudo iniciar el proceso por falta de memoria");
                flag_espacio_proceso = false;
                continue;
            }
            else if (respuesta_memoria != MENSAJE_OP_TERMINADA) {
                log_error(debug_logger, "Memoria no pudo cargar el proceso");
                abort();
            }

            // Si la respuesta es positiva saco el primer proceso de la cola
            queue_pop(cola_new);
            
            // Crear el PCB
            t_pcb *pcb = pcb_create(proceso_inicial->pid);

            // Agregar al diccionario el pid con su pcb
            dictionary_put(diccionario_pcb_pid, string_itoa(proceso_inicial->pid), pcb);

            // Creacion Hilo inicial
            t_tcb *tcb = tcb_create(0, 0, pcb->pid);
            list_add(pcb->tcbs, tcb);

            // Tomar el permiso para agregar procesos a ready
            sem_wait(&sem_entrada_a_ready);
            // Agrego a ready
            queue_push(cola_ready, tcb);

            // Liberar el permiso para agregar procesos a ready
            sem_post(&sem_entrada_a_ready);
            // Avisar que entra un nuevo elemento a ready
            sem_post(&sem_elementos_en_ready);

            // Logs
            log_info(logger_kernel, "PID: %d - Estado Anterior: NEW - Estado Actual: READY", pcb->pid);
            log_info(logger_kernel, "## (%d:%d) Se crea el Hilo - Estado: READY", pcb->pid, tcb->tid);
            
            // Liberar el proceso
            free(proceso_inicial->path);
            free(proceso_inicial);
        }
    }
}

int enviar_proceso_nuevo_a_memoria(t_proceso_nuevo *proceso_nuevo)
{
    int new_fd = crear_conexion(config_kernel->ip_memoria, config_kernel->puerto_memoria);
    log_info(debug_logger, "NUEVO SOCKET PARA MEMORIA: %d", new_fd);
    if (new_fd == -1) {
        log_error(debug_logger, "ERROR AL CREAR CONEXION CON MEMORIA");
        exit(EXIT_FAILURE);
    }
    
    // Le enviamos a memoria el pid, path al pseudocodigo y tamaño
    t_paquete *paquete = crear_paquete(PROCESS_CREATE);
    agregar_a_paquete(paquete, &(proceso_nuevo->pid), sizeof(uint32_t));
    agregar_a_paquete(paquete, proceso_nuevo->path, strlen(proceso_nuevo->path) + 1);
    agregar_a_paquete(paquete, &(proceso_nuevo->tamanio_en_memoria), sizeof(uint32_t));
    enviar_paquete(paquete, new_fd);
    eliminar_paquete(paquete);

    return new_fd;
}