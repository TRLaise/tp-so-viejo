#include "memoria.h"

t_contexto_ejecucion* obtener_contexto_ejecucion(int socket_conexion)
{
    t_list *info_paquete = recibir_paquete(socket_conexion);
    uint32_t *pid_int = list_get(info_paquete, 0); // sacar pid asociado al paquete
    char *pid = string_itoa(*pid_int);
    uint32_t *tid_int = list_get(info_paquete, 1); // sacar tid asociado al paquete
    char *tid = string_itoa(*tid_int);

    t_contexto_ejecucion* contexto = get_contexto(pid, tid);
    contexto->pid = *pid_int;
    contexto->tid = *tid_int;

    log_info(memoria_logger, "## Contexto Solicitado - (PID-TID) - (%s-%s)", pid, tid);
    
    free(pid);
    free(tid);

    return contexto;
}

void actualizar_contexto_ejecucion(int socket_conexion)
{
    t_list *info_paquete = recibir_paquete(socket_conexion);
    uint32_t *pid_int = list_get(info_paquete, 0); // sacar pid asociado al paquete
    char *pid = string_itoa(*pid_int);
    uint32_t *tid_int = list_get(info_paquete, 1); // sacar tid asociado al paquete
    char *tid = string_itoa(*tid_int);

    t_contexto_ejecucion* contexto_viejo = get_contexto(pid, tid);

    contexto_viejo->registros->ax = *(uint32_t*) list_get(info_paquete, 2); 
    contexto_viejo->registros->bx = *(uint32_t*) list_get(info_paquete, 3);
    contexto_viejo->registros->cx = *(uint32_t*) list_get(info_paquete, 4);
    contexto_viejo->registros->dx = *(uint32_t*) list_get(info_paquete, 5);
    contexto_viejo->registros->ex = *(uint32_t*) list_get(info_paquete, 6);
    contexto_viejo->registros->fx = *(uint32_t*) list_get(info_paquete, 7);
    contexto_viejo->registros->gx = *(uint32_t*) list_get(info_paquete, 8);
    contexto_viejo->registros->hx = *(uint32_t*) list_get(info_paquete, 9);
    contexto_viejo->registros->pc = *(uint32_t*) list_get(info_paquete, 10);

    log_info(memoria_logger, "## Contexto Actualizado - (PID-TID) - (%s-%s)", pid, tid);

    // Liberar memoria
    free(pid);
    free(tid);
}

char *obtener_instruccion(int socket_conexion)
{
    t_list *info_paquete = recibir_paquete(socket_conexion);
    uint32_t *pid_int = list_get(info_paquete, 0); // sacar pid asociado al paquete
    char *pid = string_itoa(*pid_int);
    uint32_t *tid_int = list_get(info_paquete, 1); // sacar tid asociado al paquete
    char *tid = string_itoa(*tid_int);
    uint32_t *pc = list_get(info_paquete, 2); // sacar pc asociado al paquete

    t_contexto_ejecucion* contexto = get_contexto(pid, tid);

    char* instruccion = list_get(contexto->codigo,  (*pc));
    log_info(memoria_logger, "## Obtener instrucción - (PID-TID) - (%s-%s) - Instrucción: %s", pid, tid, instruccion);

    free(pid);
    free(tid);
    return instruccion;
}

void read_mem(int socket_conexion)
{
    t_list *info_paquete = recibir_paquete(socket_conexion);
    uint32_t pid = *(uint32_t*) list_get(info_paquete, 0); // sacar pid asociado al paquete
    uint32_t tid = *(uint32_t*) list_get(info_paquete, 1); // sacar tid asociado al paquete
    uint32_t direc_fisica = *(uint32_t*) list_get(info_paquete, 2); // saca la direccion fisica del paquete (size_t guarda la cantidad de bytes)

    uint32_t valor = leer_de_memoria(direc_fisica);

    enviar_uint32(valor, socket_conexion);
    
    log_info(memoria_logger, "## Lectura - (PID-TID) - (%d-%d) - Dir. Física: %d - Tamaño: %ld", pid, tid, direc_fisica, sizeof(direc_fisica));
}

void* leer_de_memoria(uint32_t dir_fisica)
{
    // uint8_t = 1 byte
    // uint32_t = 4 bytes
    // Para leer los 4 bytes del espacio contiguo de la memoria de usuario se debe castear a uint8_t obtener cada byte
    void* valor = malloc(sizeof(uint32_t));
    memcpy(valor, memoria_usuario->espacio_contiguo + dir_fisica, sizeof(uint32_t));
    
    return valor;
}

void write_mem(int socket_conexion)
{
    t_list *info_paquete = recibir_paquete(socket_conexion);
    uint32_t pid = *(uint32_t*) list_get(info_paquete, 0); // sacar pid asociado al paquete
    uint32_t tid = *(uint32_t*) list_get(info_paquete, 1); // sacar tid asociado al paquete
    uint32_t dir_fisica = *(uint32_t*) list_get(info_paquete, 2); // saca la direccion fisica del paquete (size_t guarda la cantidad de bytes)
    void* valor = list_get(info_paquete, 3);

    memcpy(memoria_usuario->espacio_contiguo + dir_fisica, valor, sizeof(uint32_t));

    t_paquete *paquete_mem = crear_paquete(OK);
    int v_rand = 5;
    agregar_a_paquete(paquete_mem,&v_rand,sizeof(int));
    enviar_paquete(paquete_mem, socket_conexion);
    eliminar_paquete(paquete_mem);  
    
    log_info(memoria_logger, "## Escritura - (PID-TID) - (%d-%d) - Dir. Física: %d - Tamaño: %ld", pid, tid, dir_fisica, sizeof(dir_fisica));
}

t_contexto_ejecucion* get_contexto(char* pid, char* tid)
{
    // concatenar el pid y el tid para usarlo como key en el diccionario
    char *pid_tid = string_new();
    pid_tid = strdup(concatenar((uint32_t) atoi(pid), (uint32_t) atoi(tid)));

    t_contexto_ejecucion *contexto = malloc(sizeof(t_contexto_ejecucion));

    if (dictionary_has_key(contextos, pid_tid)) {
        contexto = dictionary_get(contextos, pid_tid); // Obtener el contexto si existe
    } else {
        log_error(debug_logger, "No existe el contexto en el diccionario para %s", pid_tid);
        return NULL;
    }

    free(pid_tid);

    return contexto;
}

void enviar_contexto_ejecucion(t_contexto_ejecucion* contexto, int socket_conexion)
{   
    t_paquete* paquete = crear_paquete(CONTEXTO_EJECUCION);
    agregar_a_paquete(paquete, &(contexto->base), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->limite), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->tid), sizeof(uint32_t));
    agregar_a_paquete(paquete, contexto->codigo, list_size(contexto->codigo));
    agregar_a_paquete(paquete, &(contexto->registros->ax), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros->bx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros->cx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros->dx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros->ex), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros->fx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros->gx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros->hx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros->pc), sizeof(uint32_t));
    enviar_paquete(paquete, socket_conexion);
    eliminar_paquete(paquete);
}

void enviar_uint32(uint32_t valor, int socket_conexion)
{   
    t_paquete* paquete = crear_paquete(ENVIAR_BYTES);
    agregar_a_paquete(paquete, &(valor), sizeof(uint32_t));
    enviar_paquete(paquete, socket_conexion);
    eliminar_paquete(paquete);
}

void enviar_instruccion(char* instruccion, int socket) 
{
    t_paquete* paquete = crear_paquete(INSTRUCCION);
    agregar_a_paquete(paquete, instruccion, strlen(instruccion)+1);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}