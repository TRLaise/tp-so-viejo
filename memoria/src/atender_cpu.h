#ifndef ATENDER_CPU_H_
#define ATENDER_CPU_H_

t_contexto_ejecucion* obtener_contexto_ejecucion(int socket_conexion);
void actualizar_contexto_ejecucion(int socket_conexion);
char *obtener_instruccion(int socket_conexion);
void read_mem(int socket_conexion);
void* leer_de_memoria(uint32_t dir_fisica);
void write_mem(int socket_conexion);
void enviar_contexto_ejecucion(t_contexto_ejecucion* contexto, int socket_conexion);
void enviar_uint32(uint32_t valor, int socket_conexion);
void enviar_instruccion(char* instruccion, int socket);

#endif