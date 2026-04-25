#ifndef SERIALIZACION_H_
#define SERIALIZACION_H_

#include "./utils.h"

typedef struct {
    int size;
    void *stream;
} t_buffer;

typedef struct {
    op_code codigo_operacion;
    t_buffer *buffer;
} t_paquete;

/* Handshakes */
bool realizar_handshake(int socket_conexion);
bool recibir_handshake(int socket_conexion);

/* Envia un string */
void enviar_mensaje(char *mensaje, int socket_conexion);

void *serializar_paquete(t_paquete *paquete, int bytes);

/* Devuelve un paquete vacio */
t_paquete *crear_paquete(op_code codigo_op);

/* Elimina el paquete */
void eliminar_paquete(t_paquete *paquete);

/* Agrega contenido a un paquete */
void agregar_a_paquete(t_paquete *paquete, void *valor, int tamanio);

/* Envia paquete al socket indicado */
void enviar_paquete(t_paquete *paquete, int socket_conexion);

/* Devuelve el codigo de operacion al inicio de un paquete
 * Debe ser llamado antes de recibir_paquete o recibir_mensaje */
int recibir_operacion(int socket_conexion);

/* Lee size bytes del socket */
void *recibir_buffer(int *size, int socket_conexion);

/* Recibe el buffer de un paquete y devuelve una lista con sus elementos */
t_list *recibir_paquete(int socket_conexion);

/* Recibe un mensaje simple y lo devuelve */
char *recibir_mensaje(int socket_conexion);

/* Recibe un mensaje simple y lo retorna, pone true en error ante un error */
void enviar_int(uint32_t mensaje, int socket_conexion);
uint32_t recibir_int(int socket_conexion, bool *error);

/* Envia paquete a FS */
void enviar_memory_dump(char* archivo, int tamanio, char* contenido, int socket);

#endif // SERIALIZACION_H_