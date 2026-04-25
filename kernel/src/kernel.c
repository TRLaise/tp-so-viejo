#include "./kernel.h"

/*
** Variables Globales
*/
t_log *debug_logger;
t_log *logger_kernel;
t_config* config;
t_config_kernel* config_kernel;

int tid_en_ejecucion = -1; // -1 Cuando no hay ninguno
int pid_en_ejecucion = -1; // -1 Cuando no hay ninguno
t_tcb *tcb_en_ejecucion;
bool flag_espacio_proceso = true; // Siempre comienza con espacio para procesos

int dump_fd;
int cpu_dispatch_fd;
int cpu_interrupt_fd;

t_dictionary *diccionario_pcb_pid;
t_dictionary *diccionario_mutex;

// Colas
t_queue *cola_new;
t_queue *cola_ready;
t_queue *cola_blocked;
t_queue *cola_exit;
t_queue *cola_io;   // cola de hilos_io (declarado en estructuras.h)
t_queue *cola_blocked_dump; // creamos una lista para los hilos bloqueados por solicitar syscall DUMP_MEMORY

// Semaforos
sem_t sem_elementos_en_new;
sem_t sem_entrada_a_ready;
sem_t sem_elementos_en_ready;
sem_t sem_elementos_en_io;
sem_t sem_elementos_en_dump;

// Funciones locales
static t_algoritmo_planificacion parse_algoritmo_planifiacion(const char *str);

// ./bin/kernel [archivo_pseudocodigo] [tamanio_proceso] [...args]
int main(int argc, char** argv)
{
    iniciar_config();
    iniciar_logger();
    iniciar_conexiones();
    inicializar_colas();
    inicializar_semaforos();
    
    // CREAMOS EL PROCESO INICIAL
    if(argc > 0){
        process_create(argv[1], atoi(argv[2]), 0);
        log_info(debug_logger, "Se creo el proceso inicial.");
    } else {
        log_error(debug_logger, "\nNo se pasaron parametros al main !!!\n");
        abort();
    }
    
    // key->pid : value->pcb
    diccionario_pcb_pid = dictionary_create();
    // key->nombre_recurso : value->mutex
    diccionario_mutex = dictionary_create();

    // Iniciar planificadores
    pthread_t hilo_planificador_largo_plazo;
    int iret = pthread_create(&hilo_planificador_largo_plazo, NULL, planificador_largo_plazo, NULL);
    if (iret != 0) {
        log_error(debug_logger, "No se pudo crear un hilo para el planificador de largo plazo");
    }

    pthread_t hilo_planificador_corto_plazo;
    iret = pthread_create(&hilo_planificador_corto_plazo, NULL, planificador_corto_plazo, NULL);
    if (iret != 0) {
        log_error(debug_logger, "No se pudo crear un hilo para el planificador de corto plazo");
    }

    // Iniciar io
    pthread_t hilo_io;
    iret = pthread_create(&hilo_io, NULL, entrada_salida, NULL);
    if (iret != 0) {
        log_error(debug_logger, "No se pudo crear un hilo para io");
    }

    // Iniciar dump_memory
    pthread_t hilo_dump;
    iret = pthread_create(&hilo_dump, NULL, dump_mem, NULL);
    if (iret != 0) {
        log_error(debug_logger, "No se pudo crear un hilo para dump memory");
    }

    //close(cpu_dispatch_fd);
    //close(cpu_interrupt_fd);
    //close(dump_fd);

    pthread_exit(NULL);
}

void iniciar_logger()
{
    logger_kernel = log_create("./cfg/kernel.log", KERNEL, true, config_kernel->log_level);
    if (logger_kernel == NULL)
        exit(1);

    debug_logger = log_create("./cfg/kernel_debug.log", "KERNEL DEBUG", true, LOG_LEVEL_INFO);
}

void iniciar_config()
{
    config_kernel = malloc(sizeof(t_config_kernel));
    config = config_create("./cfg/kernel.config");

    config_kernel->ip_memoria = get_config_string(config, "IP_MEMORIA");
    config_kernel->puerto_memoria = get_config_string(config, "PUERTO_MEMORIA");
    config_kernel->ip_cpu = get_config_string(config, "IP_CPU");
    config_kernel->puerto_cpu_dispatch = get_config_string(config, "PUERTO_CPU_DISPATCH");
    config_kernel->puerto_cpu_interrupt = get_config_string(config, "PUERTO_CPU_INTERRUPT");
    config_kernel->algoritmo_planificacion = parse_algoritmo_planifiacion(get_config_string(config, "ALGORITMO_PLANIFICACION"));
    config_kernel->quantum = get_config_int(config, "QUANTUM");
    config_kernel->log_level = log_level_from_string(get_config_string(config, "LOG_LEVEL"));
}

void iniciar_conexiones()
{
    cpu_dispatch_fd = crear_conexion(config_kernel->ip_cpu, config_kernel->puerto_cpu_dispatch);
    log_warning(debug_logger, "SOCKET DISPATCH: %d", cpu_dispatch_fd);
    //if (!realizar_handshake(cpu_dispatch_fd)) {
    //    log_info(debug_logger, "Handshake no se pudo realizar (dispatch).");
    //}
    // HANDSHAKE A MANOPLA
    //uint32_t msg = MENSAJE_HANDSHAKE;
    //size_t bytes;

    //bytes = send(cpu_dispatch_fd, &msg, sizeof(uint32_t), 0);
    log_info(debug_logger, "Conexion con CPU DISPATCH establecida!");

    cpu_interrupt_fd = crear_conexion(config_kernel->ip_cpu, config_kernel->puerto_cpu_interrupt);
    log_warning(debug_logger, "SOCKET INTERRUPT: %d", cpu_interrupt_fd);
    //if (!realizar_handshake(cpu_interrupt_fd)) {
    //    log_info(debug_logger, "Handshake no se pudo realizar (interrupt).");
    //}
    //bytes = send(cpu_interrupt_fd, &msg, sizeof(uint32_t), 0);
    log_info(debug_logger, "Conexion con CPU INTERRUPT establecida!");

    dump_fd = crear_conexion(config_kernel->ip_memoria, config_kernel->puerto_memoria);
    //if (!realizar_handshake(memoria_fd)) {
    //    log_info(debug_logger, "Handshake no se pudo realizar (memoria).");
    //}
    log_warning(debug_logger, "SOCKET DUMP: %d", dump_fd);

    log_info(debug_logger, "Conexion con MEMORIA establecida!");
}

void inicializar_colas()
{
    cola_new = queue_create();
    cola_ready = queue_create();
    cola_blocked = queue_create();
    cola_exit = queue_create();
    cola_io = queue_create();
    cola_blocked_dump = queue_create(); 
}

static t_algoritmo_planificacion parse_algoritmo_planifiacion(const char *str)
{
    if (!strcmp(str, "FIFO")) {
        return FIFO;
    } else if (!strcmp(str, "PRIORIDADES")) {
        return PRIORIDADES;
    } else if (!strcmp(str, "CMN")) {
        return CMN;
    }
    log_error(debug_logger, "El algoritmo de planificacion en el config no es valido");
    exit(1);
}

void eliminar_hilo(t_tcb* tcb)
{
    // Desbloquear a los hilos que estaban bloqueados por el join
    devolver_a_ready_joins(tcb);

    // Cambia el campo "tcb_asignado" a NULL de todos los mutex los cuales esta ocupando
    liberar_mutex_asociados(tcb);

    // Saca el tcb del hilo eliminado de la lista de tcbs del pcb del hilo padre
    t_pcb *pcb_padre = dictionary_get(diccionario_pcb_pid, string_itoa(tcb->pid_padre));
    bool removido = list_remove_element(pcb_padre->tcbs, tcb);
    if (!removido){
        log_error(debug_logger, "error con la eliminacion del tcb del hilo en el pcb padre");
    }
    
    tcb_destroy(tcb);
}

void inicializar_semaforos(){
    sem_init(&sem_elementos_en_new, 0, 0);
    sem_init(&sem_entrada_a_ready, 0, 1);
    sem_init(&sem_elementos_en_ready, 0, 0);
    sem_init(&sem_elementos_en_io, 0, 0);
    sem_init(&sem_elementos_en_dump, 0, 0);
}