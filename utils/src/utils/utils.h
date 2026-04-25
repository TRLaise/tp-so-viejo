#ifndef UTILS_H_
#define UTILS_H_

/* Nombre de modulos */
//#define KERNEL "KERNEL"
//#define CPU "CPU"
//#define MEMORIA "MEMORIA"
//#define ENTRADASALIDA "ENTRADASALIDA"

#define VERDE "\x1b[32m"

/* IPs para la entrega final */
//#define IP_KERNEL "127.0.0.1"
//#define IP_MEMORIA "127.0.0.1"
//#define IP_CPU "127.0.0.1"
//#define IP_ENTRADASALIDA "127.0.0.1"

/* Librerias compartidas entre modulos */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <readline/readline.h>
#include <commons/bitarray.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <utils/conexion.h>
#include <math.h>
#include <commons/temporal.h>
#include <sys/mman.h>
#include "./mensajes.h"
#include <assert.h>

/* Variables globales para los modulos */
extern t_log *debug_logger;
extern sem_t sem_espera_ok;

typedef struct {
    uint32_t ax;
    uint32_t bx;
    uint32_t cx;
    uint32_t dx;
    uint32_t ex;
    uint32_t fx;
    uint32_t gx;
    uint32_t hx;
    uint32_t pc;
} t_registros;

typedef struct {
    uint32_t base;
    uint32_t limite;
    t_list* codigo;
    uint32_t tid;
    uint32_t pid;
    t_registros* registros;
} t_contexto_ejecucion;

typedef struct {
    char* nombre;
    t_list* params;
} t_instruccion;

/* Funciones para reutilizar en varios modulos */

// Devuelve string config
char *get_config_string(t_config *config, char*clave);
int get_config_int(t_config *config, char*clave);
int ceil_div(int dividend, int divisor);

void* max_int(void* a, void* b);

#endif // UTILS_H_