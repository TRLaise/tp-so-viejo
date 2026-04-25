#ifndef CPU_UTILS_H_
#define CPU_UTILS_H_

// Libs
#include <cpu.h>

// Estructuras
typedef struct {
    int numero_pagina;
    int desplazamiento;
    int direccion_fisica;
} t_mmu;

// FUNCIONES

t_tcb* recibir_tcb(int fd);
uint32_t obtener_valor_registro(t_contexto_ejecucion* context, char* valor);
t_contexto_ejecucion* solicitar_contexto_ejecucion(int fd, uint32_t pid, uint32_t tid);
char* solicitar_instruccion(int fd, uint32_t pid, uint32_t tid, uint32_t pc);
t_instruccion* parsear_linea(char* linea);
op_code parsear_instruccion(char* nombre);
void set(t_contexto_ejecucion* context, char* registro, uint32_t valor);
void sum(t_contexto_ejecucion* context, char* reg1, char* reg2);
void sub(t_contexto_ejecucion* context, char* reg1, char* reg2);
void actualizar_contexto_ejecucion(t_contexto_ejecucion* context);
op_code recibir_motivo_exit();
void enviar_interrupcion(op_code instruccion);
void enviar_tid(uint32_t tid);
void enviar_syscall(op_code syscall, char* param1, char* param2, char* param3);
char* imprimir_parametros(t_list* params);
t_contexto_ejecucion* recibir_contexto_ejecucion(t_list* data);
int mmu(t_contexto_ejecucion* context, int dir_logica);
void enviar_seg_fault(t_contexto_ejecucion* context);
void escribir_en_memoria(t_contexto_ejecucion* context, int dir_fisica, uint32_t valor);
void write_mem(t_contexto_ejecucion* context, char* reg1, char* reg2);
void read_mem(t_contexto_ejecucion* context, char* reg1, char* reg2);
uint32_t leer_de_memoria(t_contexto_ejecucion* context, int dir_fisica);
bool recibir_desalojo();

#endif // CPU_UTILS_H_