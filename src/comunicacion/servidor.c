#include <stdio.h>
#include <commons/log.h>
#include "comunicacion.h"

int main(int argc, char **argv) {
    t_log* logger = log_create("../logs/servidor.log", "SERVIDOR",true, LOG_LEVEL_TRACE);

    log_info(logger, ">>>>>>>>Inicio del proceso SERVIDOR<<<<<<<<");
	int port = DEFAULT_PORT;

	int listener = com_escucharEn(port);
	if(listener < 0) {
		log_error(logger, "Error al crear listener para el servidor en PUERTO: %d", port);
		return -1;
	}
	log_trace(logger, "Estoy escuchando conexiones en PUERTO: $d con el LISTENER: %d", port, listener);

	while (1) {
		log_trace(logger, "Voy a aceptar conexiones en LISTENER: %d", listener);
		int sock = com_aceptarConexion(listener);
		log_trace(logger, "Acepte una conexion y se creo el SOCK: %d", sock);
		if(sock < 0) {
			log_error(logger, "Error al aceptar conexion en listener: %d", listener);
			return -1;
		}

		log_trace(logger, "Voy a responder handshake con el SOCK: %d", sock);
		// hago handshake entre cliente y kernel --> el resto son todos iguales
		if(!com_prgker_handshake_kernel(sock, logger)) {
			log_error(logger, "Error en hadshake en sock: %d", sock);
			return -1;
		}

		log_trace(logger, "Voy a recibir un mensaje");
		t_mensaje *mensaje = com_recibirMensaje(sock, logger);
		if(mensaje == NULL) {
			log_error(logger, "Error al recibir mensaje del cliente en el sock: %d", sock);
			return -1;
		}

		if(mensaje->header->tipo != PK_TEST_ENVIAR_PROGRAMA){
			log_error(logger, "Error, el tipo de mensaje no es el que esperaba");
			return -1;
		}

		if(mensaje->header->len == 0){
			log_error(logger, "Error, el mensaje deberia tener un body");
			return -1;
		}

		log_trace(logger, "Recibi el mensaje: %s", com_imprimirMensaje(mensaje));

		log_trace(logger, "voy a parsear el mensaje recibido");
		char *script = com_leerStringAMensaje(mensaje, "script", logger);
		char *saludo = com_leerStringAMensaje(mensaje, "saludo", logger);
		if(strcmp(saludo, "Hola Pepe")) {
			log_error(logger, "Error, esperaba el texto >Hola Pepe< y me llego %s", saludo);
			return -1;
		}

		int32_t numero = com_leerIntAMensaje(mensaje, "numero", logger);
		if(numero != 25) {
			log_error(logger, "Error, esperaba el numero 25 y me llego %d", numero);
			return -1;
		}

		log_trace(logger, "Voy a imprimir el mensaje recibido");
		printf("script: %s, saludo: %s, numero: %d\n", script, saludo, numero);

		log_trace(logger, "Armo un mensaje para responder");
		t_mensaje *mensajeRespondo = com_nuevoMensaje(PK_TEST_FINALIZO_PROGRMA, logger);
		com_agregarStringAMensaje(mensajeRespondo, "resultado", "Finalizo el programa", logger);
		com_agregarIntAMensaje(mensajeRespondo, "error_code", 0, logger);
		com_agregarStringAMensaje(mensajeRespondo, "saludo", "Hola Coco", logger);
		com_agregarIntAMensaje(mensajeRespondo, "numero", -17, logger);

		log_trace(logger, "Enl mensaje que arme para responder es: %s", com_imprimirMensaje(mensajeRespondo));

		log_trace(logger, "Voy a enviar el mesaje");
		com_enviarMensaje(mensajeRespondo, sock, logger);
		log_trace(logger, "Voy a destruir el mensaje");
		com_destruirMensaje(mensajeRespondo, logger);

		log_trace(logger, "Voy a recibir un mensaje vacio");
		t_mensaje *mensajeVacioRecibido = com_recibirMensaje(sock, logger);
		if(mensajeVacioRecibido->header->tipo != PK_TEST_EJECUTANDO) {
			log_error(logger, "El mensaje recibido es diferente al esperado: %d", mensajeVacioRecibido->header->tipo);
			return -1;
		}
		if(mensajeVacioRecibido->header->len != 0) {
			log_error(logger, "El largo del mensaje recibido deberia ser 0 y no lo es: %d", mensajeVacioRecibido->header->len);
			return -1;
		}

		log_trace(logger, "Armo un mensaje vacio");
		t_mensaje *mensajeVacio = com_nuevoMensaje(PK_TEST_EJECUTANDO_OK, logger);
		if(mensajeVacio->header->len != 0){
			log_error(logger, "El largo del mensaje deberia ser 0 y no lo es: %d", mensajeVacio->header->len);
			return -1;
		}

		log_trace(logger, "Voy a enviar un mensaje vacio");
		com_enviarMensaje(mensajeVacio, sock, logger);
		log_trace(logger, "Destruyo mensaje vacio");
		com_destruirMensaje(mensajeVacio, logger);


		log_trace(logger, "Voy a recibir el mensaje de desconexion");
		t_mensaje *mensajeDesconexion = com_recibirMensaje(sock, logger);
		if(mensajeDesconexion == NULL) {
			log_info(logger, "Se desconecto el sock: %d", sock);
		} else {
			log_error(logger, "Error, el sock: %d, se deberia desconectar y no lo hizo", sock);
		}
	}

	log_info(logger, ">>>>>>>>Fin del proceso SERVIDOR<<<<<<<<");
    log_destroy(logger);
	return 0;
}
