#ifndef MEMORIA_H_
#define MEMORIA_H_

#include "estructuras.h"
#include <utils/conexion.h>
#include <utils/mensajes.h>
#include <utils/serializacion.h>
#include <utils/utils.h>
#include <memoria_utils.h>
#include <atender_kernel.h>
#include <atender_cpu.h>

//Loggers
extern t_log *debug_logger;
extern t_log *memoria_logger;

//Variables de Config
extern char *PUERTO_ESCUCHA;
extern char *IP_FILESYSTEM;
extern char *PUERTO_FILESYSTEM;
extern int TAM_MEMORIA;
extern char *PATH_INSTRUCCIONES;
extern int RETARDO_RESPUESTA;
extern t_particion ESQUEMA;
extern t_algoritmo ALGORITMO_BUSQUEDA;
extern char **PARTICIONES;
extern char *LOG_LEVEL;

//Variables Globales
extern t_dictionary *contextos;
extern mem_usuario *memoria_usuario;
extern t_bitarray *bitmap;
extern t_dictionary *pid_tamanio;

extern int conexion_filesystem;

//Funciones
void inicializar_globales(void);
void cargar_config(t_config *config); 
void *atender_conexion(void *param);
void atender_cpu(int socket_conexion);
void atender_kernel(int socket_conexion);
t_contexto_ejecucion* get_contexto(char* pid, char* tid);


#endif // MEMORIA_H_