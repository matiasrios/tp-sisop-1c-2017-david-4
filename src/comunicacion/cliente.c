#include <stdio.h>
#include <commons/log.h>
#include "comunicacion.h"

int main(int argc, char **argv) {
    t_log* logger = log_create("../logs/cliente.log", "CLIENTE",true, LOG_LEVEL_TRACE);

    log_info(logger, ">>>>>>>>Inicio del proceso CLIENTE<<<<<<<<");

	char ip[20+1] = DEFAULT_IP;
	int port = DEFAULT_PORT;

	if(argc == 2) { // recibi ip por parametro
		strcpy(ip, argv[1]);
	}

	int sock = com_conectarseA(ip, port);
    if(sock <= 0) {
		log_error(logger, "Error al conectarse al servidor IP: %s PUERTO: %d", ip, port);
		return -1;
	}
    log_trace(logger, "Me conecte a IP: %s PORT: %d y tengo el SOCK: %d", ip, port, sock);

    log_trace(logger, "Voy a iniciar handshake con el SOCK: %d", sock);
	// hago handshake entre cliente y kernel --> el resto son todos iguales
	if(!com_prgker_handshake_programa(sock, logger)) {
		log_error(logger, "Error en hadshake en sock: %d", sock);
		return -1;
	}

	log_trace(logger, "Voy a armar un mensaje para enviar al servidor");
	t_mensaje *mensaje = com_nuevoMensaje(PK_TEST_ENVIAR_PROGRAMA, logger);
	com_agregarStringAMensaje(mensaje, "script", "begin\r\nvariables a, b\r\na = b + 5\r\nprint \"Hola Mundo\"\r\nend\r\n", logger);
	com_agregarIntAMensaje(mensaje, "numero", 25, logger);
	com_agregarStringAMensaje(mensaje, "saludo", "Hola Pepe", logger);

	log_trace(logger, "Mensaje que voy a enviar>>>> \n%s", com_imprimirMensaje(mensaje));
	com_enviarMensaje(mensaje, sock, logger);

	log_trace(logger, "Voy a destruir el mensaje");
	com_destruirMensaje(mensaje, logger);

	log_trace(logger, "Voy a recibir un mensaje");
	t_mensaje *mensajeRecibido = com_recibirMensaje(sock, logger);

	if(mensajeRecibido->header->tipo != PK_TEST_FINALIZO_PROGRMA){
		log_error(logger, "Error, el tipo de mensaje no es el que esperaba");
		return -1;
	}

	if(mensajeRecibido->header->len == 0){
		log_error(logger, "Error, el mensaje deberia tener un body");
		return -1;
	}

	log_trace(logger, "Mensaje que recibi>>>> \n%s", com_imprimirMensaje(mensajeRecibido));

	log_trace(logger, "Parseo el mensaje");
	char *resultado = com_leerStringAMensaje(mensajeRecibido, "resultado", logger);
	int32_t errorCode = com_leerIntAMensaje(mensajeRecibido, "error_code", logger);
	char *saludo = com_leerStringAMensaje(mensajeRecibido, "saludo", logger);
	if(strcmp(saludo, "Hola Coco")) {
		log_error(logger, "Error, esperaba el texto >Hola Coco< y me llego %s", saludo);
		return -1;
	}
	int32_t numero = com_leerIntAMensaje(mensajeRecibido, "numero", logger);
	if(numero != -17) {
		log_error(logger, "Error, esperaba el nuemero -17 y me llego %d", numero);
		return -1;
	}

	log_trace(logger, "Imprimo los valores parseados del mensaje");
	printf("resultado: %s, errorCode: %d, saludo: %s, numero: %d\n", resultado, errorCode, saludo, numero);

	log_trace(logger, "Destruyo mensaje recibido");
	com_destruirMensaje(mensajeRecibido, logger);

	log_trace(logger, "Armo un mensaje vacio");
	t_mensaje *mensajeVacio = com_nuevoMensaje(PK_TEST_EJECUTANDO, logger);
	if(mensaje->header->len != 0){
		log_error(logger, "El largo del mensaje deberia ser 0 y no lo es: %d", mensajeVacio->header->len);
		return -1;
	}

	log_trace(logger, "Voy a enviar un mensaje vacio");
	com_enviarMensaje(mensajeVacio, sock, logger);
	log_trace(logger, "Destruyo mensaje vacio");
	com_destruirMensaje(mensajeVacio, logger);

	log_trace(logger, "Voy a recibir un mensaje vacio");
	t_mensaje *mensajeVacioRecibido = com_recibirMensaje(sock, logger);
	if(mensajeVacioRecibido->header->tipo != PK_TEST_EJECUTANDO_OK) {
		log_error(logger, "El mensaje recibido es diferente al esperado: %d", mensajeVacioRecibido->header->tipo);
		return -1;
	}
	if(mensajeVacioRecibido->header->len != 0) {
		log_error(logger, "El largo del mensaje recibido deberia ser 0 y no lo es: %d", mensajeVacioRecibido->header->len);
		return -1;
	}

	log_trace(logger, "Cierro conexion");
	com_cerrarConexion(sock);

	log_info(logger, ">>>>>>>>Fin del proceso CLIENTE<<<<<<<<");
    log_destroy(logger);
	return 0;
}
