#include "kernel.h"
void process_exit_dump(uint32_t pid);


void *dump_mem(void *params)
{
    while(true) {
        // Esperar que haya elementos en espera para hacer dump
        sem_wait(&sem_elementos_en_dump);

        t_tcb *tcb = queue_pop(cola_blocked_dump);

        // Espero la respuesta de memoria
        op_code respuesta_memoria = recibir_operacion((int)dump_fd);
        log_info(debug_logger, "OPERACION RECIBIDA: %d", respuesta_memoria);
        if (respuesta_memoria != MENSAJE_OP_TERMINADA) {
            log_error(debug_logger, "Memoria no pudo terminar el hilo");
            abort();
        }
        
        if (respuesta_memoria == MENSAJE_EXITO_DUMP) {
            bool removido = list_remove_element(cola_blocked_dump->elements, tcb);
            if (!removido){
                log_error(debug_logger, "No se pudo remover el hilo %d de blocked", tcb->tid);
            }

            // Devuelve el hilo a ready
            queue_push(cola_ready, tcb);
            log_info(logger_kernel, "## (%d:%d) finalizó dump y pasa a READY", tcb->pid_padre, tcb->tid);          
        }
        else {
            log_error(debug_logger, "Memoria no pudo hacer dump del proceso");
            // "en caso de error, el proceso se enviara a EXIT"
            process_exit_dump(tcb->pid_padre);
        }   
    }
}

void process_exit_dump(uint32_t pid)
{
    //tomo el pcb del pid con el que lo llamo
    t_pcb *pcb = dictionary_get(diccionario_pcb_pid, string_itoa(pid));
    
    uint32_t tid_max = obtener_tid_maximo(pcb);
    
    // Avisar a memoria que debe liberar el proceso

    int new_fd = crear_conexion(config_kernel->ip_memoria, config_kernel->puerto_memoria);
    log_info(debug_logger, "NUEVO SOCKET PARA MEMORIA: %d", new_fd);
    if (new_fd == -1) {
        log_error(debug_logger, "ERROR AL CREAR CONEXION CON MEMORIA");
        exit(EXIT_FAILURE);
    }

    // Le enviamos a memoria el pid, path al pseudocodigo y tamaño
    t_paquete *paquete = crear_paquete(PROCESS_EXIT);
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    enviar_paquete(paquete, new_fd);
    eliminar_paquete(paquete);

    // Espero la respuesta de memoria
    op_code respuesta_memoria = recibir_operacion((int)new_fd);
    log_info(debug_logger, "OPERACION RECIBIDA: %d", respuesta_memoria);
    if (respuesta_memoria != MENSAJE_OP_TERMINADA) {
        log_error(debug_logger, "Memoria no pudo terminar el proceso");
        abort();
    }

    // Avisa que se libero un espacio en memoria
    flag_espacio_proceso = true;
    
    // Desalojar el proceso
    if(pid_en_ejecucion == pid){
        t_paquete *paquete = crear_paquete(INTERRUPCION);
        op_code motivo = DESALOJO_HILO;
        agregar_a_paquete(paquete, &motivo, sizeof(op_code));
        enviar_paquete(paquete, cpu_interrupt_fd);
        eliminar_paquete(paquete);
    }
    
    // Cancelamos todos los tcbs menos el 0 (porque thread cancel solo anda desde tid 0)
    for (int i = 1; i <= tid_max ; i++){
        thread_cancel(i);
    }
    // Cancelamos tid 0
    thread_cancel(0);
    
    // Liberar PCB
    //liberar_pcb(pid);
    
    log_info(logger_kernel, "## Finaliza el proceso %d", pid);
    
    close(new_fd);
}