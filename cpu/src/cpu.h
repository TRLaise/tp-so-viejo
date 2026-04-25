#ifndef CPU_H_
#define CPU_H_

#include <utils/conexion.h>
#include <utils/mensajes.h>
#include <utils/serializacion.h>
#include <utils/utils.h>
#include <utils/tcb.h>
#include <cpu_utils.h>

typedef struct {
    char* ip_memoria;
    char* puerto_memoria;
    char* puerto_escucha_dispatch;
    char* puerto_escucha_interrupt;
    t_log_level log_level;
} t_config_cpu;

// Variables globales
extern t_log* debug_logger;
extern t_log* cpu_logger;

extern t_config* config;
extern t_config_cpu* config_cpu;

extern int socket_escucha_dispatch;
extern int socket_escucha_interrupt;

extern bool interrupt_flag;
extern bool exec_flag;
extern bool evacuate_flag;

extern pthread_mutex_t mx_interrupt;

extern t_list* tlb;

extern int dispatch_fd;
extern int interrupt_fd;
extern int socket_cpu_dispatch;
extern int socket_cpu_interrupt;
extern int conexion_memoria;

extern op_code motivo;

extern sem_t sem_exec;

// Funciones inicio
void iniciar_config();
void iniciar_logger();
void *server_dispatch();
void *server_interrupt();

// Funciones
void inicio_ciclo_de_instrucciones(t_contexto_ejecucion* context);
void fetch(t_contexto_ejecucion* context);
void decode(t_contexto_ejecucion* context, t_instruccion* instruccion);
void liberar_instruccion(t_instruccion* instruccion);
void terminar_programa();

#endif // CPU_H_