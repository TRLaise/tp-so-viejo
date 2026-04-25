#ifndef KERNEL_H_
#define KERNEL_H_

#define KERNEL "KERNEL"

// Libs
#include <estructuras_kernel.h>
#include "utils_kernel.h"

#include <utils/conexion.h>
#include <utils/mensajes.h>
#include <utils/serializacion.h>
#include <utils/utils.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/queue.h>
#include <commons/collections/dictionary.h>
#include <utils/pcb.h>
#include <utils/tcb.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// Estructuras
typedef struct {
    char* ip_memoria;
    char* puerto_memoria;
    char* ip_cpu;
    char* puerto_cpu_dispatch;
    char* puerto_cpu_interrupt;
    t_algoritmo_planificacion algoritmo_planificacion;
    int quantum;
    t_log_level log_level;
} t_config_kernel;

// Variables globales
extern t_log* logger_kernel;
extern t_log* debug_logger;
extern t_config* config;
extern t_config_kernel* config_kernel;

extern int dump_fd;
extern int cpu_dispatch_fd;
extern int cpu_interrupt_fd;

extern int tid_en_ejecucion;
extern t_tcb *tcb_en_ejecucion;
extern int pid_en_ejecucion;

extern bool flag_espacio_proceso;

extern t_queue *cola_new;  // Contiene t_proceso_nuevo
extern t_queue *cola_ready;
extern t_queue *cola_blocked;
extern t_queue *cola_exit;
extern t_queue *cola_io;    // cola de hilos_io (declarado en estructuras.h)
extern t_queue *cola_blocked_dump; // creamos una lista para los hilos bloqueados por solicitar syscall DUMP_MEMORY

extern sem_t sem_elementos_en_new;
extern sem_t sem_entrada_a_ready;
extern sem_t sem_elementos_en_ready;
extern sem_t sem_elementos_en_io;
extern sem_t sem_elementos_en_dump;
extern t_dictionary *diccionario_pcb_pid;
extern t_dictionary *diccionario_mutex;


// Funciones
void iniciar_logger();
void iniciar_config();
void iniciar_conexiones();
void inicializar_colas();
void inicializar_semaforos();

void procesar_conexion_dispatch();
void procesar_conexion_interrupt();

void *planificador_largo_plazo(void *param);
void *planificador_corto_plazo(void *params);
void *entrada_salida(void *params);
void *dump_mem(void *params);

// Syscalls
void process_create(char *path, uint32_t tamanio_en_memoria, uint32_t prioridad_hilo_main);
void process_exit();
void thread_create(char *path, uint32_t prioridad);
void thread_join(uint32_t tid);
void thread_cancel(uint32_t tid);
void thread_exit();
void io(uint32_t milisegundos);
void mutex_create(char *nombre_nuevo_mutex);
bool mutex_lock(char *nombre_mutex);
void mutex_unlock(char *nombre_mutex);
void dump_memory();

// Kernel Utils
void eliminar_hilo(t_tcb *tcb);
void devolver_a_ready_joins(t_tcb* tcb);
int enviar_proceso_nuevo_a_memoria(t_proceso_nuevo *proceso_nuevo);

#endif // KERNEL_H_