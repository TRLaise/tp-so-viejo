#ifndef ESTRUCTURAS_H_
#define ESTRUCTURAS_H_

#include <commons/collections/list.h>
#include <utils/utils.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    uint32_t pid;
    uint32_t tamanio_particion;
    bool esta_ocupada;
    uint32_t base;  // ANTES ERA uint8
    uint32_t limite;// ANTES ERA uint8
} info_particion;

typedef enum {
    PARTICION_FIJA,
    PARTICION_DINAMICA
} t_particion;

typedef struct {
    t_particion esquema;
    void* espacio_contiguo;
    t_list *particiones;
} mem_usuario;

typedef enum {
    FIRST,
    WORST,
    BEST
} t_algoritmo;

#endif // ESTRUCTURAS_H_