#ifndef MAIN_H_
#define MAIN_H_

#include <utils/conexion.h>
#include <utils/mensajes.h>
#include <utils/serializacion.h>
#include <utils/utils.h>
#include <sys/stat.h>

typedef struct {
    char* puerto_escucha;
    char* mount_dir;
    int block_size;
    int block_count;
    int retardo_acceso_bloque;
    char* log_level;
} t_config_fs;

t_bitarray* bitarray;
char* bloques_mapped;

// Variables globales
t_log *debug_logger;
t_log *filesystem_logger;
t_config_fs* config_fs;
int* ptr_aux;

// Variables de config
char *puerto_escucha;

// Definiciones de funciones
void iniciar_logger();
void cargar_config();
void *atender_conexion(void* param);
bool dump_memory(int socket_conexion);
char* make_metadata_dir(char* archivo);
int buscar_bloque_indice();
int buscar_espacio();
bool make_mount_dir(char* dir_path);
void inicializar_bloques();
void inicializar_bitmap();

#endif // MAIN_H_