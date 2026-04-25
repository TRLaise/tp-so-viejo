#include "./utils.h"

char *get_config_string(t_config* config, char* clave)
{
    if (!config_has_property(config, clave)){
        log_error(debug_logger, "La clave: %s no existe en el config.", clave);
        exit(1);
    }
    return config_get_string_value(config, clave);
}

int get_config_int(t_config* config, char* clave)
{
    if (!config_has_property(config, clave)){
        log_error(debug_logger, "La clave: %s no existe en el config.", clave);
        exit(1);
    }
    return config_get_int_value(config, clave);
}

void* max_int(void* a, void* b) {
    int* int_a = (int*)a;
    int* int_b = (int*)b;

    if (*int_a > *int_b) {
        return a;
    } else {
        return b;
    }
}

// Recibe un dividendo y un divisor, si el resto es 0 devuelve la division, caso contrario redondea para arriba
int ceil_div(int dividend, int divisor)
{
    assert(divisor != 0);
    return (dividend % divisor) == 0 ? dividend / divisor : (dividend / divisor) + 1;
}