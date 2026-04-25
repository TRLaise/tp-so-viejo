#include "kernel.h"

// Estructuras
typedef struct {
    int64_t ms_espera;
    t_tcb *tcb;
    bool cancelado;
} t_parametros_reloj_rr;

// Variables globales
static bool planificar_nuevo_proceso;
static bool *cancelar_ultimo_reloj;    // Permite cancelar el ultimo reloj.
static bool en_ejecucion_ultimo_reloj; // Permite saber si el ultimo reloj ya termino.
static pthread_mutex_t mutex_en_ejecucion_ultimo_reloj;
int continue_flag = 0;
t_dictionary *diccionario_cmn; // prioridad : cola_prioridad

// Definiciones locales
static void *reloj_rr(void *param);
static t_tcb* obtener_tcb_fifo();
static t_tcb* obtener_tcb_prioridades();
static t_tcb* obtener_tcb_cmn();
static char* obtener_key_prioritaria(t_list *keys);
static t_queue* obtener_cola_para_prioridad(uint32_t prioridad); // Obtener o crear cola para "x" prioridad en diccionario_cmn
static void separar_por_prioridad(); // Separar cola_ready por prioridad y guardar en diccionario_cmn
void *revisar_quantum(void *param);

// Motivos de devolucion de pcb
static void manejar_fin_quantum(uint32_t tid_recibido);
static void manejar_out_of_memory(uint32_t tid_recibido);

// Pasa procesos de READY a EXEC
void *planificador_corto_plazo(void *vparams)
{
    planificar_nuevo_proceso = true;

    while (true) {
        // No siempre hay que enviar un proceso nuevo segun el algoritmo de planificacion,
        // a veces devolvemos la ejecucion al proceso devuelto.
        log_warning(debug_logger, "estoy replanificando");
        if (planificar_nuevo_proceso) {
            t_tcb *tcb_a_ejecutar;

            // Esperar que haya elementos en ready.
            
            sem_wait(&sem_elementos_en_ready);

            if(config_kernel->algoritmo_planificacion == FIFO){
                log_warning(debug_logger, "BUSCO EN FIFO");
                tcb_a_ejecutar = obtener_tcb_fifo();
                if (continue_flag == -1) continue;
            }
            else if(config_kernel->algoritmo_planificacion == PRIORIDADES){
                tcb_a_ejecutar = obtener_tcb_prioridades();
                if (continue_flag == -1) continue;
            }
            else if(config_kernel->algoritmo_planificacion == CMN){
                diccionario_cmn = dictionary_create();
                tcb_a_ejecutar = obtener_tcb_cmn();
            }
            else {
                log_error(logger_kernel, "No se inserto un algoritmo de planificacion valido.");
            }

            // Enviamos el tcb a CPU.
            tcb_send(tcb_a_ejecutar, cpu_dispatch_fd);
            pid_en_ejecucion = tcb_a_ejecutar->pid_padre;
            tid_en_ejecucion = tcb_a_ejecutar->tid; // Registrar que proceso esta en ejecucion
            tcb_en_ejecucion = tcb_a_ejecutar;
            log_info(debug_logger, "Se envio un tcb con tid: %d a ejecutar a la CPU.", tid_en_ejecucion);

            // Si estamos en CMN iniciar el reloj.
            if (config_kernel->algoritmo_planificacion == CMN) {

                log_info(debug_logger, "Iniciando reloj RR con q=%d para tid=%u", config_kernel->quantum, tcb_a_ejecutar->tid);

                pthread_t thread_quantum;
                if (pthread_create(&thread_quantum, NULL, revisar_quantum, tcb_en_ejecucion) != 0)
                {
                    printf("Error al crear hilo quantum");
                    exit(EXIT_FAILURE);
                }
                pthread_detach(thread_quantum);
            }
        
            continue_flag = 0;
        }

        /*
         En este punto, se esta ejecutando el proceso, esperamos que interrumpa y
         nos devuelvan el tcb actualizado y el motivo.
         
            - El primer elemento de la respuesta de dispatch es el tcb_actualizado
            - El segundo elemento de la respuesta de dispatch es el op_code correspondiente (ya sea para interrupciones o syscalls)
        */
        op_code respuesta_cpu = recibir_operacion(cpu_dispatch_fd);
        log_info(debug_logger, "Respuesta de cpu: %d", respuesta_cpu);

        t_list *elementos = recibir_paquete(cpu_dispatch_fd); // Bloqueante
        op_code operacion = *(op_code*) list_get(elementos, 0); // Obtener motivo

        // Despues de recibir el tcb actualizado, cancelar el hilo del reloj, en caso de que siga corriendo.
        pthread_mutex_lock(&mutex_en_ejecucion_ultimo_reloj);
        if (en_ejecucion_ultimo_reloj) {
            // Significa que la estructura de parametros no fue liberada, podemos pedirle que cancele
            // sin segfaultear.
            *cancelar_ultimo_reloj = true;
            en_ejecucion_ultimo_reloj = false;
            log_info(debug_logger, "Se recibio un tcb y el reloj seguia en ejecucion, pidiendole que cancele...");
        }
        pthread_mutex_unlock(&mutex_en_ejecucion_ultimo_reloj);


        log_info(debug_logger, "Se recibio un TCB de CPU con el motivo: %d", operacion);

        switch (operacion) {
        //////////////////
        //INTERRUPCIONES//
        //////////////////

        case FIN_QUANTUM:
            manejar_fin_quantum(*(uint32_t *) list_get(elementos, 1));
            planificar_nuevo_proceso = true;
            break;
        case OUT_OF_MEMORY:
            manejar_out_of_memory(*(uint32_t *) list_get(elementos, 1));
            planificar_nuevo_proceso = true;
            break;
        case DESALOJO_HILO:
            planificar_nuevo_proceso = true;
            log_warning(debug_logger, "PLANIFICAR NUEVO PROCESO == TRUE");
            break;

        ////////////
        //SYSCALLS//
        ////////////

        // El parametro 0 de la lista de elementos es el TID_RECIBIDO
        case PROCESS_CREATE:
        // enviar_syscall(PROCESS_CREATE, param1, param2, param3);
            process_create(list_get(elementos, 1), (uint32_t) (atoi((char*)list_get(elementos, 2))), (uint32_t) (atoi((char*)list_get(elementos, 3))));
            planificar_nuevo_proceso = false;
            break;
        case PROCESS_EXIT:
        // enviar_syscall(PROCESS_EXIT);
            process_exit();
            planificar_nuevo_proceso = true;
            break;
        case THREAD_CREATE:
        // enviar_syscall(THREAD_CREATE, param1, param2, NULL);
            char *prioridad_a_crear = (char*) list_get(elementos, 2);
            thread_create(list_get(elementos, 1), (uint32_t) (atoi(prioridad_a_crear)));
            planificar_nuevo_proceso = false;
            break;
        case THREAD_JOIN:
        // enviar_syscall(THREAD_JOIN, param1, NULL, NULL);
            char *tid_join = (char*) list_get(elementos, 1);
            log_warning(debug_logger,"EL TID RECIBIDO ES: %s", tid_join);
            thread_join((uint32_t) (atoi(tid_join)));
            planificar_nuevo_proceso = true;
            break;
        case THREAD_CANCEL:
        // enviar_syscall(THREAD_CANCEL, param1, NULL, NULL);
            char *tid_cancel = (char*) list_get(elementos, 1);
            thread_cancel((uint32_t) (atoi(tid_cancel)));
            planificar_nuevo_proceso = true;
            break;
        case THREAD_EXIT:
        // enviar_syscall(THREAD_EXIT, NULL, NULL, NULL);
            thread_exit();
            planificar_nuevo_proceso = true;
            break;
        case MUTEX_CREATE:
        // enviar_syscall(MUTEX_CREATE, param1, NULL, NULL);
            mutex_create((char*) list_get(elementos, 1));
            planificar_nuevo_proceso = false;
            break;
        case MUTEX_LOCK:
        // enviar_syscall(MUTEX_LOCK, param1, NULL, NULL);
            int random = 10;
        ///TODO: FALTA VERIFICAR SI SE BLOQUEO O NO PARA PODER DECIR SI HAY QUE REPLANIFICAR
            bool evacuate_flag = mutex_lock((char*) list_get(elementos, 1));
            if (evacuate_flag) {
                t_paquete* pack = crear_paquete(DESALOJO_HILO);
                agregar_a_paquete(pack, &random, sizeof(int));
                enviar_paquete(pack, cpu_dispatch_fd);
                eliminar_paquete(pack);
                planificar_nuevo_proceso = true;
            } else {
                t_paquete* pack = crear_paquete(OK);
                agregar_a_paquete(pack, &random, sizeof(int));
                enviar_paquete(pack, cpu_dispatch_fd);
                eliminar_paquete(pack);
                planificar_nuevo_proceso = false;
            }
            break;
        case MUTEX_UNLOCK:
        // enviar_syscall(MUTEX_UNLOCK, param1, NULL, NULL);   
            mutex_unlock((char*) list_get(elementos, 1));
            planificar_nuevo_proceso = false;
            break;
        case DUMP_MEMORY:
        // enviar_syscall(DUMP_MEMORY, NULL, NULL, NULL);
            dump_memory();
            planificar_nuevo_proceso = true;
            break;
        case IO:
        // enviar_syscall(IO, param1, NULL, NULL);
            char *milisegundos = (char*) list_get(elementos, 1);
            io((uint32_t) atoi(milisegundos));
            planificar_nuevo_proceso = true;
            break;
        default:
            log_error(debug_logger, "Motivo de desalojo no reconocido.");
            break;
        }
    }
}

/*
    RELOJ RR
*/
static void *reloj_rr(void *param)
{
    int64_t ms_espera = ((t_parametros_reloj_rr *)param)->ms_espera;
    t_tcb *tcb_reloj = ((t_parametros_reloj_rr *)param)->tcb;
    assert(ms_espera >= 0);

    // Esperar
    usleep(ms_espera * 1000);

    // Solo enviar la interrupcion si este reloj no fue cancelado mientras esperaba
    bool cancelar_reloj = ((t_parametros_reloj_rr *)param)->cancelado;
    if (!cancelar_reloj) {
        log_info(debug_logger,
                 "Reloj para pid=%u termino despues de esperar %ldms, enviando interrupcion",
                 tcb_reloj->tid,
                 ms_espera);

        // Desalojar el proceso
        op_code motivo = FIN_QUANTUM;
        t_paquete *paquete = crear_paquete(INTERRUPCION);
        // agregar_a_paquete(paquete, &tcb_reloj->pid_padre, sizeof(uint32_t));
        // agregar_a_paquete(paquete, &tcb_reloj->tid, sizeof(uint32_t));
        agregar_a_paquete(paquete, &motivo, sizeof(op_code));
        enviar_paquete(paquete, cpu_interrupt_fd);
        eliminar_paquete(paquete);

        // Avisar que este reloj termino su ejecucion,
        // si fue cancelado, en_ejecucion_ultimo reloj es setteado a falso cuando se cancela
        pthread_mutex_lock(&mutex_en_ejecucion_ultimo_reloj);
        en_ejecucion_ultimo_reloj = false;
        pthread_mutex_unlock(&mutex_en_ejecucion_ultimo_reloj);
    } else {
        log_info(debug_logger, "El reloj para tid=%u fue cancelado", tcb_reloj->tid);
    }

    free(param);

    return NULL;
}

void *revisar_quantum(void *param)
{
    t_tcb *tcb = (t_tcb *)param;
    int quantum = config_kernel->quantum;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    usleep(quantum * 1000);

    // Desalojar el proceso
    op_code motivo = FIN_QUANTUM;
    t_paquete *paquete = crear_paquete(INTERRUPCION);
    agregar_a_paquete(paquete, &motivo, sizeof(op_code));
    enviar_paquete(paquete, cpu_interrupt_fd);
    eliminar_paquete(paquete);
}

/*

MANEJO ALGORITMOS PLANIFICACION
(devuelven el tcb_a_ejecutar)

*/

// Caso planificacion por FIFO
static t_tcb* obtener_tcb_fifo()
{
    t_tcb *tcb_a_ejecutar;
    if (queue_is_empty(cola_ready)) {
        tcb_a_ejecutar = NULL;
        log_warning(debug_logger, "NO SE ENCONTRO NINGUN HILO EN READY PARA FIFO: %d",tcb_a_ejecutar->tid);
    } else {
        tcb_a_ejecutar = queue_pop(cola_ready);
    }

    // Si no tenemos ningun proceso, no hacer nada
    if (tcb_a_ejecutar == NULL) {
        continue_flag = -1;
    }

    return tcb_a_ejecutar;
}

// Caso planificacion por PRIORIDADES
static t_tcb* obtener_tcb_prioridades()
{
    t_tcb *tcb_a_ejecutar;
    if (queue_is_empty(cola_ready)) {
        tcb_a_ejecutar = NULL;
    } else {
        tcb_a_ejecutar = obtener_tcb_maxima_prioridad(cola_ready);
    }
    
    bool removed = list_remove_element(cola_ready->elements, tcb_a_ejecutar);
    if(removed == false){
        log_error(debug_logger, "No se encontro un tcb a remover en prioridades.");
    }
    
    // Si no tenemos ningun proceso, no hacer nada
    if (tcb_a_ejecutar == NULL) {
        continue_flag = -1;
    }
    
    return tcb_a_ejecutar;
}

// Caso planificacion por COLAS MULTINIVEL
static t_tcb* obtener_tcb_cmn() 
{   
    bool tcb_encontrado = false;
    t_queue *cola_prioritaria = queue_create();

    // Preparar todas las colas de prioridades
    separar_por_prioridad();
    
    // Obtener todas keys para buscar la cola con mas prioridad
    t_list *keys = dictionary_keys(diccionario_cmn);

    while(!tcb_encontrado){
        char *key_mayor_prio = obtener_key_prioritaria(keys);

        cola_prioritaria = dictionary_get(diccionario_cmn, key_mayor_prio);
        
        // Si la cola_mayor_prioridad esta vacia vuelvo a buscar la segunda lista con mayor prioridad
        if(list_is_empty(cola_prioritaria->elements)){
            list_remove_element(keys, key_mayor_prio);

            continue;
        }

        tcb_encontrado = true;
    }
    
    // Sacamos al tcb de ready y de la cola_prioritaria
    t_tcb *tcb_a_ejecutar = queue_pop(cola_prioritaria);
    
    bool removido = list_remove_element(cola_ready->elements, tcb_a_ejecutar);
    if (!removido){
        log_error(debug_logger, "error al remover tcb de cola ready en CMN");
    }

    // Libero memoria de keys
    list_destroy_and_destroy_elements(keys, (void*) free);

    // Devuelve el tcb con mayor prioridad
    return tcb_a_ejecutar;
}

// Recibe una lista de arrays, los pasa a int y devuelve el de menor valor como array
static char* obtener_key_prioritaria(t_list *keys)
{
    t_list_iterator *it = list_iterator_create(keys);

    int prioridad_minima = -1;
    while (list_iterator_has_next(it)) {
        int prioridad = atoi(list_iterator_next(it));
        if (prioridad_minima > prioridad || prioridad_minima < 0) {
            prioridad_minima = prioridad;
        }
    }

    list_iterator_destroy(it);

    return string_itoa(prioridad_minima);
}



/*******************************
********************************
*UTILS PLANIFICADOR CORTO PLAZO*
********************************
*******************************/

// Obtener o crear cola para "x" prioridad en diccionario_cmn
static t_queue* obtener_cola_para_prioridad(uint32_t prioridad)
{
    // Clave tiene que ser array
    char *clave = string_itoa(prioridad);

    // Obtengo la cola, si es nula la creamos y agregamos al diccionario
    t_queue *cola = dictionary_get(diccionario_cmn, clave);
    if (cola == NULL) {
        cola = queue_create();
        dictionary_put(diccionario_cmn, clave, cola);
    }
    return cola;
}

// Separar cola_ready por prioridad y guardar en diccionario_cmn
static void separar_por_prioridad() 
{
    //t_queue *ready_actual = cola_ready;
    t_queue *ready_actual = queue_create();
    list_add_all(ready_actual->elements, cola_ready->elements);
    
    int tamanio_cola_ready = list_size(ready_actual->elements);
    for(int i=0; i < tamanio_cola_ready; i++) {
        t_tcb *tcb = queue_pop(ready_actual);
        t_queue *cola = obtener_cola_para_prioridad(tcb->prioridad);
        queue_push(cola, tcb);
    }
}

/********************************
*********************************
*******MANEJO POR DESALOJO*******
*********************************
********************************/

static void manejar_fin_quantum(uint32_t tid_recibido)
{
    t_pcb *pcb_en_ejecucion = dictionary_get(diccionario_pcb_pid, string_itoa(pid_en_ejecucion));
    t_tcb *tcb_recibido = encontrar_tcb_en(pcb_en_ejecucion, tid_recibido);

    // Lo volvemos a agregar a la cola de READY
    queue_push(cola_ready, tcb_recibido);

    // Logs
    log_info(logger_kernel, "## (%d : %d) - Desalojado por fin de Quantum", pid_en_ejecucion, tid_recibido);
    log_info(logger_kernel, "TID: %d - Estado Anterior: EXEC - Estado Actual: READY", tid_recibido);

    sem_post(&sem_elementos_en_ready);
}

static void manejar_out_of_memory(uint32_t tid_recibido)
{
    log_info(logger_kernel, "TID: %d - Estado Anterior: EXEC - Estado Actual: EXIT", tid_recibido);
    log_info(logger_kernel, "Finaliza el proceso %d", tid_recibido);

    // Si hay out_of_memory significa que un proceso intento escribir en espacio de otro proceso, 
    // seg_fault == abortar
    log_error(debug_logger, "Segmentation Fault - Out of Memory");
    abort();
}