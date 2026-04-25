#include "kernel.h"

static uint32_t ultimo_pid = -1;

//Definiciones locales
static bool buscar_y_eliminar_en_cola(uint32_t tid, t_queue* cola);
static bool mutex_esta_tomado_por(t_tcb *tcb, t_mutex *mutex);
static bool buscar_y_eliminar_en_cola(uint32_t tid, t_queue* cola);
static bool existe_mutex_en(char *mutex_a_buscar, t_list *lista_mutex);
static bool esta_tomado(t_mutex *mutex);
static int enviar_hilo_nuevo_a_memoria(uint32_t pid, uint32_t tid, char *path);
static void liberar_pcb(uint32_t pid); 

/* 
SYSCALLS 
*/
void process_create(char *path, uint32_t tamanio_en_memoria, uint32_t prioridad_hilo_main)
{
    // Guardar tanto el pid como el path, para enviar a memoria
    t_proceso_nuevo *proceso_nuevo = malloc(sizeof(t_proceso_nuevo));
    if (proceso_nuevo == NULL) {
        log_error(debug_logger, "No se pudo alojar memoria");
        abort();
    }
    
    ultimo_pid++;
    proceso_nuevo->pid = ultimo_pid;
    // Hay que duplicarlo para poder liberar el comando despues
    proceso_nuevo->path = string_duplicate(path);
    proceso_nuevo->tamanio_en_memoria = tamanio_en_memoria;

    // Agregarlo a new
    queue_push(cola_new, proceso_nuevo);

    sem_post(&sem_elementos_en_new);
    
    log_info(logger_kernel, "## (%d:0) - Se crea el proceso - Estado: NEW", proceso_nuevo->pid);
}

void process_exit()
{
    if(tid_en_ejecucion != 0){
        log_error(debug_logger, "un TID que no es TID 0 quiso finalizar el proceso");
    }

    //tomo el pcb del pid que esta en ejecucion
    t_pcb *pcb_en_ejecucion = dictionary_get(diccionario_pcb_pid, string_itoa(pid_en_ejecucion));
    
    uint32_t tid_max = obtener_tid_maximo(pcb_en_ejecucion);
    
    // Avisa que se libero un espacio en memoria
    flag_espacio_proceso = true;
    
    // Cancelamos todos los tcbs menos el 0 (porque thread cancel solo anda desde tid 0)
    for (int i = 1; i <= tid_max ; i++){
        thread_cancel(i);
    }
    // Cancelamos tid 0
    thread_cancel(0);

    // Avisar a memoria que debe liberar el proceso
    int new_fd = crear_conexion(config_kernel->ip_memoria, config_kernel->puerto_memoria);
    log_info(debug_logger, "NUEVO SOCKET PARA MEMORIA: %d", new_fd);
    if (new_fd == -1) {
        log_error(debug_logger, "ERROR AL CREAR CONEXION CON MEMORIA");
        exit(EXIT_FAILURE);
    }

    // Le enviamos a memoria el pid, path al pseudocodigo y tamaño
    t_paquete *paquete = crear_paquete(PROCESS_EXIT);
    agregar_a_paquete(paquete, &(pcb_en_ejecucion->pid), sizeof(uint32_t));
    enviar_paquete(paquete, new_fd);
    eliminar_paquete(paquete);

    // Espero la respuesta de memoria
    op_code respuesta_memoria = recibir_operacion((int)new_fd);
    t_list* data = recibir_paquete(new_fd);
    log_info(debug_logger, "OPERACION RECIBIDA: %d", respuesta_memoria);
    if (respuesta_memoria != MENSAJE_OP_TERMINADA) {
        log_error(debug_logger, "Memoria no pudo terminar el proceso");
        abort();
    }
    
    // Liberar PCB
    liberar_pcb(pid_en_ejecucion);
    
    log_info(logger_kernel, "## Finaliza el proceso %d", pcb_en_ejecucion->pid);
    close(new_fd);
}

void thread_create(char *path, uint32_t prioridad)
{
    t_pcb *pcb_en_ejecucion = dictionary_get(diccionario_pcb_pid, string_itoa(pid_en_ejecucion));
    t_list* values = dictionary_elements(diccionario_pcb_pid);
    for (int i = 0; i < list_size(values); ++i) {
        t_pcb* p = (t_pcb*) list_get(values, i);
    }
    uint32_t ultimo_tid = obtener_tid_maximo(pcb_en_ejecucion);

    /*
    if (tid_actual > tid_maximo){
        tid_maximo = tid_actual;
    } 
    void* _obtener_maximo(void* a, void* b) {
        int max = 0;
        uint32_t tid_actual = ((t_tcb*)a)->tid;
        if (tid_actual > max) max = tid_actual;
        
    }

    uint32_t ultimo_tid = list_get_maximum(pcb_en_ejecucion->tcbs, _obtener_maximo);
    */

    // Avisar a memoria creacion de hilo nuevo
    uint32_t nuevo_tid = ultimo_tid + 1;

    int new_fd = enviar_hilo_nuevo_a_memoria(pcb_en_ejecucion->pid, nuevo_tid, path);

    // Espero la respuesta de memoria
    op_code respuesta_memoria = recibir_operacion((int)new_fd);
    log_info(debug_logger, "OPERACION RECIBIDA: %d", respuesta_memoria);
    if (respuesta_memoria != MENSAJE_OP_TERMINADA) {
        log_error(debug_logger, "Memoria no pudo crear el hilo");
        abort();
    }
    
    // Creacion TCB
    t_tcb *tcb = tcb_create(nuevo_tid, prioridad, pcb_en_ejecucion->pid);
    list_add(pcb_en_ejecucion->tcbs,tcb);

    // Tomar el permiso para agregar procesos a ready
    sem_wait(&sem_entrada_a_ready);

    // Agrego a ready
    queue_push(cola_ready, tcb);
    
    // Liberar el permiso para agregar procesos a ready
    sem_post(&sem_entrada_a_ready);

    // Avisar que entra un nuevo elemento a ready
    sem_post(&sem_elementos_en_ready);

    log_info(logger_kernel, "## (%d:%d) Se crea el Hilo - Estado: READY", pid_en_ejecucion, tcb->tid);
    // Ahora depende del planificador a largo plazo.
    close(new_fd);
}

void thread_join(uint32_t tid)
{
    t_pcb *pcb_actual = dictionary_get(diccionario_pcb_pid, string_itoa(pid_en_ejecucion));
    t_tcb *tcb_destino = encontrar_tcb_en(pcb_actual, tid);

    // En caso de que el TID pasado por parámetro no exista o ya haya finalizado, esta syscall no hace nada y el hilo que la invocó continuará su ejecución.
    if(tcb_destino != NULL) {
        list_add(tcb_destino->hilos_joineados, tcb_en_ejecucion);
    
        // Agregamos a la cola blocked
        queue_push(cola_blocked, tcb_en_ejecucion);
    }
}

void thread_cancel(uint32_t tid)
{
    printf("Buscando hilo con tid: %u\n", tid);

    // Ver si se encuentra en ejecucion
    if (tid_en_ejecucion != -1 && tid_en_ejecucion == tid) {
        log_info(debug_logger, "Se encontro el hilo en EXEC");
        eliminar_hilo(tcb_en_ejecucion);
        // Desalojar el proceso y eliminarlo cuando vuelva
    } else {
        // Hay que buscarlo en todas las otras colas
        //primero en ready
        bool encontrado = buscar_y_eliminar_en_cola(tid, cola_ready);
        if (encontrado) {
            log_info(logger_kernel, "TID: %d - Estado Anterior: READY - Estado Actual: EXIT", tid);
        }

        if (!encontrado) { // No estaba en ready, buscar en bloqueados
            encontrado = buscar_y_eliminar_en_cola(tid, cola_blocked);
            if (encontrado) {
                log_info(logger_kernel, "TID: %d - Estado Anterior: BLOCKED - Estado Actual: EXIT", tid);
            }
        }

        if (!encontrado) { // No estaba bloqueado por recuros, buscar en io
            encontrado = buscar_y_eliminar_en_cola(tid, cola_io);
            if (encontrado) {
                log_info(logger_kernel, "TID: %d - Estado Anterior: BLOCKED - Estado Actual: EXIT", tid);
            }
        }

        if (!encontrado) { // No se encontro el proceso en ninguna cola
            log_error(debug_logger, "No se encontro el hilo con tid: %u\n", tid);
            abort();
        }
    }

    // Le avisamos a memoria que finaliza un hilo
    int new_fd = crear_conexion(config_kernel->ip_memoria, config_kernel->puerto_memoria);
    log_info(debug_logger, "NUEVO SOCKET PARA MEMORIA: %d", new_fd);
    if (new_fd == -1) {
        log_error(debug_logger, "ERROR AL CREAR CONEXION CON MEMORIA");
        exit(EXIT_FAILURE);
    }

    // Le enviamos a memoria el pid en ejecución y el tid dado por parametro
    t_paquete *paquete = crear_paquete(THREAD_EXIT);
    agregar_a_paquete(paquete, &pid_en_ejecucion, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tid, sizeof(uint32_t));
    enviar_paquete(paquete, new_fd);
    eliminar_paquete(paquete);

    
    // Espero la respuesta de memoria
    op_code respuesta_memoria = recibir_operacion((int)new_fd);
    log_info(debug_logger, "OPERACION RECIBIDA: %d", respuesta_memoria);
    if (respuesta_memoria != MENSAJE_OP_TERMINADA) {
        log_error(debug_logger, "Memoria no pudo terminar el hilo");
        abort();
    }

    log_info(logger_kernel, "## (%d:%d) Finaliza el hilo",pid_en_ejecucion,tid);
    
    close(new_fd);
}

void thread_exit()
{
    thread_cancel(tcb_en_ejecucion->tid);
}

void mutex_create(char *nombre_nuevo_mutex)
{
    // Creamos nuevo mutex, sin tcb asignado inicialmente
    t_mutex *nuevo_mutex = malloc(sizeof(t_mutex));
    nuevo_mutex->nombre_recurso = nombre_nuevo_mutex;
    nuevo_mutex->pid = pid_en_ejecucion;
    nuevo_mutex->tcb_asignado = NULL;
    nuevo_mutex->cola_bloqueados = queue_create();

    // Agregar al diccionario el nombre_mutex con su mutex
    dictionary_put(diccionario_mutex, nombre_nuevo_mutex, nuevo_mutex);

    // Agregamos el nuevo mutex al proceso en ejecucion
    t_pcb *pcb_en_ejecucion = dictionary_get(diccionario_pcb_pid, string_itoa(pid_en_ejecucion));
    list_add(pcb_en_ejecucion->mutexs,nuevo_mutex);
}

bool mutex_lock(char *nombre_mutex)
{
    log_warning(debug_logger, "ENTRO A MUTEX LOCK");
    t_pcb *pcb_en_ejecucion = dictionary_get(diccionario_pcb_pid, string_itoa(pid_en_ejecucion));
    t_mutex *mutex = dictionary_get(diccionario_mutex, nombre_mutex);

    // Verificamos si el mutex existe en proceso en ejecucion
    bool existe_mutex = existe_mutex_en(nombre_mutex, pcb_en_ejecucion->mutexs);
    if(existe_mutex) {
        // Verificamos si el mutex esta tomado por algun hilo
        log_error(debug_logger, "entro al if de existe");
        bool mutex_esta_tomado = esta_tomado(mutex);
        
        // Si no esta tomado, entonces le asignamos el hilo que esta ejecutando
        if(!mutex_esta_tomado) {
            log_error(debug_logger, "entra a que el mutex no está tomado");
            mutex->tcb_asignado = tcb_en_ejecucion;
            return false;
        }
        else {
            queue_push(mutex->cola_bloqueados, tcb_en_ejecucion);
            
            log_info(debug_logger, "## (%d:%d) - Bloqueado por: MUTEX", tcb_en_ejecucion->pid_padre, tcb_en_ejecucion->tid);
            return true;
        }
    }
}

void mutex_unlock(char *nombre_mutex)
{
    t_pcb *pcb_en_ejecucion = dictionary_get(diccionario_pcb_pid, string_itoa(pid_en_ejecucion));
    t_mutex *mutex = dictionary_get(diccionario_mutex, nombre_mutex);

    // Verificamos si el mutex existe en proceso en ejecucion
    bool existe_mutex = existe_mutex_en(nombre_mutex, pcb_en_ejecucion->mutexs);
    if(existe_mutex) {
        // Verificamos si el mutex esta tomado por el hilo que esta ejecutando
        bool mutex_esta_tomado = mutex_esta_tomado_por(tcb_en_ejecucion, mutex);

        if(mutex_esta_tomado) {
            // En caso de que la cola de bloqueados del mutex sea igual a 0 significa que ya no hay ninguno
            if(queue_size(mutex->cola_bloqueados) == 0){
                // el mutex queda sin TCB asignado
                mutex->tcb_asignado = NULL;
            }
            else {
                // Saca al primer elemento de la cola de bloqueados del recurso
                t_tcb *hilo_desbloqueado = queue_pop(mutex->cola_bloqueados);
            
                // Agrega el hilo recien desbloqueado a ready
                sem_wait(&sem_entrada_a_ready);
                queue_push(cola_ready, hilo_desbloqueado);
                sem_post(&sem_entrada_a_ready);
                sem_post(&sem_elementos_en_ready);

                // Asigna el recurso al hilo recien desbloqueado
                mutex->tcb_asignado = hilo_desbloqueado;
            }            
        }
        else {
            log_info(debug_logger, "El hilo que solicito el unlock no posee el recurso especificado.");
        }
    }
}

void dump_memory()
{
    // int new_fd = crear_conexion(config_kernel->ip_memoria, config_kernel->puerto_memoria);
    // log_info(debug_logger, "NUEVO SOCKET PARA MEMORIA: %d", new_fd);
    // if (new_fd == -1) {
    //     log_error(debug_logger, "ERROR AL CREAR CONEXION CON MEMORIA");
    //     exit(EXIT_FAILURE);
    // }

    // Enviamos paquete (a traves de conexion dump_fd creada previamente) con pid y tid a hacer DUMP_MEMORY
    t_paquete *paquete_memoria = crear_paquete(OPCODE_DUMP_MEMORY);
    agregar_a_paquete(paquete_memoria, &pid_en_ejecucion, sizeof(uint32_t));
    agregar_a_paquete(paquete_memoria, &tid_en_ejecucion, sizeof(uint32_t));
    enviar_paquete(paquete_memoria, dump_fd);
    eliminar_paquete(paquete_memoria);

    // Bloqueamos al hilo que solicito
    t_tcb *tcb_a_dumpear = tcb_en_ejecucion;
    queue_push(cola_blocked_dump, tcb_a_dumpear);
    sem_post(&sem_elementos_en_dump);
    
    log_info(logger_kernel, "## (%d:%d) - Bloqueado por: DUMP", pid_en_ejecucion, tid_en_ejecucion);
}

void io(uint32_t milisegundos)
{
    t_hilo_io *hilo_io = malloc(sizeof(t_hilo_io));
    hilo_io->tcb = tcb_en_ejecucion;
    hilo_io->milisegundos = milisegundos;

    // Bloqueamos al hilo pasandolo a la cola de entrada/salida
    queue_push(cola_io, hilo_io);
    log_info(logger_kernel, "## (%d:%d) - Bloqueado por: IO", hilo_io->tcb->pid_padre, hilo_io->tcb->tid);
    
    sem_post(&sem_elementos_en_io);
}

static bool buscar_y_eliminar_en_cola(uint32_t tid, t_queue* cola)
{
    bool encontrado = false;
    t_list_iterator *it = list_iterator_create(cola->elements);

    while (list_iterator_has_next(it) && !encontrado) {
        t_tcb *tcb = list_iterator_next(it);
        if (tcb->tid == tid) {
            encontrado = true;
            log_info(debug_logger, "Se encontro el hilo");
            list_iterator_remove(it);
            eliminar_hilo(tcb);
        }
    }

    list_iterator_destroy(it);
    return encontrado;
}

// Verifica si un nombre de mutex esta en una determinada lista de mutex 
static bool existe_mutex_en(char *mutex_a_buscar, t_list *lista_mutex)
{
    bool encontrado = false;
    // t_list_iterator *it = list_iterator_create(lista_mutex);
    // while (list_iterator_has_next(it) && !encontrado) {
    //     t_mutex *mutex = list_iterator_next(it);
    //     if (strcmp(mutex->nombre_recurso, mutex_a_buscar) == 0) {
    //         encontrado = true;
    //         list_iterator_remove(it);
    //     }
    // }
    // list_iterator_destroy(it);
    
    for (int i = 0; i < list_size(lista_mutex); i++) {
        t_mutex *mutex_analizado = list_get(lista_mutex,i);
        if (strcmp(mutex_analizado->nombre_recurso, mutex_a_buscar) == 0) {
            encontrado = true;
            return encontrado;
        }
    }

    return encontrado;
}

// Verifica si el tcb asignado al mutex no es null
static bool esta_tomado(t_mutex *mutex) 
{
    return mutex->tcb_asignado != NULL;
}

// Verifica si el tcb asignado al mutex tiene el mismo tid que el tcb pasado
static bool mutex_esta_tomado_por(t_tcb *tcb, t_mutex *mutex) 
{
    return mutex->tcb_asignado->tid == tcb->tid;
}

static int enviar_hilo_nuevo_a_memoria(uint32_t pid, uint32_t tid, char *path)
{
    
    int new_fd = crear_conexion(config_kernel->ip_memoria, config_kernel->puerto_memoria);
    log_info(debug_logger, "NUEVO SOCKET PARA MEMORIA: %d", new_fd);
    if (new_fd == -1) {
        log_error(debug_logger, "ERROR AL CREAR CONEXION CON MEMORIA");
        exit(EXIT_FAILURE);
    }

    // Le enviamos a memoria el pid, path al pseudocodigo y tamaño
    t_paquete *paquete = crear_paquete(THREAD_CREATE);
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tid, sizeof(uint32_t));
    agregar_a_paquete(paquete, path, strlen(path) + 1);
    enviar_paquete(paquete, new_fd);
    eliminar_paquete(paquete);

    return new_fd;
}

static void liberar_pcb(uint32_t pid) 
{
    t_pcb *pcb_a_liberar = dictionary_get(diccionario_pcb_pid, string_itoa(pid));
    
    list_destroy_and_destroy_elements(pcb_a_liberar->tcbs, free);
    destruir_mutex_asociados(pcb_a_liberar);
    list_destroy(pcb_a_liberar->mutexs);
}