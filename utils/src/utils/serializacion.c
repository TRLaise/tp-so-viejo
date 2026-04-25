#include "./serializacion.h"


/*
void enviar_int(uint32_t mensaje, int socket_conexion)
{
    t_paquete *paquete = malloc(sizeof(t_paquete));

    log_info(debug_logger,"ENVIAR INT QUIERE ENVIAR: %d, en el socket %d", mensaje, socket_conexion);

    paquete->codigo_operacion = OP_MENSAJE_INT;
    paquete->buffer = malloc(sizeof(t_buffer));
    paquete->buffer->size = sizeof(uint32_t);
    paquete->buffer->stream = malloc(sizeof(uint32_t));
    *((uint32_t *)paquete->buffer->stream) = mensaje;

    int bytes = paquete->buffer->size + 2 * sizeof(int);

    void *a_enviar = serializar_paquete(paquete, bytes);

    send(socket_conexion, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}


uint32_t recibir_int(int socket_conexion, bool *error)
{
    // Verificar que se envió un int
    int ret = recibir_operacion(socket_conexion);
    log_error(debug_logger, "RECIBIENDO UN INT: %d", ret);
    if (ret != OP_MENSAJE_INT) {
        log_warning(debug_logger, "recibir_int: Error al recibir la operacion en el socket %d", socket_conexion);
        *error = true;
        return -1;
    }

    int size;
    uint32_t *buffer = recibir_buffer(&size, socket_conexion);
    uint32_t valor = *buffer;
    free(buffer);

    return valor;
}
*/
void enviar_mensaje(char *mensaje, int socket_conexion)
{
    t_paquete *paquete = malloc(sizeof(t_paquete));

    paquete->codigo_operacion = MENSAJE;
    paquete->buffer = malloc(sizeof(t_buffer));
    paquete->buffer->size = strlen(mensaje) + 1;
    paquete->buffer->stream = malloc(paquete->buffer->size);
    memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

    int bytes = paquete->buffer->size + 2 * sizeof(int);

    void *a_enviar = serializar_paquete(paquete, bytes);
    
    //log_warning(debug_logger, "A ENVIAR: %s", (char*) a_enviar);

    send(socket_conexion, a_enviar, bytes, 0);

    free(a_enviar);
    eliminar_paquete(paquete);
}

char *recibir_mensaje(int socket_conexion)
{
    int size;
    char *buffer = recibir_buffer(&size, socket_conexion);
    return buffer;
}

void enviar_paquete(t_paquete *paquete, int socket_conexion)
{
    int bytes = paquete->buffer->size + 2 * sizeof(int);
    void *a_enviar = serializar_paquete(paquete, bytes);

    send(socket_conexion, a_enviar, bytes, 0);

    free(a_enviar);
}

// int recibir_operacion(int socket_conexion)
// {
//     int cod_op;
//     
//     if (recv(socket_conexion, &cod_op, (u_int32_t) sizeof(int), MSG_WAITALL) > 0) {
//         log_info(debug_logger, "recv no falla");
//         return cod_op;
//     }
//     else {
//         close(socket_conexion);
//         return -1;
//     }
// }

int recibir_operacion(int socket_cliente)
{
    int cod_op;
    //printf("cod_op: %d \n", cod_op);
    //printf("&cod_op: %d \n", &cod_op);
    //printf("socket_cliente: %d \n", socket_cliente);
    if(socket_cliente < 0){
        perror("Error al recibir el codigo de operacion");
        exit(EXIT_FAILURE);
    }
    ssize_t bytes_recibidos = recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL);
    //printf("bytes_recibidos: %d \n", bytes_recibidos);
    if (bytes_recibidos > 0) {
        return cod_op;
    } else if (bytes_recibidos == 0) {
        printf("El cliente cerró la conexión. \n");
    } else {
        //printf("%s",strerror(errno));
        log_error(debug_logger, "FALLO RECIBIR OP, EN EL SOCKET %d", socket_cliente);
    }
    close(socket_cliente);
    return -1;
}

void *recibir_buffer(int *size, int socket_conexion)
{
    void *buffer;

    recv(socket_conexion, size, sizeof(int), MSG_WAITALL);
    buffer = malloc(*size);
    recv(socket_conexion, buffer, *size, MSG_WAITALL);

    return buffer;
}

t_list *recibir_paquete(int socket_conexion)
{
    int size;
    int desplazamiento = 0;
    void *buffer;
    t_list *valores = list_create();
    int tamanio;

    buffer = recibir_buffer(&size, socket_conexion);
    while (desplazamiento < size) {
        memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
        desplazamiento += sizeof(int);
        char *valor = malloc(tamanio);
        memcpy(valor, buffer + desplazamiento, tamanio);
        desplazamiento += tamanio;
        list_add(valores, valor);
    }
    free(buffer);
    return valores;
}

/******************************************/
/*************** Handshakes ***************/
/******************************************/

/*
bool realizar_handshake(int socket_conexion)
{
    size_t bytes;

    // Enviar handshake
    uint32_t msg = MENSAJE_HANDSHAKE;
    bytes = send(socket_conexion, &msg, sizeof(uint32_t), 0);
    if (bytes <= 0) {
        log_error(debug_logger, "No se pudo enviar el handshake");
        exit(1);
    }
    uint32_t respuesta;
    recv(socket_conexion, &respuesta, sizeof(uint32_t), MSG_WAITALL);

    // Verifico que la respuesta sea la correcta
    return respuesta == RESPUESTA_HANDSHAKE_OK;
}

bool recibir_handshake(int socket_conexion)
{
    uint32_t mensaje_recibido;
    ssize_t bytes = recv(socket_conexion, &mensaje_recibido, sizeof(uint32_t), MSG_WAITALL);
    if (bytes <= 0) {
        log_error(debug_logger, "Hubo un error recibiendo el handshake");
        exit(1);
    }

    // Si el mensaje recibido es correcto
    uint32_t msg = (mensaje_recibido == MENSAJE_HANDSHAKE)
                    ? RESPUESTA_HANDSHAKE_OK
                    : RESPUESTA_HANDSHAKE_ERROR;
    bytes = send(socket_conexion, &msg, sizeof(uint32_t), 0);
    if (bytes <= 0) {
        log_error(debug_logger, "No se pudo enviar la respuesta al handshake");
        exit(1);
    }
    return mensaje_recibido == MENSAJE_HANDSHAKE;
}

*/
void enviar_memory_dump(char* archivo, int tamanio, char* contenido, int socket)
{   
    t_paquete* paquete = crear_paquete(OPCODE_DUMP_MEMORY);
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    agregar_a_paquete(paquete, &(tamanio), sizeof(int));
    agregar_a_paquete(paquete, contenido, strlen(contenido)+1);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

/***************************************/
/************** TP0 Utils **************/
/***************************************/

void *serializar_paquete(t_paquete *paquete, int bytes)
{
    void *magic = malloc(bytes);
    int desplazamiento = 0;

    memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
    desplazamiento += paquete->buffer->size;

    return magic;
}

void crear_buffer(t_paquete *paquete)
{
    paquete->buffer = malloc(sizeof(t_buffer));
    paquete->buffer->size = 0;
    paquete->buffer->stream = NULL;
}

t_paquete *crear_paquete(op_code codigo_op)
{
    t_paquete *paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = codigo_op;
    crear_buffer(paquete);
    return paquete;
}

void agregar_a_paquete(t_paquete *paquete, void *valor, int tamanio)
{
    paquete->buffer->stream =
        realloc(paquete->buffer->stream,
                paquete->buffer->size + tamanio + sizeof(int));

    memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
    memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

    paquete->buffer->size += tamanio + sizeof(int);
}

void eliminar_paquete(t_paquete *paquete)
{
    free(paquete->buffer->stream);
    free(paquete->buffer);
    free(paquete);
}

