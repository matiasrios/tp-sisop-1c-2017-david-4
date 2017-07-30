#include "comunicacion.h"

int handshakeInicia(int sock, char *mensajeEnvia, char *mensajeRecibe, char *procesosHandshake, t_log* logger);

int handshakeResponde(int sock, char *mensajeRecibe, char *mensajeResponde, char *procesosHandshake, t_log* logger);

char *resizeBody(char *body, uint32_t len, uint32_t newLen);

/* Manejo de Select */
void porCadaEvento(void *);

_Bool removerPorSocket(void *elemento, void *sock);

void removerDescriptor(void *elementoDescriptor);
/* Fin manejo de select */

int com_escucharEn(int port){
	int listener;
	struct sockaddr_in myaddr;
	int yes=1; // para setsockopt() SO_REUSEADDR, más abajo

	if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = INADDR_ANY;
	myaddr.sin_port = htons(port);
	memset(&(myaddr.sin_zero), '\0', 8);
	if (bind(listener, (struct sockaddr *)&myaddr, sizeof(myaddr)) == -1) {
		perror("Falla el bind");
		exit(1);
	}

	if (listen(listener, 10) == -1) {
		perror("listen");
		exit(1);
	}

	return listener;
}

int com_conectarseA(char *ip, int port){
	int sock;
	struct sockaddr_in DireccionServidor;
	DireccionServidor.sin_family = AF_INET;
	DireccionServidor.sin_addr.s_addr = inet_addr(ip);
	DireccionServidor.sin_port = htons(port);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (connect(sock, (void*) &DireccionServidor, sizeof(DireccionServidor)) != 0) {
		perror("No se pudo conectar");
		return -1;
	}
	return sock;
}

int com_aceptarConexion(int listener) {
	struct sockaddr_in remoteaddr;
	int addrlen;
	int sock;
	addrlen = sizeof(remoteaddr);
	if ((sock = accept(listener, (struct sockaddr*) &remoteaddr, &addrlen)) == -1){
		perror("Error en el accept");
		return -1;
	}
	return sock;
}

int com_enviar(int sock, void *mensaje, int size) {
	int bytesEnviados;
	bytesEnviados = send(sock, mensaje, size, 0);
	if (bytesEnviados <= 0) {
		perror("Error en el send");
		return -1;
	}
	return bytesEnviados;
}

int com_recibir(int sock, void *mensaje, int size) {
	int bytesRecibidos;
	if((bytesRecibidos = recv(sock, mensaje, size, 0)) < 0) {
		perror("Error en el recv.\n");
		return -1;
	}
	return bytesRecibidos;
}

void com_cerrarConexion(int sock){
	close(sock);
}

int com_cpuker_handshake_cpu(int sock, t_log* logger) {
	return handshakeInicia(sock, CK_CPU_HANDSHAKE, CK_KERNEL_HANDSHAKE, "CPU KERNEL", logger);
}

int com_cpuker_handshake_kernel(int sock, t_log* logger) {
	return handshakeResponde(sock, CK_CPU_HANDSHAKE, CK_KERNEL_HANDSHAKE, "CPU KERNEL", logger);
}

int com_cpumem_handshake_cpu(int sock, t_log* logger) {
	return handshakeInicia(sock, CM_CPU_HANDSHAKE, CM_MEMORIA_HANDSHAKE, "CPU MEMORIA", logger);
}

int com_cpumem_handshake_memoria(int sock, t_log* logger) {
	return handshakeResponde(sock, CM_CPU_HANDSHAKE, CM_MEMORIA_HANDSHAKE, "CPU MEMORIA", logger);
}

int com_kerfs_handshake_kernel(int sock, t_log* logger) {
	return handshakeInicia(sock, KF_KERNEL_HANDSHAKE, KF_FS_HANDSHAKE, "KERNEL FILESYSTEM", logger);
}

int com_kerfs_handshake_filesystem(int sock, t_log* logger) {
	return handshakeResponde(sock, KF_KERNEL_HANDSHAKE, KF_FS_HANDSHAKE, "KERNEL FILESYSTEM", logger);
}

int com_kermem_handshake_kernel(int sock, t_log* logger) {
	return handshakeInicia(sock, KM_KERNEL_HANDSHAKE, KM_MEMORIA_HANDSHAKE, "KERNEL MEMORIA", logger);
}

int com_kermem_handshake_memoria(int sock, t_log* logger) {
	return handshakeResponde(sock, KM_KERNEL_HANDSHAKE, KM_MEMORIA_HANDSHAKE, "KERNEL MEMORIA", logger);
}


int com_prgker_handshake_programa(int sock, t_log* logger) {
	return handshakeInicia(sock, PK_PROGRAMA_HANDSHAKE, PK_KERNEL_HANDSHAKE, "PROGRAMA KERNEL", logger);
}

int com_prgker_handshake_kernel(int sock, t_log* logger) {
	return handshakeResponde(sock, PK_PROGRAMA_HANDSHAKE, PK_KERNEL_HANDSHAKE, "PROGRAMA KERNEL", logger);
}

t_mensaje *com_nuevoMensaje(uint32_t tipo, t_log* logger){
	t_mensaje *nuevoMensaje;
	nuevoMensaje = malloc(sizeof(t_mensaje));
	nuevoMensaje->header = malloc(sizeof(t_header));
	nuevoMensaje->header->tipo = tipo;
	nuevoMensaje->header->len = 0;
	nuevoMensaje->body = malloc(1); // depues utilizo realloc para reservar el tamaño real
	strcpy(nuevoMensaje->body, "");
//	log_trace(logger, "Nuevo mensaje INICIALIZADO en 0>%s<", com_imprimirMensaje(nuevoMensaje));
	return nuevoMensaje;
}

void com_destruirMensaje(t_mensaje *mensaje, t_log* logger){
	free(mensaje->header);
	free(mensaje->body);
	free(mensaje);
}

t_mensaje *com_recibirMensaje(int sock, t_log* logger){
	t_header *header = malloc(sizeof(t_header));
	//log_trace(logger, "Voy a recibir el header del mensaje en el sock: %d", sock);
	int ret = com_recibir(sock, header, sizeof(t_header));
	if(ret <= 0) {
		if(ret < 0) {
			log_error(logger, "Error al recibir body mensaje del sock: %d", sock);
		}
		//log_trace(logger, "Se cerro la conexion en el sock: %d", sock);
		com_cerrarConexion(sock);
		return NULL;
	}
	//log_trace(logger, "Seteo el tipo: %d y el len: %d, en el mensaje", header->tipo, header->len);
	t_mensaje *mensaje = com_nuevoMensaje(header->tipo, logger);
	mensaje->header->len = header->len;
	//log_trace(logger, "Seteo el tipo: %d y el len: %d, en el mensaje");
	if(mensaje->header->len > 0) {
		//log_trace(logger, "El mensaje tiene body, voy a recibirlo");
		mensaje->body = realloc(mensaje->body, mensaje->header->len);
		ret = com_recibir(sock, mensaje->body, mensaje->header->len);
		if(ret <= 0) {
			if(ret < 0) {
				log_error(logger, "Error al recibir body mensaje del sock: %d", sock);
			}
			//log_trace(logger, "Se cerro la conexion en el sock: %d", sock);
			com_cerrarConexion(sock);
			return NULL;
		}
	} else {
		//log_trace(logger, "El mensaje no tiene body");
	}
	return mensaje;
}

int com_enviarMensaje(t_mensaje *mensaje, int sock, t_log* logger){
	//log_trace(logger, "Voy a enviar el header tipo: %d len: %d al sock: %d", mensaje->header->tipo, mensaje->header->len, sock);
	if(com_enviar(sock, mensaje->header, sizeof(t_header)) == -1) {
		log_error(logger, "Error al enviar header mensaje al sock: %d", sock);
		return -1;
	}
	if(mensaje->header->len > 0) {
		log_trace(logger, "Voy a enviar el body del mensaje: %s al sock: %d", com_imprimirMensaje(mensaje), sock);
		if(com_enviar(sock, mensaje->body, mensaje->header->len) == -1) {
			log_error(logger, "Error al enviar body mensaje al sock: %d", sock);
			return -1;
		}
	}

	return 1;
}

uint32_t com_agregarIntAMensaje(t_mensaje *mensaje, char *clave, int32_t valor, t_log* logger){
	uint32_t lenClave = strlen(clave) + 1;
	// el valor maximo de un int INT con signo es INT32_MAX = 2147483647 y se precisa un caracter mas para el signo
	uint32_t lenValor = strlen("2147483647") + 1 + 1 + 1;
	uint32_t lenClaveValor = lenClave + lenValor;
	uint32_t newLen = strlen(mensaje->body) + lenClaveValor + 1;
	//log_trace(logger, "Agregar INT: %d en CLAVE: %s  lenClave: %d lenValor: %d, lenClaveValor: %d, newLen: %d", valor, clave, lenClave, lenValor, lenClaveValor, newLen);

	char *numeroString = malloc(lenClaveValor);
	valor >= 0? sprintf(numeroString, "%s|%011d|", clave, valor): sprintf(numeroString, "%s|%010d|", clave, valor);

	//log_trace(logger, "Redimensiono el body de: %d a: %d", mensaje->header->len, newLen);
	//mensaje->body = resizeBody(mensaje->body, mensaje->header->len, newLen);
	mensaje->body = realloc(mensaje->body, newLen);

	//log_trace(logger, "Agrego el numero al body len: %d", lenClaveValor);
	//printf("Puntero body: %p, len actual: %d, voy a escribir en: %p", mensajeAux->body, mensajeAux->header->len, (mensajeAux->body + mensajeAux->header->len));
	//memcpy((void *) (mensajeAux->body + mensajeAux->header->len), numeroString, lenClaveValor);
	strcat(mensaje->body, numeroString);

	//log_trace(logger, "Actualizo en nuevo tamaño del mensaje de: %d a: %d", mensaje->header->len, newLen);
	mensaje->header->len = newLen;
	return newLen;
}

uint32_t com_agregarStringAMensaje(t_mensaje *mensaje, char *clave, char *valor, t_log* logger){
	uint32_t lenClave = strlen(clave) + 1;
	uint32_t lenValor = strlen(valor) + 1 + 1;
	uint32_t lenClaveValor = lenClave + lenValor + 1;
	uint32_t newLen = strlen(mensaje->body) + lenClaveValor + 1;
	//log_trace(logger, "Agregar STRING: %s en CLAVE: %s lenClave: %d lenValor: %d, lenClaveValor: %d, newLen: %d", valor, clave, lenClave, lenValor, lenClaveValor, newLen);

	char *stringString = malloc(lenClaveValor);
	sprintf(stringString, "%s|%s|", clave, valor);

	//log_trace(logger, "Redimensiono el body de: %d a: %d", mensaje->header->len, newLen);
	mensaje->body = realloc(mensaje->body, newLen);

	//log_trace(logger, "Agrego el string al body len: %d", lenClaveValor);
	strcat(mensaje->body, stringString);

	//log_trace(logger, "Actualizo en nuevo tamaño del mensaje de: %d a: %d", mensaje->header->len, newLen);
	mensaje->header->len = newLen;
	return newLen;
}

uint32_t com_agregarMatrizNxMIntAMensaje(t_mensaje *mensaje, char *clave, t_matrix_n_x_m *valor, t_log* logger){

	return mensaje->header->len;
}

uint32_t com_cargarPaginaAMensaje(t_mensaje *mensaje, char *pagina, uint32_t len, t_log* logger){
	mensaje->header->len = len;
	mensaje->body = (char *)realloc(mensaje->body, len);
	memset(mensaje->body, '\0', len);
	memcpy(mensaje->body, pagina, len);
	return mensaje->header->len;
}

int32_t com_leerIntAMensaje(t_mensaje *mensaje, char *clave, t_log* logger){
	char *buffer = malloc(mensaje->header->len);
	memcpy(buffer, mensaje->body, mensaje->header->len);
	char *token = strtok(buffer, CARACTER_SEPARADOR);
	while( token != NULL ){
		if(strcmp(clave, token) == 0) {
			char *valor = strtok(NULL, CARACTER_SEPARADOR);
			return atoi(valor);
		}
		token = strtok(NULL, CARACTER_SEPARADOR);
		token = strtok(NULL, CARACTER_SEPARADOR); // Salteo dos para comparar solo las claves
	}
	log_error(logger, "Error, no se encontro el valor para la clave: %s", clave);
	return 0;
}

char *com_leerStringAMensaje(t_mensaje *mensaje, char *clave, t_log* logger){
	char *buffer = malloc(mensaje->header->len);
	memcpy(buffer, mensaje->body, mensaje->header->len);
	char *token = strtok(buffer, CARACTER_SEPARADOR); //\0
	while( token != NULL ){
		if(strcmp(clave, token) == 0) {
			return strtok(NULL, CARACTER_SEPARADOR);
		}
		token = strtok(NULL, CARACTER_SEPARADOR);
		token = strtok(NULL, CARACTER_SEPARADOR); // Salteo dos para comparar solo las claves
	}
	log_error(logger, "Error, no se encontro el valor para la clave: %s", clave);
	return 0;
}

t_matrix_n_x_m *com_leerMatrizNxMIntDeMensaje(t_mensaje *mensaje, char *clave, t_log* logger){
	t_matrix_n_x_m *matriz;

	return matriz;
}

char *com_leerPaginaDeMensaje(t_mensaje *mensaje, t_log* logger){
	return mensaje->body;
}

char *com_imprimirMensaje(t_mensaje *mensaje){
	//printf("Puntero del mensaje %p", mensaje);
	//printf("Puntero del header %p", mensaje->header);
	char *stringMensaje = malloc(1000 + mensaje->header->len);
	sprintf(stringMensaje, "TIPO: %d\nLEN: %d\nMENSAJE:\n", mensaje->header->tipo, mensaje->header->len);
	if(mensaje->header->len > 0){
		//printf("Puntero del body %p", mensaje->body);
		char *buffer = malloc(mensaje->header->len + 1);
		memcpy(buffer, mensaje->body, mensaje->header->len + 1);
		char *token = strtok(buffer, CARACTER_SEPARADOR);
		while(token != NULL ){
			char *clave = token;
			token = strtok(NULL, CARACTER_SEPARADOR);
			char *valor = token;
			sprintf(stringMensaje, "%s-[%s]: >%s<\n", stringMensaje, clave, valor);
			token = strtok(NULL, CARACTER_SEPARADOR);
		}
	} else {
		sprintf(stringMensaje, "%s (empty)\n", stringMensaje);
	}

	return stringMensaje;
}

// Funiciones privadas :O
int handshakeInicia(int sock, char *mensajeEnvia, char *mensajeRecibe, char *procesosHandshake, t_log* logger){
	char buffer[strlen(mensajeRecibe) + 1];
	log_trace(logger, "Inicia Hanshake: voy a enviar mensaje: %s", mensajeEnvia);
	com_enviar(sock, mensajeEnvia, strlen(mensajeEnvia) + 1);
	log_trace(logger, "Inicia Hanshake: envie mensaje: %s", mensajeEnvia);
	com_recibir(sock, buffer, strlen(mensajeRecibe) + 1);
	log_trace(logger, "Inicia Hanshake: recibi mensaje: %s", buffer);
	if(strcmp(buffer, mensajeRecibe) != 0) {
		log_error(logger, "Error hanshake %s en sock: %d", procesosHandshake, sock);
		return 0;
	}
	log_trace(logger, "Inicia Handshake exitoso!!");
	return 1;
}

int handshakeResponde(int sock, char *mensajeRecibe, char *mensajeResponde, char *procesosHandshake, t_log* logger) {
	char buffer[strlen(mensajeRecibe) + 1];
	com_recibir(sock, buffer, strlen(mensajeRecibe) + 1);
	log_trace(logger, "Responde Hanshake: recibi mensaje: %s", buffer);
	if(strcmp(buffer, mensajeRecibe) != 0) {
		log_error(logger, "Error hanshake %s en sock: %d", procesosHandshake, sock);
		return 0;
	}
	log_trace(logger, "Responde Hanshake: voy a enviar mensaje respuesta: %s", mensajeResponde);
	com_enviar(sock, mensajeResponde, strlen(mensajeResponde) + 1);
	log_trace(logger, "Responde Handshake exitoso!!");
	return 1;
}

char *resizeBody(char *body, uint32_t len, uint32_t newLen){
	void *bodyTemp = body;
	body = malloc(newLen);
	memcpy(body, bodyTemp, len);
	free(bodyTemp);
	return body;
}

/* manejo de Select */
t_select *com_crearSelect(int tiempo) {
	t_select *controladorSelect;
	controladorSelect = (t_select *)malloc(sizeof(t_select));

	controladorSelect->cantidadDescriptores = 0;
	controladorSelect->maxDescriptor = 0;

	controladorSelect->listaDescriptores = list_create();
	controladorSelect->listaEventos = list_create();

	FD_ZERO(&(controladorSelect->maestroDescriptores));
	FD_ZERO(&(controladorSelect->lecturaDescriptores));

	controladorSelect->tiempoOriginal = tiempo;
	controladorSelect->tiempoRestante.tv_usec = controladorSelect->tiempoOriginal *1000;
	return controladorSelect;
}

void com_controlarSelect(t_select *controladorSelect){
	int ret;

	void *tiempo;
	if(controladorSelect->tiempoOriginal != 0) {
		tiempo = &(controladorSelect->tiempoRestante);
	} else {
		tiempo = NULL;
	}
	controladorSelect->lecturaDescriptores = controladorSelect->maestroDescriptores;
	ret = select(controladorSelect->cantidadDescriptores,
			&(controladorSelect->lecturaDescriptores),
			NULL,
			NULL,
			tiempo);
	if(ret == 0) {
		list_iterate(controladorSelect->listaEventos, porCadaEvento);
		controladorSelect->tiempoRestante.tv_usec = controladorSelect->tiempoOriginal * 1000;
	}

	int i;
	for(i = 0; i <= controladorSelect->maxDescriptor; i++){
		if(FD_ISSET(i, &(controladorSelect->lecturaDescriptores))){
			int index = 0;
			for(;;index++){
				t_manejo_descriptores *manejadorDescriptor = list_get(controladorSelect->listaDescriptores, index);
				if(manejadorDescriptor->sock == i){
					break;
				}
			}
			t_manejo_descriptores *manejadorDescriptor = list_get(controladorSelect->listaDescriptores, index);
			manejadorDescriptor->manejarDescriptor(manejadorDescriptor->parametros);
		}
	}
}

void com_destruirSelect(t_select *controladorSelect) {
	list_destroy(controladorSelect->listaDescriptores);
	list_destroy(controladorSelect->listaEventos);
	free(controladorSelect);
}

void com_agregarDescriptorAlSelect(t_select *controladorSelect, int sock, void (*manejarDescriptor)(void*), void *parametros){
	t_manejo_descriptores *nuevoDescriptor;
	nuevoDescriptor = (t_manejo_descriptores *)malloc(sizeof(t_manejo_descriptores));
	nuevoDescriptor->sock = sock;
	nuevoDescriptor->manejarDescriptor = manejarDescriptor;
	nuevoDescriptor->parametros = parametros;
	list_add(controladorSelect->listaDescriptores, nuevoDescriptor);
	FD_SET(sock, &(controladorSelect->maestroDescriptores));
	controladorSelect->cantidadDescriptores++;
	if(sock > controladorSelect->maxDescriptor) {
		controladorSelect->maxDescriptor = sock;
	}
}

void com_agregarEventoTemporalAlSelect(t_select *controladorSelect, void (*manejarEvento)(void*), void *parametros){
	t_manejo_temporal *nuevoEvento;
	nuevoEvento = (t_manejo_temporal *)malloc(sizeof(t_manejo_temporal));
	// nuevoEvento->manejarEvento = manejarEvento; // lo comente porque da error al compilar
	// nuevoEvento->parametros = parametros;
	list_add(controladorSelect->listaDescriptores, nuevoEvento);
}

void com_quitarDescriptorAlSelect(t_select *controladorSelect, int sock){
	int index = 0;
	for(;;index++){
		t_manejo_descriptores *manejadorDescriptor = list_get(controladorSelect->listaDescriptores, index);
		if(manejadorDescriptor->sock == sock){
			break;
		}
	}
	list_remove_and_destroy_element(controladorSelect->listaDescriptores, index, (void *) removerDescriptor);
	FD_CLR(sock, &(controladorSelect->maestroDescriptores));
	controladorSelect->cantidadDescriptores--;
}

void porCadaEvento(void *evento){
	t_manejo_temporal *eventoTemporal = (t_manejo_temporal *)evento;
	// eventoTemporal->manejarEvento(eventoTemporal->parametros); // Lo comente porque esta dando error para compilar
}

_Bool removerPorSocket(void *elementoLista, void *sock){
	t_manejo_descriptores *manejadorDescriptor = (t_manejo_descriptores *)elementoLista;
	if(manejadorDescriptor->sock == (int)*((int *)sock)) {
		return true;
	}
	return false;
}

void removerDescriptor(void *elementoDescriptor) {
	t_manejo_descriptores *manejadorDescriptor = (t_manejo_descriptores *)manejadorDescriptor;
	free(manejadorDescriptor->parametros);
	free(manejadorDescriptor->manejarDescriptor);
	free(manejadorDescriptor);
	close(manejadorDescriptor->sock);
}
/* Fin manejo de Select */
