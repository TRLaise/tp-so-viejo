#include "./cpu.h"

t_log* cpu_logger;
t_log* debug_logger;

t_config* config;
t_config_cpu* config_cpu;

int socket_escucha_dispatch;
int socket_escucha_interrupt;

bool interrupt_flag;
bool exec_flag;
bool evacuate_flag;

pthread_mutex_t mx_interrupt;

t_list* tlb;

int dispatch_fd;
int interrupt_fd;
int socket_cpu_dispatch;
int socket_cpu_interrupt;
int conexion_memoria;

op_code motivo;

sem_t sem_exec;

int main(int argc, char* argv[]) 
{
    interrupt_flag = false;
    sem_init(&sem_exec, 0, 0);
    pthread_mutex_init(&mx_interrupt, NULL);

    iniciar_config();
    iniciar_logger();

    /* Conexion MEMORIA */
    conexion_memoria = crear_conexion(config_cpu->ip_memoria, config_cpu->puerto_memoria);
    log_info(debug_logger, "%s¡Conexion con MEMORIA exitosa!", VERDE);


    /* Server DISPATCH */
    pthread_t hilo_dispatch;
    socket_escucha_dispatch = iniciar_servidor(IP_ESCUCHA, config_cpu->puerto_escucha_dispatch);
    if(pthread_create(&hilo_dispatch, NULL, server_dispatch, NULL) != 0) {
        log_error(debug_logger, "No se pudo crear el hilo para el server (dispatch).");
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_dispatch);

    /* Server INTERRUPT */
    socket_escucha_interrupt = iniciar_servidor(IP_ESCUCHA, config_cpu->puerto_escucha_interrupt);
    server_interrupt();

    // Cerrar sockets
    //liberar_conexion(conexion_memoria);

    // Liberar memoria
    terminar_programa();
    return 0;
}

/***** INITS *****/

void iniciar_config() 
{
    config_cpu = malloc(sizeof(t_config_cpu));

    t_config *config = config_create("./cfg/cpu.config");

    config_cpu->ip_memoria               = get_config_string(config, "IP_MEMORIA");
    config_cpu->puerto_memoria           = get_config_string(config, "PUERTO_MEMORIA");
    config_cpu->puerto_escucha_dispatch  = get_config_string(config, "PUERTO_ESCUCHA_DISPATCH");
    config_cpu->puerto_escucha_interrupt = get_config_string(config, "PUERTO_ESCUCHA_INTERRUPT");
    config_cpu->log_level                = log_level_from_string(get_config_string(config, "LOG_LEVEL"));
}

void iniciar_logger()
{
    debug_logger = log_create("./cfg/cpu_debug.log", "CPU DEBUG", true, LOG_LEVEL_INFO);
    cpu_logger = log_create("./cfg/cpu.log", "CPU", true, config_cpu->log_level);
}

/***** SERVERS *****/

void* server_dispatch()
{
    op_code cod_op;
    dispatch_fd = esperar_cliente(socket_escucha_dispatch);
    log_info(debug_logger, "%s¡Se conecto un cliente por DISPATCH!", VERDE);

    while ((cod_op = recibir_operacion(dispatch_fd)) != -1) {
        switch (cod_op) {
            case INIT_PID_TID: {

                t_tcb* tcb = malloc(sizeof(t_tcb));
                tcb = recibir_tcb(dispatch_fd);

                t_contexto_ejecucion* context = malloc(sizeof(context));
                context = solicitar_contexto_ejecucion(conexion_memoria, tcb->pid_padre, tcb->tid);

                /* Log obligatorio */
                log_info(cpu_logger, "## TID: <%d> - Solicito Contexto Ejecucion.", tcb->tid);

                inicio_ciclo_de_instrucciones(context);
                tcb_destroy(tcb);
                break;
            }
            default: {
                printf("escuchar_dispatch() - operación desconocida %d\n", cod_op);
                break;
            }
        }
    }
}

void* server_interrupt()
{
    op_code cod_op;
    interrupt_fd = esperar_cliente(socket_escucha_interrupt);
    log_info(debug_logger, "%s¡Se conecto un cliente por INTERRUPT!", VERDE);

    while ((cod_op = recibir_operacion(interrupt_fd)) != -1) {
        switch (cod_op) {
            case INTERRUPCION:
            {
                motivo = recibir_motivo_exit();

                pthread_mutex_lock(&mx_interrupt);
                interrupt_flag = true;
                pthread_mutex_unlock(&mx_interrupt);

                /* Log Obligatorio */
                log_info(cpu_logger, "## Llega interrrupcion al puerto Interrupt.");
                
                break;
            }
            default:
            {
                printf("escuchar_interrupt() - operacion desconocida %d\n", cod_op);
                break;
            }
        }
    }
}

////////////////////////////////////
////// CICLO DE INSTRUCCIONES //////
////////////////////////////////////

void inicio_ciclo_de_instrucciones(t_contexto_ejecucion* context)
{
	exec_flag = true;
    while(exec_flag) {

        evacuate_flag = false;
        fetch(context);

        if (evacuate_flag) {
            log_warning(debug_logger, "Se ejecuto una SYSCALL, desalojando hilo...");
            exec_flag = false;
        } else if (interrupt_flag) {
            switch (motivo) {
                case FIN_QUANTUM /*|| DESALOJO_HILO || OUT_OF_MEMORY*/: {
                    actualizar_contexto_ejecucion(context);
                    enviar_tid(context->tid);
                    exec_flag = false;
                    // Tiene sentido poner la interrupcion en false aca por que se supone que despues de la interrupcion, 
                    // la exec flag queda en false, por lo tanto se corta el ciclo de instruccion,
                    // vuelve el mando al kernel y cpu se queda esperando al proximo tid a ejecutar en dispatch
                    pthread_mutex_lock(&mx_interrupt);
                    interrupt_flag = false;
                    pthread_mutex_unlock(&mx_interrupt);

                    //return;
                    break;
                }
                default: {
                    log_error(debug_logger, "Interrupcion con motivo desconocido (motivo = %d)", motivo);
                    break;
                }
            }
        }
    }
}

void fetch(t_contexto_ejecucion* context)
{
    /* Log Obligatorio */
    log_info(cpu_logger, "## TID: <%d> - FETCH - Program Counter <%d>", context->tid, context->registros->pc);
    
    char* linea = solicitar_instruccion(conexion_memoria, context->pid, context->tid, context->registros->pc);
    t_instruccion* instruccion = parsear_linea(linea);

    decode(context, instruccion);
}

void decode(t_contexto_ejecucion* context, t_instruccion* instruccion)
{
    //bool auto_incremento_pc = true;
    op_code cod_instruccion = parsear_instruccion(instruccion->nombre);

    char* param1 = string_new();
    char* param2 = string_new();
    char* param3 = string_new();

    /* Log Obligatorio */
    log_info(cpu_logger, "## TID: <%d> - Ejecutando: %s - %s", context->tid, instruccion->nombre, imprimir_parametros(instruccion->params));

    switch (cod_instruccion) {
        case SET:
        {
            param1 = (char*) list_get(instruccion->params, 0);
            param2 = (char*) list_get(instruccion->params, 1);
            if (string_equals_ignore_case(param1, "PC")) {
                context->registros->pc = (uint32_t) atoi(param2);
                //auto_incremento_pc = false;
            }
            else {
                ++context->registros->pc;
                set(context, param1, (uint32_t) atoi(param2));
            }

            
            break;
        }
        case READ_MEM:
        {
            param1 = (char*) list_get(instruccion->params, 0);
            param2 = (char*) list_get(instruccion->params, 1);

            ++context->registros->pc;
            read_mem(context, param1, param2);
            
            break;
        }
        case WRITE_MEM:
        {
            param1 = (char*) list_get(instruccion->params, 0);
            param2 = (char*) list_get(instruccion->params, 1);

            ++context->registros->pc;
            write_mem(context, param1, param2);
            
            break;
        }
        case SUM:
        {
            param1 = (char*) list_get(instruccion->params, 0);
            param2 = (char*) list_get(instruccion->params, 1);

            ++context->registros->pc;
            sum(context, param1, param2);
            
            break;
        }
        case SUB:
        {
            param1 = (char*) list_get(instruccion->params, 0);
            param2 = (char*) list_get(instruccion->params, 1);

            ++context->registros->pc;
            sub(context, param1, param2);
            
            break;
        }
        case JNZ:
        {
            param1 = (char*) list_get(instruccion->params, 0);
            param2 = (char*) list_get(instruccion->params, 1);

            if (obtener_valor_registro(context, param1) != 0) {
                context->registros->pc = (uint32_t) atoi(param2);
                //auto_incremento_pc = false;
            }
            else {
                ++context->registros->pc;
            }
            
            break;
        }
        case LOG:
        {
            ++context->registros->pc;
            uint32_t valor_obtenido = obtener_valor_registro(context, (char*) list_get(instruccion->params, 0));
            log_trace(cpu_logger, "%d", valor_obtenido);

            break;
        }
        case DUMP_MEMORY:
        {
            ++context->registros->pc;
            actualizar_contexto_ejecucion(context);
            enviar_syscall(DUMP_MEMORY, NULL, NULL, NULL);

            evacuate_flag = true;
            break;
        }
        case IO:
        {
            param1 = (char*) list_get(instruccion->params, 0);

            ++context->registros->pc;
            actualizar_contexto_ejecucion(context);
            enviar_syscall(IO, param1, NULL, NULL);

            evacuate_flag = true;
            break;
        }
        case PROCESS_CREATE:
        {
            param1 = (char*) list_get(instruccion->params, 0);
            param2 = (char*) list_get(instruccion->params, 1);
            param3 = (char*) list_get(instruccion->params, 2);
            
            ++context->registros->pc;
            actualizar_contexto_ejecucion(context);
            enviar_syscall(PROCESS_CREATE, param1, param2, param3);

            break;
        }
        case THREAD_CREATE:
        {
            param1 = (char*) list_get(instruccion->params, 0);
            param2 = (char*) list_get(instruccion->params, 1);

            ++context->registros->pc;
            actualizar_contexto_ejecucion(context);
            enviar_syscall(THREAD_CREATE, param1, param2, NULL);
            
            break;
        }
        case THREAD_JOIN:
        {
            param1 = (char*) list_get(instruccion->params, 0);

            ++context->registros->pc;
            actualizar_contexto_ejecucion(context);
            enviar_syscall(THREAD_JOIN, param1, NULL, NULL);

            evacuate_flag = true;
            break;
        }
        case THREAD_CANCEL:
        {
            param1 = (char*) list_get(instruccion->params, 0);

            ++context->registros->pc;
            actualizar_contexto_ejecucion(context);
            enviar_syscall(THREAD_CANCEL, param1, NULL, NULL);

            break;
        }
        case MUTEX_CREATE:
        {
            param1 = (char*) list_get(instruccion->params, 0);

            ++context->registros->pc;
            actualizar_contexto_ejecucion(context);
            enviar_syscall(MUTEX_CREATE, param1, NULL, NULL);

            break;
        }
        case MUTEX_LOCK:
        {
            param1 = (char*) list_get(instruccion->params, 0);

            ++context->registros->pc;
            enviar_syscall(MUTEX_LOCK, param1, NULL, NULL);

            if (recibir_desalojo()) {
                actualizar_contexto_ejecucion(context);
                evacuate_flag = true;
            }
            break;
        }
        case MUTEX_UNLOCK:
        {
            param1 = (char*) list_get(instruccion->params, 0);

            ++context->registros->pc;
            actualizar_contexto_ejecucion(context);
            enviar_syscall(MUTEX_UNLOCK, param1, NULL, NULL);

            break;
        }
        case THREAD_EXIT:
        {
            ++context->registros->pc;
            actualizar_contexto_ejecucion(context);
            enviar_syscall(THREAD_EXIT, NULL, NULL, NULL);

            evacuate_flag = true;
            break;
        }
        case PROCESS_EXIT:
        {
            ++context->registros->pc;
            actualizar_contexto_ejecucion(context);
            enviar_syscall(PROCESS_EXIT, NULL, NULL, NULL);

            evacuate_flag = true;
            break;
        }
        default:
        {
            log_error(debug_logger, "decode() - instrucción invalida %s", instruccion->nombre);
            evacuate_flag = true;
            break;
        }
    }

    //if (auto_incremento_pc)

    liberar_instruccion(instruccion);
    if (!string_is_empty(param1) && param1) free(param1);
    if (!string_is_empty(param2) && param2) free(param2);
    if (!string_is_empty(param3) && param3) free(param3);
}

void liberar_instruccion(t_instruccion* instruccion)
{
    free(instruccion->nombre);
    //list_destroy_and_destroy_elements(instruccion->params, free);
    free(instruccion);
}

void terminar_programa()
{
    //close(*socket_escucha);
    //close(*socket_escucha);
    log_destroy(debug_logger);
    log_destroy(cpu_logger);
    //config_destroy(config_cpu);
}