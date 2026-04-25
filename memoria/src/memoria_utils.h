#ifndef MEMORIA_UTILS_H_
#define MEMORIA_UTILS_H_

#include <estructuras.h>
#include <memoria.h>

t_algoritmo get_algoritmo(char* algoritmo);
t_list* get_lista_particiones(char** particiones);
t_particion get_esquema(char* esquema);
t_registros *registros_en_cero();
char* concatenar(uint32_t pid,uint32_t tid);
void contexto_destroyer(t_contexto_ejecucion *contexto_a_eliminar);
bool _no_esta_ocupada(void *ptr);
t_list* filtrar_lista_si_entra(uint32_t tam_proceso, t_list* lista);
void* _min_base(void* a, void* b);
void* _min_limite(void* a, void* b); 
void* _max_tamanio_particion(void* a, void* b);
int sumatoria_hasta_posicion(int n, char** particiones);
t_list* parsear_instrucciones(char* path);
char* agregar_prefijo(const char* prefijo, const char* path);
void liberar_contextos(uint32_t pid);

#endif // MEMORIA_UTILS_H_