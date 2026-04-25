#include "./memoria.h"

//Loggers
t_log *debug_logger;
t_log *memoria_logger;

//Variables de Config
char *IP_FILESYSTEM;
char *PUERTO_ESCUCHA;
char *PUERTO_FILESYSTEM;
int TAM_MEMORIA;
char *PATH_INSTRUCCIONES;
int RETARDO_RESPUESTA;
t_particion ESQUEMA;
t_algoritmo ALGORITMO_BUSQUEDA;
char **PARTICIONES;
char *LOG_LEVEL;

//Variables Globales
t_dictionary *contextos;
mem_usuario *memoria_usuario;
t_bitarray *bitmap;
int conexion_filesystem;
t_dictionary *pid_tamanio;

int main(int argc, char* argv[]) {

    //Crear logger
    debug_logger = log_create("./cfg/memoria_debug.log", "memoria_debug", true, LOG_LEVEL_INFO);
    memoria_logger = log_create("./cfg/memoria.log", "memoria", true, LOG_LEVEL_INFO);

    //Crear config
    t_config *config = config_create("./cfg/memoria.config");
    if (config == NULL) {
        log_error(debug_logger, "No se pudo crear la config");
        exit(1);
    }

    cargar_config(config);

    inicializar_globales();

    // Conexion con filesystem
    conexion_filesystem = crear_conexion(IP_FILESYSTEM, PUERTO_FILESYSTEM);
    //if (conexion_filesystem == -1) {
    //    log_error(debug_logger, "No se pudo conectar FILESYSTEM");
    //    exit(EXIT_FAILURE);;
    //}
    log_info(debug_logger, "%s¡Conexion con FILESYSTEM exitosa!", VERDE);

    // SERVIDOR
    int server_socket = iniciar_servidor(IP_ESCUCHA, PUERTO_ESCUCHA);

    while (1) {
        int *arg = malloc(sizeof(int));
        *arg = esperar_cliente(server_socket);
        log_info(debug_logger, "%s¡Se conecto un cliente! (socket = %d)", VERDE, *arg);

        pthread_t hilo;
        int iret = pthread_create(&hilo, NULL, atender_conexion, arg);
        if (iret != 0) {
            log_error(debug_logger, "No se pudo crear un hilo para atender la conexion");
            exit(1);
        }
        pthread_detach(hilo);
    }

    return 0;
}

void cargar_config(t_config *config) 
{
    PUERTO_ESCUCHA = get_config_string(config, "PUERTO_ESCUCHA");
    IP_FILESYSTEM = get_config_string(config, "IP_FILESYSTEM");
    PUERTO_FILESYSTEM  = get_config_string(config, "PUERTO_FILESYSTEM");
    TAM_MEMORIA = get_config_int(config, "TAM_MEMORIA");
    PATH_INSTRUCCIONES = get_config_string(config, "PATH_INSTRUCCIONES");
    RETARDO_RESPUESTA = get_config_int(config, "RETARDO_RESPUESTA");
    ESQUEMA = get_esquema(get_config_string(config, "ESQUEMA"));
    ALGORITMO_BUSQUEDA = get_algoritmo(get_config_string(config, "ALGORITMO_BUSQUEDA"));
    PARTICIONES = config_get_array_value(config, "PARTICIONES");
    LOG_LEVEL = get_config_string(config, "LOG_LEVEL");
}

void inicializar_globales(void)
{
    // Diccionario con los contextos de ejecucion
    contextos = dictionary_create();
    pid_tamanio = dictionary_create();

    // Inicializar memoria de usuario
    memoria_usuario = malloc(sizeof(mem_usuario));
    
    memoria_usuario -> espacio_contiguo = malloc(TAM_MEMORIA);
    memoria_usuario -> esquema = ESQUEMA;

    switch (ESQUEMA) {
    case PARTICION_FIJA:
        memoria_usuario->particiones = get_lista_particiones(PARTICIONES);
        // Inicializa el bitmap con tamaño de la lista de particiones dividido 8. Si es 0 redondea para arriba
        //int tam_particiones = list_size(memoria_usuario->particiones);
        //int tam = ceil_div(tam_particiones , 8);
        //char* bits = malloc(tam);
        //memset(bits, 0, TAM_MEMORIA);
        //bitmap = bitarray_create_with_mode(bits, tam, LSB_FIRST);
        break;
    case PARTICION_DINAMICA: 
        memoria_usuario->particiones = list_create();
        // Inicializamos la lista de particiones con una partición que ocupe todo el tamaño
        info_particion *particion_inicial = malloc(sizeof(info_particion));
        particion_inicial->pid = -1;
        particion_inicial->tamanio_particion = (uint32_t) TAM_MEMORIA;
        particion_inicial->esta_ocupada = false;
        particion_inicial->base = 0;
        particion_inicial->limite = TAM_MEMORIA - 1;
        list_add(memoria_usuario->particiones, particion_inicial);

        free(particion_inicial);
        break;
    default:
        log_error(debug_logger, "El esquema de memoria indicado no corresponde");
        break;
    }
}

void *atender_conexion(void* param)
{
    int socket_conexion = *(int*) param;

    op_code cod_op;
    while ((cod_op = recibir_operacion(socket_conexion)) != -1) {
        switch (cod_op) {
            /* Solicitudes de KERNEL */
            case PROCESS_CREATE:
                crear_proceso(socket_conexion);
                break;
            case PROCESS_EXIT:
                finalizar_proceso(socket_conexion);
                break;
            case THREAD_CREATE:
                crear_hilo(socket_conexion);
                break;
            case THREAD_EXIT:
                finalizar_hilo(socket_conexion);
                break;
            case DUMP_MEMORY:
                memory_dump(socket_conexion);
                break;

            /* Solicitudes de CPU */
            case SOLICITAR_CONTEXTO:
                usleep(RETARDO_RESPUESTA * 1000);
                t_contexto_ejecucion *contexto = obtener_contexto_ejecucion(socket_conexion);
                enviar_contexto_ejecucion(contexto, socket_conexion);
                break;
            case ACTUALIZAR_CONTEXTO:
                usleep(RETARDO_RESPUESTA * 1000);
                int rand = 10;
                actualizar_contexto_ejecucion(socket_conexion);
                
                t_paquete *act_contexto = crear_paquete(OK);
                agregar_a_paquete(act_contexto, &rand, sizeof(int));
                enviar_paquete(act_contexto, socket_conexion);
                eliminar_paquete(act_contexto);
                break;
            case SOLICITAR_INSTRUCCION:
                usleep(RETARDO_RESPUESTA * 1000);
                char* instruccion = obtener_instruccion(socket_conexion);
                
                t_paquete *solicitar_instruccion= crear_paquete(INSTRUCCION);
                agregar_a_paquete(solicitar_instruccion, instruccion, strlen(instruccion)+1);
                enviar_paquete(solicitar_instruccion, socket_conexion);
                eliminar_paquete(solicitar_instruccion);
                break;
            case OP_READ_MEM:
                usleep(RETARDO_RESPUESTA * 1000);
                read_mem(socket_conexion); 
                break;
            case OP_WRITE_MEM:
                usleep(RETARDO_RESPUESTA * 1000);
                write_mem(socket_conexion);
                break;
            default:
                log_error(debug_logger, "Operacion no reconocida para el socket %d (operacion = %d) - atender_conexion()", socket_conexion, cod_op);
                break;
        }
    }
}
