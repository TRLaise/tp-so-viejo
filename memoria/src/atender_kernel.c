#include "memoria.h"

void crear_proceso(int socket_conexion)
{
    t_list *info_paquete = recibir_paquete(socket_conexion);
    uint32_t pid = *(uint32_t*) list_get(info_paquete, 0);
    char *path = (char*) list_get(info_paquete, 1);
    uint32_t tam_proceso = *(uint32_t*) list_get(info_paquete, 2);

    // Guardamos el tamaño del pid
    char *pid_str = string_itoa(pid);
    dictionary_put(pid_tamanio, pid_str, list_get(info_paquete, 2));
    //free(pid_str);

    // Filtramos la lista de particiones por las que no estan ocupadas y en las que entra el proceso a crear
    t_list *lista_particiones_aux = filtrar_lista_si_entra(tam_proceso, list_filter(memoria_usuario->particiones, _no_esta_ocupada));

    // Si la lista esta vacia => No hay particiones disponibles para este proceso
    // Devolvemos error a Kernel
    if(list_is_empty(lista_particiones_aux)){
        log_warning(debug_logger, "no hay espacio en memoria");
        
        // Aviso a kernel que no hay espacio
        t_paquete *paquete = crear_paquete(MENSAJE_NO_HAY_ESPACIO);
        enviar_paquete(paquete, socket_conexion);
        eliminar_paquete(paquete);
        
        list_destroy(lista_particiones_aux);
        list_destroy_and_destroy_elements(info_paquete, free);
        return;
    }
    info_particion* particion_elegida = malloc(sizeof(info_particion));

    // Busco la partición a asignar según el algoritmo de busqueda
    switch (ALGORITMO_BUSQUEDA)
        {
        case FIRST:
            // Tomamos la particion con la base mas chica (la primera)
            particion_elegida = list_get_minimum(lista_particiones_aux, _min_base);
            break;
            
        case BEST:
            // Tomamos la particion con el menor limite, ya que, segun el filtro realizado anteriormente,
            // el proceso deberia entrar en todas las particiones de esta lista
            particion_elegida = list_get_minimum(lista_particiones_aux, _min_limite);
            break;

        case WORST:
            // Tomamos la particion con el tamanio mas grande
            particion_elegida = list_get_maximum(lista_particiones_aux, _max_tamanio_particion);
            break;
        
        default:
            log_error(debug_logger, "Estrategia no reconocida.");
            break;
        }
    
    t_contexto_ejecucion *nuevo_contexto = malloc(sizeof(t_contexto_ejecucion));
    
    nuevo_contexto->codigo = parsear_instrucciones(path);
    nuevo_contexto->pid = pid;
    nuevo_contexto->tid = 0;
    nuevo_contexto->registros = registros_en_cero(nuevo_contexto);

    // Revisamos si es fijas o dinamicas
    switch (ESQUEMA)
    {
    case PARTICION_FIJA:
        // Actualizamos la particion
        particion_elegida->pid = pid;
        particion_elegida->esta_ocupada = true;
        
        // Creamos el contexto del proceso/hilo_main
        nuevo_contexto->base = particion_elegida->base;
        nuevo_contexto->limite = particion_elegida->limite;

        break;
    
    case PARTICION_DINAMICA:
        
        // Creamos la partición dinámica
        info_particion *particion_a_ocupar = malloc(sizeof(info_particion));
        particion_a_ocupar->pid = pid;
        particion_a_ocupar->base = particion_elegida->base;
        particion_a_ocupar->limite = particion_a_ocupar->base + tam_proceso - 1;
        particion_a_ocupar->esta_ocupada = true;
        particion_a_ocupar->tamanio_particion = tam_proceso;
        list_add(memoria_usuario->particiones, particion_a_ocupar);
        
        // Creamos la partición para lo que sobró entre la original y la dinámica
        if (particion_elegida->tamanio_particion > tam_proceso){
            info_particion *particion_libre = malloc(sizeof(info_particion));
            particion_libre->pid = -1;
            particion_libre->base = particion_a_ocupar->base + particion_a_ocupar->limite + 1;
            particion_libre->limite = particion_elegida->limite - particion_a_ocupar->limite;
            particion_libre->esta_ocupada = false;
            particion_libre->tamanio_particion = particion_elegida->tamanio_particion - tam_proceso;
            list_add(memoria_usuario->particiones, particion_libre);
        }

        // Agregamos al contexto la base y limite
        nuevo_contexto->base = particion_a_ocupar->base;
        nuevo_contexto->limite = particion_a_ocupar->limite;

        // Borramos la partición original de la lista de particiones
        list_remove_element(memoria_usuario->particiones, particion_elegida);
        free(particion_elegida);
    }

    // Agregamos el proceso/hilo_main al diccionario de pids-tids
    char *pid_tid = concatenar((uint32_t)pid, 0);
    dictionary_put(contextos, pid_tid, nuevo_contexto);
    
    //free(pid_tid);
    list_destroy(lista_particiones_aux);
    //list_destroy_and_destroy_elements(info_paquete, free);

    // Aviso a kernel que se termino la transaccion
    t_paquete *paquete = crear_paquete(MENSAJE_OP_TERMINADA);
    enviar_paquete(paquete, socket_conexion);
    eliminar_paquete(paquete);
    
    log_info(memoria_logger, "## Proceso Creado - PID: %d - Tamaño: %d", pid, tam_proceso);
}

void finalizar_proceso(int socket_conexion)
{
    // Recibir pid a finalizar
    t_list *info_paquete = recibir_paquete(socket_conexion);
    uint32_t pid = *(uint32_t*) list_get(info_paquete, 0);


    // Eliminar contextos relacionados con el pid a eliminar
    liberar_contextos(pid);
    
    info_particion *particion_a_finalizar = desocupar_particion(pid);

    // Si estamos en esquemas de particiones dinámicas tenemos que ver si hay que consolidar.
    if (ESQUEMA == PARTICION_DINAMICA) {
        checkeo_consolidar(particion_a_finalizar);
    }
    
    // Aviso a kernel que se termino la transaccion
    int rand = 10;
    t_paquete *paquete = crear_paquete(MENSAJE_OP_TERMINADA);
    agregar_a_paquete(paquete, &rand, (sizeof(int)));
    enviar_paquete(paquete, socket_conexion);
    eliminar_paquete(paquete);
    list_destroy(info_paquete);

    // Marcar la particion como desocupada cuando el pid coincide
    uint32_t tamanio = 0;
    tamanio = *(uint32_t*) dictionary_get(pid_tamanio, string_itoa(pid));
    if(tamanio != NULL){
        log_info(memoria_logger, "## Proceso Destuido - PID: %d - Tamaño: %d", pid, tamanio);
    } else {
        log_error(debug_logger,"No se encontro el pid %d en el diccionario de pid-tamanio.",pid);
    }
}

void crear_hilo(int socket_conexion)
{
    t_list *paquete = recibir_paquete(socket_conexion);
    uint32_t pid = *(uint32_t*) list_get(paquete, 0);
    uint32_t tid = *(uint32_t*) list_get(paquete, 1);
    char *path = (char*) list_get(paquete, 2);
    list_destroy(paquete);

    // Obtener contexto main del pid para conocer la base y limite
    char *pid_tid_main = concatenar(pid, 0);
    t_contexto_ejecucion *contexto_main = dictionary_get(contextos, pid_tid_main);

    // Creamos el nuevo contexto
    t_contexto_ejecucion *nuevo_contexto = malloc(sizeof(t_contexto_ejecucion));
    nuevo_contexto->codigo = parsear_instrucciones(path);
    nuevo_contexto->pid = pid;
    nuevo_contexto->tid = tid;
    nuevo_contexto->base = contexto_main->base;
    nuevo_contexto->limite = contexto_main->limite;
    nuevo_contexto->registros = registros_en_cero();
    
    // Agregamos el nuevo contexto al diccionario de pids-tids
    char *pid_tid = concatenar(pid, tid);
    dictionary_put(contextos, pid_tid, nuevo_contexto);
    //free(pid_tid);

    // Aviso a kernel que se termino la transaccion
    t_paquete *paquete_rta = crear_paquete(MENSAJE_OP_TERMINADA);
    enviar_paquete(paquete_rta, socket_conexion);
    eliminar_paquete(paquete_rta);

    log_info(memoria_logger, "## Hilo Creado - (PID-TID) - (%d-%d)", pid, tid);
}

void finalizar_hilo(int socket_conexion)
{
    t_list *paquete = recibir_paquete(socket_conexion);
    uint32_t pid = *(uint32_t*) list_get(paquete, 0);
    uint32_t tid = *(uint32_t*) list_get(paquete, 1);
    list_destroy(paquete);
    
    char *pid_tid = concatenar(pid, tid);

    // Elimina el contexto del diccionario
    t_contexto_ejecucion* contexto_a_eliminar = dictionary_remove(contextos, pid_tid);
    contexto_destroyer(contexto_a_eliminar);
    free(pid_tid);

    // Aviso a kernel que se termino la transaccion
    t_paquete *paquete_rta = crear_paquete(MENSAJE_OP_TERMINADA);
    enviar_paquete(paquete_rta, socket_conexion);
    eliminar_paquete(paquete_rta);

    log_info(memoria_logger, "## Hilo Destuido - (PID-TID) - (%d-%d)", pid, tid);
}

char *obtener_nombre_archivo(uint32_t pid, uint32_t tid)
{
    char* nombre_archivo = string_new();
    char* pid_tid = concatenar(pid, tid);
    char* timestamp = temporal_get_string_time("%H:%M:%S:%MS");

    string_append(&nombre_archivo, pid_tid);
    string_append(&nombre_archivo, "-");
    string_append(&nombre_archivo, timestamp);
    string_append(&nombre_archivo, ".dmp");

    free(pid_tid);
    free(timestamp);

    return nombre_archivo;
}

void memory_dump(int socket_conexion)
{
    t_list* paquete = recibir_paquete(socket_conexion);
    uint32_t pid = *(uint32_t*) list_get(paquete, 0);
    uint32_t tid = *(uint32_t*) list_get(paquete, 1);
    list_destroy(paquete);

    t_contexto_ejecucion* contexto = get_contexto(string_itoa(pid), string_itoa(tid));

    // nombre - tamanio (sizeof(espacio_contiguo)) - contenido (espacio_contiguo)
    char* nombre_archivo = obtener_nombre_archivo(pid, tid);
    uint32_t tamanio = contexto->limite - contexto->base;
    char* contenido = malloc(tamanio);

    //! WARNING: puede romper + contexto->limite - puede que funcione con + contexto->base
    memcpy(contenido, (*(uint32_t*)memoria_usuario->espacio_contiguo) + contexto->limite, tamanio);

    enviar_memory_dump(nombre_archivo, tamanio, contenido, conexion_filesystem);

    // Recibimos respuesta de FS, es un op_code, solo puede ser OK o FALLO
    op_code ret = recibir_operacion(conexion_filesystem);
    
    // 1 caso positivo, 0 negativo
    if (ret == OK){
        // Envio respuesta exito a kernel
        // Aviso a kernel que se termino la transaccion
        t_paquete *paquete_rta_exito = crear_paquete(MENSAJE_EXITO_DUMP);
        enviar_paquete(paquete_rta_exito, socket_conexion);
        eliminar_paquete(paquete_rta_exito);
            
        // Enviamos paquete con resultado dump, pid y tid
        //t_paquete *paquete_memoria = crear_paquete(OPCODE_DUMP_MEMORY);
        // agregar_a_paquete(paquete_memoria, &rta_exito, sizeof(int));
        // agregar_a_paquete(paquete_memoria, &pid, sizeof(uint32_t));
        // agregar_a_paquete(paquete_memoria, &tid, sizeof(uint32_t));
        // enviar_paquete(paquete_memoria, socket_conexion);
        // eliminar_paquete(paquete_memoria);
    } else if (ret == FALLO){
        // Envio respuesta fallo a kernel
        t_paquete *paquete_rta_fallo = crear_paquete(MENSAJE_FALLO_DUMP);
        enviar_paquete(paquete_rta_fallo, socket_conexion);
        eliminar_paquete(paquete_rta_fallo);
        
        // Enviamos paquete con resultado dump, pid y tid
        // t_paquete *paquete_memoria = crear_paquete(OPCODE_DUMP_MEMORY);
        // agregar_a_paquete(paquete_memoria, &rta_fallo, sizeof(int));
        // agregar_a_paquete(paquete_memoria, &pid, sizeof(uint32_t));
        // agregar_a_paquete(paquete_memoria, &tid, sizeof(uint32_t));
        // enviar_paquete(paquete_memoria, socket_conexion);
        // eliminar_paquete(paquete_memoria);
    } else {
        log_error(debug_logger, "Error en el dump memory (enviar respuesta de fs a memoria)");
    }
    
    log_info(memoria_logger, "## Memory Dump solicitado - (PID-TID) - (%d-%d)", pid, tid);
}

// Liberar particion con "X" pid
info_particion *desocupar_particion(uint32_t pid)
{
    info_particion *particion_desocupada = malloc(sizeof(info_particion));

    // Iteramos las particiones para encontrar una que tenga nuestro pid y que este_ocupada == true
    t_list_iterator *it = list_iterator_create(memoria_usuario->particiones);
    while (list_iterator_has_next(it)) {
        info_particion *particion = list_iterator_next(it);
        // Si encontramos actualiza esta_ocupada
        if (particion->pid == pid && particion->esta_ocupada) {
            particion_desocupada = particion;
            particion->esta_ocupada = false;
            log_warning(debug_logger, "Se libero una particion con el pid %d", pid);
        }
        else {
            log_info(debug_logger, "No se encontro una particion con el pid %d para liberar", pid);
        }
    }
    list_iterator_destroy(it);
    return particion_desocupada;
}

void checkeo_consolidar(info_particion *particion_consolidacion)
{
    t_list_iterator *it = list_iterator_create(memoria_usuario->particiones);
    while (list_iterator_has_next(it)) {
        info_particion *particion = list_iterator_next(it);

        // Si la particion encontrada es la misma que queremos ver si podemos consolidar 
        if (particion_consolidacion == particion) {
            continue;
        }
        // Analizo el caso en que la base encontrada está a izquierda que la que checkeamos consolidar
        else if (particion_consolidacion->base == particion->base + particion->limite)
        {
            // Pasamos la particion a izquierda como primer parametro (porque es más chica)
            info_particion *particion_nueva = consolidar(particion, particion_consolidacion);
            particion_consolidacion = particion_nueva;
        }
        // Analizo el caso a derecha
        else if(particion_consolidacion->base + particion_consolidacion->limite == particion->base)
        {
            // Pasamos la particion a derecha como segundo parametro (porque es más grande)
            info_particion *particion_nueva = consolidar(particion_consolidacion, particion);
            particion_consolidacion = particion_nueva;
        }
    }
    list_iterator_destroy(it);
}

// A izquierda la base mas chica, a derecha la base mas grande
info_particion *consolidar(info_particion *particion_base_mas_chica, info_particion* particion_base_mas_grande)
{
    // Creo la particion nueva en la que voy a consolidar
    info_particion* particion_nueva = malloc(sizeof(info_particion));
    
    // Fijamos la base nueva, como la base de la más chica
    particion_nueva->base = particion_base_mas_chica->base;

    // Fijamos el offset
    particion_nueva->limite = particion_base_mas_chica->limite + particion_base_mas_grande->limite; 

    // La marcamos como libre
    particion_nueva->esta_ocupada = false;

    // Fijamos el tamaño
    particion_nueva->tamanio_particion = particion_base_mas_chica->tamanio_particion + particion_base_mas_grande->tamanio_particion;

    // Como no tiene pid, le ponemos -1
    particion_nueva->pid = -1;

    // Liberamos ambas particiones
    bool removio = list_remove_element(memoria_usuario->particiones, particion_base_mas_chica);
    if (!removio){
        log_error(debug_logger, "No se encontro dicha particion.");
    }
    free(particion_base_mas_chica);
    
    list_remove_element(memoria_usuario->particiones, particion_base_mas_grande);
    if(!removio){
        log_error(debug_logger, "No se encontro dicha particion.");
    }
    free(particion_base_mas_grande);

    // Agrego la partición nueva a la lista de particiones en memoria de usuario
    list_add(memoria_usuario->particiones, particion_nueva);

    return particion_nueva;
}
