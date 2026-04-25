#include "memoria_utils.h"

t_algoritmo get_algoritmo(char* algoritmo)
{
    if (string_equals_ignore_case(algoritmo, "FIRST")){
        return FIRST;
    }
    else if (string_equals_ignore_case(algoritmo, "BEST")){
        return BEST;
    }
    else if (string_equals_ignore_case(algoritmo, "WORST")){
        return WORST;
    }
    else {
        log_error(debug_logger,"No se reconoce el algoritmo.");
        abort();
    }
}

t_list* get_lista_particiones(char** particiones) 
{
    t_list* lista = list_create();

    for (int i = 0; particiones[i] != NULL; ++i) {
        info_particion* particion = malloc(sizeof(info_particion));
        
        particion->limite               = atoi(particiones[i]) - 1;
        particion->tamanio_particion    = atoi(particiones[i]);
        particion->pid                  = -1;
        particion->esta_ocupada         = false;
        particion->base                 = sumatoria_hasta_posicion(i, particiones);
        
        log_warning(debug_logger, "PARTICION CREADA - TAMAÑO = %d, BASE = %d, LIMITE = %d",particion->tamanio_particion, particion->base, particion->limite);
        list_add(lista, particion);
    }

    return lista;
}

int sumatoria_hasta_posicion(int n, char** particiones){
    int sumatoria = 0;
    for (int i = 0; i < n; i++)
    {
        sumatoria += atoi(particiones[i]);
    }
    return sumatoria;
}

t_particion get_esquema(char* esquema)
{
    if(string_equals_ignore_case(esquema,"fijas")){
        return PARTICION_FIJA;
    } 
    else if(string_equals_ignore_case(esquema,"dinamicas")){
        return PARTICION_DINAMICA;
    } 
    else {
        log_info(debug_logger, "No se logro obtener el tipo de particion");
        abort();
    }
}

t_registros *registros_en_cero()
{
    t_registros *registros = malloc(sizeof(t_registros));
    registros->ax = 0;
    registros->bx = 0;
    registros->cx = 0;
    registros->dx = 0;
    registros->ex = 0;
    registros->fx = 0;
    registros->gx = 0;
    registros->hx = 0;
    registros->pc = 0;

    return registros;
}

char* concatenar(uint32_t pid, uint32_t tid)
{
    char *pid_str = string_itoa(pid);
    char *tid_str = string_itoa(tid);
    char *dash = "-";

    // concatenar el pid y el tid para usarlo como key en el diccionario
    size_t longitud = strlen(pid_str) + strlen(tid_str) + strlen(dash) + 1; // Espacio para "-" y '\0'
    char *pid_tid = malloc(longitud);
    
    strcpy(pid_tid, pid_str);
    strcat(pid_tid, dash);
    strcat(pid_tid, tid_str);
                  
    return pid_tid;
}

void contexto_destroyer(t_contexto_ejecucion *contexto_a_eliminar)
{
    list_destroy_and_destroy_elements(contexto_a_eliminar->codigo, free);
    free(contexto_a_eliminar->registros);
    free(contexto_a_eliminar);
}

bool _no_esta_ocupada(void *ptr) 
{
    info_particion *particion  = (info_particion*) ptr;
    return !particion->esta_ocupada;
}

t_list* filtrar_lista_si_entra(uint32_t tam_proceso, t_list* lista)
{
    t_list *lista_aux = list_create();
    
    t_list_iterator *it = list_iterator_create(lista);
    while (list_iterator_has_next(it)) {
        info_particion *particion = list_iterator_next(it);
        // Si el tam_proceso entra en la particion lo agregamos a lista aux
        if (tam_proceso <= particion->tamanio_particion) {
            list_add(lista_aux, particion);
        }
    }

    list_iterator_destroy(it);
    list_destroy(lista);

    return lista_aux;
}

void* _min_base(void* a, void* b) 
{
    info_particion* particion_a = (info_particion*) a;
    info_particion* particion_b = (info_particion*) b;
    return particion_a->base <= particion_b->base ? particion_a : particion_b;
}

void* _min_limite(void* a, void* b) 
{
    info_particion* particion_a = (info_particion*) a;
    info_particion* particion_b = (info_particion*) b;
    return particion_a->base <= particion_b->base ? particion_a : particion_b;
}

void* _max_tamanio_particion(void* a, void* b) 
{
    info_particion* particion_a = (info_particion*) a;
    info_particion* particion_b = (info_particion*) b;
    return particion_a->tamanio_particion >= particion_b->tamanio_particion ? particion_a : particion_b;
}

void liberar_contextos(uint32_t pid)
{
    t_list *lista_pids_tids = dictionary_keys(contextos);

    for (int i = 0 ; i < list_size(lista_pids_tids) ; i++)
    {
        char* pid_tid = list_get(lista_pids_tids, i);
        char** pid_tid_split = string_split(pid_tid,"-");
        if (strcmp(pid_tid_split[0], string_itoa(pid)) == 0)
        {
            t_contexto_ejecucion* contexto_a_eliminar = dictionary_remove(contextos, pid_tid);
            contexto_destroyer(contexto_a_eliminar);
        }
        free(pid_tid_split);
    }
    
    free(lista_pids_tids);
}

t_list* parsear_instrucciones(char* path)
{
    char *path_completo = agregar_prefijo("scripts/", path);
    FILE *fp = fopen(path_completo, "r");
	char *linea = NULL;
	t_list* instrucciones = list_create();

	if (fp == NULL) {
        log_error(debug_logger, "Error al levantar el archivo de instrucciones....");
        exit(EXIT_FAILURE);
	}

	while (getline(&linea, &(size_t) {0}, fp) > 0) {
		linea[strcspn(linea, "\n")] = '\0';
		list_add(instrucciones, linea);
		linea = NULL; // reset
	}

	free(linea);

	//TEST: printear todas las lineas
	// for (int i=0; i<list_size(instrucciones); i++)
	// 	log_info(debug_logger, "%d - %s", i, (char *) list_get(instrucciones, i));

	fclose(fp);

	return instrucciones;
}

char* agregar_prefijo(const char* prefijo, const char* path) {
    // Calcular el tamaño necesario para la nueva cadena
    size_t nuevo_tamanio = strlen(prefijo) + strlen(path) + 1; // +1 para '\0'
    char* nuevo_path = malloc(nuevo_tamanio); // Reservar memoria

    // Concatenar prefijo y path
    strcpy(nuevo_path, prefijo);  // Copiar el prefijo
    strcat(nuevo_path, path);    // Agregar el contenido original

    return nuevo_path; // Retornar la nueva cadena
}