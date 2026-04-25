#ifndef ATENDER_KERNEL_H_
#define ATENDER_KERNEL_H_

info_particion *desocupar_particion(uint32_t pid);
void checkeo_consolidar(info_particion *particion_consolidacion);
info_particion *consolidar(info_particion *particion_base_mas_chica, info_particion* particion_base_mas_grande);
void memory_dump(int socket_conexion);
void finalizar_hilo(int socket_conexion);
void crear_hilo(int socket_conexion);
void finalizar_proceso(int socket_conexion);
void crear_proceso(int socket_conexion);

#endif