#ifndef MENSAJES_H_
#define MENSAJES_H_

//#define MENSAJE_DE_KERNEL "kernel"
//#define MENSAJE_DE_CPU "cpu"
//#define MENSAJE_DE_MEMORIA "memoria"

typedef enum {
    MODULO_KERNEL,
    MODULO_CPU,
} t_modulo;

// Este enum largo es para manejar tanto operaciones de interrupt como syscalls

typedef enum {
    // PAQUETES
    //OP_MENSAJE_INT,

    // SYSCALLS
    SET,
    READ_MEM,
    WRITE_MEM,
    SUM,
    SUB,
    JNZ,
    LOG,
    DUMP_MEMORY,
    IO,
    PROCESS_CREATE,
    THREAD_CREATE,
    THREAD_JOIN,
    THREAD_CANCEL,
    MUTEX_CREATE,
    MUTEX_LOCK,
    MUTEX_UNLOCK,
    THREAD_EXIT,
    PROCESS_EXIT,
    //interrupciones
    FIN_QUANTUM,
    DESALOJO_HILO,
    OUT_OF_MEMORY,
    
    // CPU
    INIT_PID_TID,
    INTERRUPCION,
    SOLICITAR_CONTEXTO,
    ACTUALIZAR_CONTEXTO,
    SOLICITAR_INSTRUCCION,
    OP_WRITE_MEM,
    OP_READ_MEM,
    ENVIAR_BYTES,
    INSTRUCCION,

    // MEMORIA
    CONTEXTO_EJECUCION,
    OPCODE_FIN_DUMP, //memoria indica fin del dump a kernel

    // KERNEL
    OPCODE_DUMP_MEMORY,

    // MENSAJES
    // Enviado a kernel por la memoria cuando termina su operacion
    MENSAJE_OP_TERMINADA,
    MENSAJE_NO_HAY_ESPACIO,
    MENSAJE_EXITO_DUMP,
    MENSAJE_FALLO_DUMP,
    OK, // para fs y memoria
    FALLO, // para fs y memoria
    SYSCALL,
    MENSAJE,
    INTERRUPT,
    ESPERA_OK
} op_code;


// #define MENSAJE_OP_TERMINADA 32

// #define MENSAJE_NO_HAY_ESPACIO 11

// #define MENSAJE_EXITO_DUMP 44
// #define MENSAJE_FALLO_DUMP 45

#endif