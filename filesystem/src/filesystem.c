#include "./filesystem.h"

int main(int argc, char* argv[]) {
    
    iniciar_logger();
    cargar_config();

    if (!make_mount_dir(config_fs->mount_dir)){
        log_error(debug_logger, "Error al crear directorio %s", config_fs->mount_dir);
        exit(EXIT_FAILURE);
    }

    inicializar_bloques();
    inicializar_bitmap();

    int socket_escucha = iniciar_servidor(IP_ESCUCHA, config_fs->puerto_escucha);

    while(true){
        // Guardo el socket en el heap para no perderlo
        int *conexion = malloc(sizeof(int));
        *conexion = esperar_cliente(socket_escucha);

        // Crear hilo para manejar esta conexion
        pthread_t hilo;
        int iret = pthread_create(&hilo, NULL, atender_conexion, conexion);
        if (iret != 0) {
            log_error(debug_logger, "No se pudo crear un hilo para atender la conexion");
            exit(1);
        }

        pthread_detach(hilo);
    }

    return 0;
}

bool make_mount_dir(char* dir_path)
{
    struct stat dir = {0};
    if (stat(dir_path, &dir) == -1)
        return mkdir(dir_path, 0700) == 0;
}

void *atender_conexion(void* param)
{
    int socket_conexion = *(int*) param;
    log_info(debug_logger, "%s¡Se conecto un cliente!", VERDE);

    int random = 10;
    op_code cod_op;
    while ((cod_op = recibir_operacion(socket_conexion)) != -1) {
        switch(cod_op) {
            case OPCODE_DUMP_MEMORY:
                if (dump_memory(socket_conexion)) {
                    t_paquete *paquete_fs = crear_paquete(OK);
                    agregar_a_paquete(paquete_fs, &random, sizeof(int));
                    enviar_paquete(paquete_fs, socket_conexion);
                    eliminar_paquete(paquete_fs);
                } else {
                    t_paquete *paquete_fs = crear_paquete(FALLO); 
                    agregar_a_paquete(paquete_fs, &random, sizeof(int));
                    enviar_paquete(paquete_fs, socket_conexion);
                    eliminar_paquete(paquete_fs); 
                    exit(EXIT_FAILURE);
                }
                break;
            default: 
                log_error(filesystem_logger, "Memoria envio un op_code invalido.");
                abort();
                break;
        }
    }

    //close(socket_conexion);
    //free(socket_conexion);
    //pthread_exit(NULL);
}


void iniciar_logger()
{
    // Logger
    debug_logger = log_create("./cfg/filesystem_debug.log", "filesystem_debug", true, LOG_LEVEL_INFO);
    filesystem_logger = log_create("./cfg/filesystem.log", "filesystem", true, LOG_LEVEL_INFO);
}

void cargar_config()
{
    config_fs = malloc(sizeof(t_config_fs));
    t_config* config = config_create("./cfg/filesystem.config");

    if (config == NULL) {
        log_error(debug_logger, "No se pudo crear la config");
        exit(1);
    }

    config_fs->puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    config_fs->mount_dir = config_get_string_value(config, "MOUNT_DIR");
    config_fs->block_size = config_get_int_value(config, "BLOCK_SIZE");
    config_fs->block_count = config_get_int_value(config, "BLOCK_COUNT");
    config_fs->retardo_acceso_bloque = config_get_int_value(config, "RETARDO_ACCESO_BLOQUE");
    config_fs->log_level = config_get_string_value(config, "LOG_LEVEL");
}

void inicializar_bloques()
{
    size_t pathlen = strlen(config_fs->mount_dir);
    char* path = malloc(pathlen + 1);

    strcpy(path, config_fs->mount_dir);
    strcat(path, "/bloques.dat"); // agregar al path /bloques.dat

    FILE* archivo = fopen(path, "a+"); // abrir para que escriba al final

    if (archivo == NULL) {
        log_error(debug_logger, "Error al abrir el archivo de bloques.");
        exit(EXIT_FAILURE);
    }

    int fd = fileno(archivo); // Para las operaciones ftruncate y mmap.
    int error = ftruncate(fd, config_fs->block_size * config_fs->block_count);
    if (error == -1) {
        log_error(debug_logger, "Error al truncar el archivo de bloques");
        exit(EXIT_FAILURE);
    }

    bloques_mapped = mmap(
        NULL,
        config_fs->block_size * config_fs->block_count,
        PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    ); // Mapea el archivo bloques.dat en la memoria del proceso para facilitar su acceso 

    fclose(archivo);
}

void inicializar_bitmap()
{
    int pathlen = strlen(config_fs->mount_dir)+ strlen("/bitmap.dat") + 1;
    char* path = malloc(pathlen);
    
    strcpy(path, config_fs->mount_dir);
    strcat(path, "/bitmap.dat"); // agregar al path /bitmap.dat
    
    FILE* archivo = fopen(path, "a+"); // abrir para que escriba al final
    if (archivo == NULL) {
        log_error(debug_logger, "Error al abrir el archivo de bitmap.");
        exit(EXIT_FAILURE);
    }

    int fd = fileno(archivo);
    int error = ftruncate(fd, ceil(config_fs->block_count / 8));
    if (error == -1) {
        log_error(debug_logger, "Error al truncar el archivo de bitmap.");
        exit(EXIT_FAILURE);
    }

    char* mapped = malloc(ceil(config_fs->block_count / 8));

    bitarray = bitarray_create_with_mode(mapped, ceil(config_fs->block_count / 8), LSB_FIRST);

    bitarray->bitarray = mmap(NULL, ceil(config_fs->block_count/8), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (msync(bitarray->bitarray, (ceil(config_fs->block_count/8)), MS_SYNC) == -1) {
        log_error(debug_logger, "Error al sincronizar el archivo de bitmap.");
        exit(EXIT_FAILURE);
    }

    if (!bitarray->bitarray)
    {
        log_error(debug_logger, "Error al asignar memoria para el bitarray.");
        munmap(bitarray->bitarray, ceil(config_fs->block_count / 8));
        close(fd);
        exit(EXIT_FAILURE);
    }

    fclose(archivo);
}


int buscar_espacio() {
    int bloques_disponibles = 0;
    for (int i = 0; config_fs->block_count; ++i) {
        if (bitarray_test_bit(bitarray, i) == 0) {
            ++bloques_disponibles;
        }
    }
    return bloques_disponibles;
}

int buscar_bloque_indice()
{
    for (int i = 0; i < config_fs->block_count; ++i) {
        if (!bitarray_test_bit(bitarray, i)) {
            bitarray_set_bit(bitarray, i);
            if (msync(bitarray->bitarray, ceil(config_fs->block_count/8), MS_SYNC) == -1)
                log_error(debug_logger, "No se pudo sincronizar el archivo bitmap - reservar_bloque()");
            return i;
        }
    }

    return -1;
}

char* make_metadata_dir(char* archivo)
{
    char* dir = string_new();
    string_append(&dir, config_fs->mount_dir);
    string_append(&dir, "/files");

    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0700) != 0) {
            log_error(debug_logger, "Error al crear directorio %s/%s", config_fs->mount_dir, archivo);
            return NULL;
        }
    }

    char* path = string_new();
    string_append(&path, dir);
    string_append(&path, "/");
    string_append(&path, archivo);

    free(dir);

    return path;
}

bool dump_memory(int socket_conexion)
{
    t_list* paquete = recibir_paquete(socket_conexion);
    char* nombre = (char*) list_get(paquete, 0);
    int tamanio = *(int*) list_get(paquete, 1);
    char* contenido = (char*) list_get(paquete, 2);
    
    //1. Verificar si hay espacio disponible 
    int cantidad_bloques_necesarios = ceil((tamanio + config_fs->block_size - 1)/config_fs->block_size);
    int cantidad_bloques_disponibles = buscar_espacio();

    if (cantidad_bloques_necesarios + 1 > cantidad_bloques_disponibles) {
        log_error(debug_logger, "No hay bloques disponibles (necesarios %d - disponibles %d) - dump_memory()", cantidad_bloques_necesarios, cantidad_bloques_disponibles);
        return false;
    }
    
    // 2. Buscar y asignar indice 
    int indice = buscar_bloque_indice();
    if (indice == -1) {
        log_error(debug_logger, "No se pudo reservar el bloque de indice.");
    }
    
    /* Log Obligatorio */
    log_info(filesystem_logger,"## Bloque asignado: %d - Archivo: %s - Bloques Libres: %d", indice, nombre, buscar_espacio());

    // 3. Buscar y asignar bloques de datos
    int bloques[cantidad_bloques_necesarios];
    for (int i = 0; i < cantidad_bloques_necesarios; ++i) {
        bloques[i] = buscar_bloque_indice();
        if(bloques[i] == -1) {
            log_error(debug_logger, "No se pudo reservar el bloque de datos.");
            return false;
        }
        /* Log Obligatorio */
        log_info(filesystem_logger,"## Bloque asignado: %d - Archivo: %s - Bloques Libres: %d", indice, nombre, buscar_espacio());
    }
    
    // 4. Crear archivo <PID>-<TID>-<TIMESTAMP>.dmp 
    FILE* f_metadata = fopen(make_metadata_dir(nombre), "w");
    if (!f_metadata) {
        log_error(debug_logger, "No se pudo crear el archivo de metadata en el directorio: %s", make_metadata_dir(nombre));
        return false;
    }
    /* Log Obligatorio */
    log_info(filesystem_logger, " ## Archivo Creado: %s - Tamaño: %d", nombre, tamanio);
    fclose(f_metadata);

    // 5. Aceder al bloque de punteros y grabar los bloques reservados.
    int* punteros = (int*) (ptr_aux + indice * config_fs->block_size);
    for (int i = 0; i < cantidad_bloques_necesarios; ++i) {
        punteros[i] = bloques[i];
    }
    msync(ptr_aux + indice * config_fs->block_size, config_fs->block_size, MS_SYNC);

    /* Log Obligatorio */
    log_info(filesystem_logger, "## Acceso Bloque - Archivo: %s - Tipo Bloque: ÍNDICE - Bloque File System %d", nombre, indice);

    // 6. Acceder bloque a bloque e ir escribiendo el contenido de la memoria.
    int bytes_written = 0;
    for (int i = 0; i < cantidad_bloques_necesarios; ++i) {
        int write_bytes = (tamanio - bytes_written > config_fs->block_size) ? config_fs->block_size : (tamanio - bytes_written);
        memset(ptr_aux + bloques[i] * config_fs->block_size, 0, config_fs->block_size);

        if (bytes_written < tamanio) {
            int valid_bytes = (tamanio - bytes_written > write_bytes) ? write_bytes : (tamanio - bytes_written);
            memcpy(ptr_aux + bloques[i] * config_fs->block_size, contenido + write_bytes, valid_bytes);
        }
        msync(ptr_aux + bloques[i] * config_fs->block_size, config_fs->block_size, MS_SYNC);

        /* Log Obligatorio */
        log_info(debug_logger, "## Acceso Bloque - Archivo: %s - Tipo Bloque: DATOS - Bloque File System %d", nombre, bloques[i]);

        bytes_written += write_bytes;

        usleep(config_fs->retardo_acceso_bloque * 1000);
    }

    /* Log Obligatorio */
    log_info(filesystem_logger, " ## Fin de solicitud - Archivo: %s ", nombre);
    return true;
}