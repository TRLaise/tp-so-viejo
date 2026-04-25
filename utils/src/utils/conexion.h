#ifndef CONEXION_H_
#define CONEXION_H_

#define IP_ESCUCHA "127.0.0.1"

#include <string.h>
#include <stdbool.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <utils/utils.h>

/* client-side */
int crear_conexion(char *ip, char* puerto);

/* server-side */
int iniciar_servidor(char* ip, char* puerto);
int esperar_cliente(int socket_servidor);
void liberar_conexion(int socket_cliente);

#endif