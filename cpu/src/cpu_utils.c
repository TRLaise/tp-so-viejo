#include "cpu_utils.h"

t_tcb* recibir_tcb(int fd)
{
    t_list* paquete = recibir_paquete(fd);
    return (t_tcb*) list_get(paquete, 0);
}

t_contexto_ejecucion* solicitar_contexto_ejecucion(int fd, uint32_t pid, uint32_t tid)
{
    t_paquete* paquete = crear_paquete(SOLICITAR_CONTEXTO);
    agregar_a_paquete(paquete, &(pid), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(tid), sizeof(uint32_t));
    enviar_paquete(paquete, fd);
    eliminar_paquete(paquete);

    op_code code_op = recibir_operacion(fd);
    if (code_op == CONTEXTO_EJECUCION) {
        t_list* data = recibir_paquete(fd);
        t_contexto_ejecucion *contexto = recibir_contexto_ejecucion(data);
        return contexto;
    } else {
        log_error(debug_logger, "solicitar_contexto_ejecucion() - codigo de operacion desconocido...");
        abort();
    }
}

t_contexto_ejecucion* recibir_contexto_ejecucion(t_list* data)
{
    t_contexto_ejecucion* contexto = malloc(sizeof(t_contexto_ejecucion));
    t_registros* registro_contexto = malloc(sizeof(t_registros));

    uint32_t* base = list_get(data, 0);
    contexto->base = *base;
    uint32_t* limite = list_get(data, 1);
    contexto->limite = *limite;
    uint32_t* pid = list_get(data, 2);
    contexto->pid = *pid;
    uint32_t* tid = list_get(data, 3);
    contexto->tid = *tid;
    t_list* codigo = list_get(data, 4);
    contexto->codigo = codigo;

    contexto->registros = malloc(sizeof(t_registros));

    registro_contexto->ax = *(uint32_t*)list_get(data, 5);
    registro_contexto->bx = *(uint32_t*)list_get(data, 6);
    registro_contexto->cx = *(uint32_t*)list_get(data, 7);
    registro_contexto->dx = *(uint32_t*)list_get(data, 8);
    registro_contexto->ex = *(uint32_t*)list_get(data, 9);
    registro_contexto->fx = *(uint32_t*)list_get(data, 10);
    registro_contexto->gx = *(uint32_t*)list_get(data, 11);
    registro_contexto->hx = *(uint32_t*)list_get(data, 12);
    registro_contexto->pc = *(uint32_t*)list_get(data, 13);

    contexto->registros = registro_contexto;

    return contexto;
}

char* solicitar_instruccion(int fd, uint32_t pid, uint32_t tid, uint32_t pc)
{
    t_paquete* paquete = crear_paquete(SOLICITAR_INSTRUCCION);
    agregar_a_paquete(paquete, &(pid), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(tid), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(pc), sizeof(uint32_t));
    enviar_paquete(paquete, fd);
    eliminar_paquete(paquete);

    op_code code_op = recibir_operacion(fd);
    if (code_op == INSTRUCCION) {
        t_list* data = recibir_paquete(fd);
        char *instruccion = (char*) list_get(data, 0);
        return instruccion;
    } else {
        log_error(debug_logger, "solicitar_instruccion() - codigo de operacion desconocido...");
        //abort();
    }
}

t_instruccion* parsear_linea(char* linea)
{
    t_instruccion* instruccion = malloc(sizeof(t_instruccion));
    instruccion->nombre = string_new();
    instruccion->params = list_create();

    char* token = strtok(linea, " ");
    instruccion->nombre = strdup(token);

    while ((token = strtok(NULL, " ")) != NULL)
        list_add(instruccion->params, strdup(token));

    return instruccion;
}

op_code parsear_instruccion(char* nombre)
{
    /* INSTRUCCIONES */
    if (!strcmp("SET", nombre)) return SET;
    if (!strcmp("READ_MEM", nombre)) return READ_MEM;
    if (!strcmp("WRITE_MEM", nombre)) return WRITE_MEM;
	if (!strcmp("SUM", nombre)) return SUM;
	if (!strcmp("SUB", nombre)) return SUB;
	if (!strcmp("JNZ", nombre)) return JNZ;
	if (!strcmp("LOG", nombre)) return LOG;

    /* SYSCALLS */ 
    if (!strcmp("DUMP_MEMORY", nombre)) return DUMP_MEMORY;
    if (!strcmp("IO", nombre)) return IO;
    if (!strcmp("PROCESS_CREATE", nombre)) return PROCESS_CREATE;
    if (!strcmp("THREAD_CREATE", nombre)) return THREAD_CREATE;
    if (!strcmp("THREAD_JOIN", nombre)) return THREAD_JOIN;
    if (!strcmp("THREAD_CANCEL", nombre)) return THREAD_CANCEL;
    if (!strcmp("MUTEX_CREATE", nombre)) return MUTEX_CREATE;
    if (!strcmp("MUTEX_LOCK", nombre)) return MUTEX_LOCK;
    if (!strcmp("MUTEX_UNLOCK", nombre)) return MUTEX_UNLOCK;
    if (!strcmp("THREAD_EXIT", nombre)) return THREAD_EXIT;
    if (!strcmp("PROCESS_EXIT", nombre)) return PROCESS_EXIT;

    return -1;
}

void set(t_contexto_ejecucion* context, char* registro, uint32_t valor)
{
    if(string_equals_ignore_case(registro, "AX"))
        context->registros->ax = valor;
    else if(string_equals_ignore_case(registro, "BX"))
        context->registros->bx = valor;
    else if(string_equals_ignore_case(registro, "CX"))
        context->registros->cx = valor;
    else if(string_equals_ignore_case(registro, "DX"))
        context->registros->dx = valor;
    else if(string_equals_ignore_case(registro, "EX"))
        context->registros->ex = valor;
    else if(string_equals_ignore_case(registro, "FX"))
        context->registros->fx = valor;
    else if(string_equals_ignore_case(registro, "GX"))
        context->registros->gx = valor;
    else if(string_equals_ignore_case(registro, "HX"))
        context->registros->hx = valor;
    else
        log_error(debug_logger, " set() - registro %s no valido...", registro);
}

void read_mem(t_contexto_ejecucion* context, char* reg1, char* reg2)
{
    uint32_t reg_datos = obtener_valor_registro(context, reg1);
    int dir_logica = (int) obtener_valor_registro(context, reg2);

    int dir_fisica = mmu(context, dir_logica);

    if (dir_fisica != -1) {
        uint32_t valor = leer_de_memoria(context, dir_fisica);
        if (valor >= 0) {
            reg_datos = valor;
            log_info(cpu_logger, "## TID: <%d> - Accion: LEER - Direccion Fisica: <%d> ", context->tid, dir_fisica);
        }
    }
}

uint32_t leer_de_memoria(t_contexto_ejecucion* context, int dir_fisica)
{
    t_paquete* paquete = crear_paquete(OP_READ_MEM);
    agregar_a_paquete(paquete, &(context->pid), sizeof(int));
    agregar_a_paquete(paquete, &(context->tid), sizeof(int));
    agregar_a_paquete(paquete, &(dir_fisica), sizeof(int));
    enviar_paquete(paquete, conexion_memoria);
    eliminar_paquete(paquete);

    op_code code_op = recibir_operacion(conexion_memoria);
    if (code_op == ENVIAR_BYTES) {
        t_list* data = recibir_paquete(conexion_memoria);
        return *(uint32_t*) list_get(data, 0);
    }
    return -1;
}

void write_mem(t_contexto_ejecucion* context, char* reg1, char* reg2)
{
    int dir_logica = (int) obtener_valor_registro(context, reg1);
    uint32_t valor = obtener_valor_registro(context, reg2);

    int dir_fisica = mmu(context, dir_logica);

    if (dir_fisica != -1) {
        escribir_en_memoria(context, dir_fisica, valor);
    }
}

void escribir_en_memoria(t_contexto_ejecucion* context, int dir_fisica, uint32_t valor)
{
    t_paquete* paquete = crear_paquete(OP_WRITE_MEM);
    agregar_a_paquete(paquete, &(context->pid), sizeof(int));
    agregar_a_paquete(paquete, &(context->tid), sizeof(int));
    agregar_a_paquete(paquete, &dir_fisica, sizeof(int));
    agregar_a_paquete(paquete, &valor, sizeof(uint32_t));
    enviar_paquete(paquete, conexion_memoria);
    eliminar_paquete(paquete);

    op_code code_op = recibir_operacion(conexion_memoria);
    t_list* paquete_2 = recibir_paquete(conexion_memoria);
    if (code_op == OK) {
        log_info(cpu_logger, "## TID: <%d> - Accion: ESCRIBIR - Direccion Fisica: <%d> ", context->tid, dir_fisica);
    } else if (code_op == FALLO) {
        log_error(debug_logger, "No se pudo escribir en el TID <%d>", context->tid);
    } else {
        log_warning(debug_logger, "Operacion desconocida %d - escribir_en_memoria()", code_op);
    }
}

int mmu(t_contexto_ejecucion* context, int dir_logica)
{
    if (dir_logica + sizeof(uint32_t) > context->limite) {
        actualizar_contexto_ejecucion(context);
        enviar_seg_fault(context);
        evacuate_flag = true;
        return -1;
    }

    return context->base + dir_logica;
}

void enviar_seg_fault(t_contexto_ejecucion* context)
{
    t_paquete* paquete = crear_paquete(OUT_OF_MEMORY);
    agregar_a_paquete(paquete, &(context->pid), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(context->tid), sizeof(uint32_t));
    enviar_paquete(paquete, dispatch_fd);
    eliminar_paquete(paquete);
}

void sum(t_contexto_ejecucion* context, char* reg1, char* reg2)
{
    uint32_t valor_reg1 = (uint32_t) obtener_valor_registro(context, reg1);
    uint32_t valor_reg2 = (uint32_t) obtener_valor_registro(context, reg2);
    
    set(context, reg1, (valor_reg1 + valor_reg2));
}

void sub(t_contexto_ejecucion* context, char* reg1, char* reg2)
{
    uint32_t valor_reg1 = (uint32_t) obtener_valor_registro(context,reg1);
    uint32_t valor_reg2 = (uint32_t) obtener_valor_registro(context,reg2);

    set(context, reg1, (uint32_t)(valor_reg1 - valor_reg2));
}

void actualizar_contexto_ejecucion(t_contexto_ejecucion* context)
{
    t_paquete* paquete = crear_paquete(ACTUALIZAR_CONTEXTO);
    agregar_a_paquete(paquete, &(context->pid), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(context->tid), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(context->registros->ax), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(context->registros->bx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(context->registros->cx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(context->registros->dx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(context->registros->ex), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(context->registros->fx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(context->registros->gx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(context->registros->hx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(context->registros->pc), sizeof(uint32_t));
    enviar_paquete(paquete, conexion_memoria);
    eliminar_paquete(paquete);

    op_code code_op = recibir_operacion(conexion_memoria);
    t_list* data = recibir_paquete(conexion_memoria);
    if (code_op != OK) {
        log_error(debug_logger, "solicitar_instruccion() - codigo de operacion desconocido...");
        //abort();
    }
}

op_code recibir_motivo_exit()
{
    t_list* paquete = recibir_paquete(interrupt_fd);
    return *(op_code*) list_get(paquete, 0);
}

bool recibir_desalojo()
{
    op_code cod_op = recibir_operacion(dispatch_fd);
    t_list* data = recibir_paquete(dispatch_fd); // Do nothing
    return cod_op == DESALOJO_HILO;
}

void enviar_interrupcion(op_code instruccion)
{
    t_paquete* paquete = crear_paquete(INTERRUPCION);
    agregar_a_paquete(paquete, &(instruccion), sizeof(op_code));
    enviar_paquete(paquete, interrupt_fd);
    eliminar_paquete(paquete);
}

void enviar_tid(uint32_t tid) 
{
    t_paquete* paquete = crear_paquete(INTERRUPCION);
    agregar_a_paquete(paquete, &motivo, sizeof(op_code));
    agregar_a_paquete(paquete, &tid, sizeof(uint32_t));
    enviar_paquete(paquete, dispatch_fd);
    eliminar_paquete(paquete);
}

void enviar_syscall(op_code syscall, char* param1, char* param2, char* param3)
{
    t_paquete* paquete = crear_paquete(SYSCALL);

    agregar_a_paquete(paquete, &(syscall), sizeof(op_code));
    agregar_a_paquete(paquete, param1 != NULL ? param1 : NULL, param1 != NULL ? strlen(param1) + 1 : 0); 
    agregar_a_paquete(paquete, param2 != NULL ? param2 : NULL, param2 != NULL ? strlen(param2) + 1 : 0); 
    agregar_a_paquete(paquete, param3 != NULL ? param3 : NULL, param3 != NULL ? strlen(param3) + 1 : 0); 
    log_warning(debug_logger, "Estoy enviando un TID %s en syscall %d", param1, syscall);

    enviar_paquete(paquete, dispatch_fd);
    eliminar_paquete(paquete);
}

uint32_t obtener_valor_registro(t_contexto_ejecucion* context, char* valor)
{
    if (strcmp(valor, "AX") == 0)
        return context->registros->ax;
    else if (strcmp(valor, "BX") == 0)
        return context->registros->bx;
    else if (strcmp(valor, "CX") == 0)
        return context->registros->cx;
    else if (strcmp(valor, "DX") == 0)
        return context->registros->dx;
    else if (strcmp(valor, "EX") == 0)
        return context->registros->ex;
    else if (strcmp(valor, "FX") == 0)
        return context->registros->fx;
    else if (strcmp(valor, "GX") == 0)
        return context->registros->gx;
    else if (strcmp(valor, "HX") == 0)
        return context->registros->hx;
    else {
        log_error(debug_logger, "obtener_valor_registro() - error: registro desconocido '%s'", valor);
        return -1;
    }
}

char* imprimir_parametros(t_list* params)
{
    if (!list_is_empty(params)) {
        char* str_params = string_new();
        //string_append(&str_params, "[");
        for (int i = 0; i < list_size(params); ++i) {
            char* str = (char*) list_get(params, i);
            if (str && !string_is_empty(str)){
                string_append(&str_params, str);
                //string_append(&str_params, list_get(params, i+1) ? "]" : ",");
            }
        }
        return str_params;
    } else {
        return "[]";
    }
}