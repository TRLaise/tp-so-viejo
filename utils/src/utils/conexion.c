#include <utils/conexion.h> 

int crear_conexion(char *ip, char* puerto) {

    struct addrinfo hints, *server_info, *p;
    int socket_cliente;
    int rv;


    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; 
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;

    // Obtener información del servidor
    if ((rv = getaddrinfo(ip, puerto, &hints, &server_info)) != 0) {
        fprintf(stderr, "Error en getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Iterar sobre todas las direcciones posibles y conectarse a la primera disponible
    for(p = server_info; p != NULL; p = p->ai_next) {
        // Crear el socket
        socket_cliente = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_cliente == -1) {
            perror("Error en socket");
            continue; 
        }

        // Conectar al socket
        if (connect(socket_cliente, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_cliente);
            perror("Error en connect socket, server esperado por socket no fue inciado");
            continue; 
        }

        break;
    }

    freeaddrinfo(server_info);

    // Verificar si se pudo conectar
    if (p == NULL) {
        return -1;
    }

    return socket_cliente;
}

// server-side controller
int iniciar_servidor(char* ip, char *puerto)
{
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(ip, puerto, &hints, &servinfo) != 0) {
        log_error(debug_logger, "No se pudo crear servinfo");
        exit(1);
    }

    // Creamos el socket de escucha del servidor
    int socket_escucha = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    // Evita errores en bind despues de un crash
    int reuse = 1;
    if (setsockopt(socket_escucha, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) != 0) {
        log_error(debug_logger, "No se pudo configurar la opcion SO_REUSEADDR en el socket");
        exit(1);
    }
    if (setsockopt(socket_escucha, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(int)) != 0) {
        log_error(debug_logger, "No se pudo configurar la opcion SO_REUSEPORT en el socket");
        exit(1);
    }

    // Asociamos el socket a un puerto
    if (bind(socket_escucha, servinfo->ai_addr, servinfo->ai_addrlen) != 0) {
        log_error(debug_logger, "No se pudo bindear el socket.");
        exit(1);
    }

    // Escuchamos las conexiones entrantes
    listen(socket_escucha, SOMAXCONN);

    freeaddrinfo(servinfo);

    return socket_escucha;
}

int esperar_cliente(int socket_servidor) {
    int client_fd = accept(socket_servidor, NULL, NULL);
    if (client_fd < 0) {
        log_error(debug_logger, "Hubo un error aceptando la conexión");
        exit(1);
    }
    log_trace(debug_logger, "Se conecto un cliente");

    return client_fd;
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}
