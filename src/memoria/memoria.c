#include "memoria.h"

// Semaforos
pthread_mutex_t mutexMemoriaProcesos;
pthread_mutex_t mutexTablaDePaginasInv;
pthread_mutex_t mutexCache;

// Hilos
pthread_t hiloKernel;
t_list *listaCPUs;
pthread_t hiloConsola;

int main(int argc, char **argv) {

	int sockKernel;
	fd_set master;
	fd_set readfds;
	FD_ZERO(&master);
	FD_ZERO(&readfds);
	int maxSock = 10; // deberia ser un numero mayor al de todos los descriptores

	crearLogger();

	leerArchConfig(argv[1]);

	escucharConexiones();

	FD_SET(listener, &master);
	FD_SET(0, &readfds);

	inicializarSemaforos();

	inicializarEstructuras();

	iniciarConsola();

	log_trace(logger, "Esperando conexion del kernel");
	sockKernel = com_aceptarConexion(listener);
	if (!com_kermem_handshake_memoria(sockKernel, logger)) {
		log_error(logger, "Error en el handshake con el kernel sock: %d", sockKernel);
		return -1;
	}
	log_trace(logger, "Handshake con kernel OK");
	crearHiloKernel(sockKernel);

	while (continuarEjecutando) {
		readfds = master;
		select(maxSock, &readfds, NULL, NULL, NULL);
		if (FD_ISSET(listener, &readfds)) {
			int sockNuevoCPU = com_aceptarConexion(listener);
			if (!com_cpumem_handshake_memoria(sockNuevoCPU, logger)) {
				log_error(logger, "Error en el handshake con el cpu");
				return -1;
			}
			crearHiloCPU(sockNuevoCPU);
		}
	}

	pthread_join(hiloKernel, NULL);
	pthread_join(hiloConsola, NULL);

	destruirSemaforos();
	liberarEstructuras();

	log_trace(logger, ">>>>>>>>Fin del proceso MEMORIA<<<<<<<<");
	log_destroy(logger);
} //end main

void crearLogger() {
	logger = log_create("../logs/memoria.log", "MEMORIA", true, LOG_LEVEL_INFO);
	log_trace(logger, ">>>>>>>>Inicio del proceso MEMORIA<<<<<<<<");
}

void leerArchConfig(char *nombreArchivo) {
	config = config_create(nombreArchivo);
	if (config != NULL) {
		log_trace(logger, "Archivo abierto");
		marcos = config_get_int_value(config, "MARCOS");
		marco_size = config_get_int_value(config, "MARCO_SIZE");
		retardoMemoria = config_get_int_value(config, "RETARDO_MEMORIA");
		puerto = config_get_int_value(config, "PUERTO");
		entradas_cache = config_get_int_value(config, "ENTRADAS_CACHE");
		entradas_x_proc = config_get_int_value(config, "CACHE_X_PROC");
		if (entradas_x_proc == 0) {
			entradas_cache = 0;
		}
		log_trace(logger, "Archivo de conf leido correcamente");
	} else {
		log_error(logger, "Error al abrir el archivo de configuracion: %s, (memoria arch.cfg)", nombreArchivo);
	}
}

void escucharConexiones() {
	listener = com_escucharEn(puerto);
	if (listener <= 0) {
		log_error(logger, "Error al crear listener en PUERTO: %d", puerto);
	}
}

void inicializarSemaforos() {
	pthread_mutex_init(&mutexMemoriaProcesos, NULL);
	pthread_mutex_init(&mutexTablaDePaginasInv, NULL);
	pthread_mutex_init(&mutexCache, NULL);
}

void destruirSemaforos() {
	log_trace(logger, "Se eliminan los semaforos");
	pthread_mutex_destroy(&mutexMemoriaProcesos);
	pthread_mutex_destroy(&mutexTablaDePaginasInv);
	pthread_mutex_destroy(&mutexCache);
}

void inicializarEstructuras() {
	crearMemoria();
	crearCache();
	incializarListaCPU();
	crearListaProcActivos();
}

void iniciarConsola() {
	pthread_create(&hiloConsola, NULL, (void *)consolaMemoria, NULL);
}

void liberarEstructuras() {
	log_trace(logger, "Se liberan las estructuras administrativas");
	free(memoria);
	free(memCache);
}

void crearMemoria() {
	memoria = malloc(marcos * marco_size);
	memset(memoria, '\0', (marcos * marco_size));
	tablaPagsInvert = (t_elem_pags_invert *) memoria;
	uint32_t espacioAdm = (marcos * sizeof(t_elem_pags_invert)); //6000 bytes
	log_trace(logger, "el espacio adm pesa: %d Bytes", espacioAdm);
	bloquesAdm = (espacioAdm / marco_size) +1; // 24 (en realidad 23,algo) bloques
	framesOcupados = bloquesAdm;
	inicioMemoriaProcesos = memoria + bloquesAdm * marco_size;
	//printf("fin tabla pags invert: %d \n", &tablaPagsInvert[500]); --> esto imprime la pos de mem
	log_trace(logger, "se necesitan: %d bloques para el espacio adm", bloquesAdm);
	int i;
	log_trace(logger, "Inicializo bloques administrativos");
	for (i = 0; i < bloquesAdm; i++) {
		tablaPagsInvert[i].pid = -2; //pid -2 es adm
		tablaPagsInvert[i].pag = -2; //pag -2 es adm
		tablaPagsInvert[i].frame = i;
	}
	for (i = bloquesAdm; i < marcos; i++) {
		tablaPagsInvert[i].pid = -1; //pid -1 es libre para procesos
		tablaPagsInvert[i].pag = -1; //pag -1 es libre para procesos
		tablaPagsInvert[i].frame = i;
	}
	log_trace(logger, "Memoria Contigua Reservada!");
}

void crearCache() {
	if (cacheActivada()) {
		memCache = malloc(entradas_cache * sizeof(t_elem_cache));
		int i;
		pthread_mutex_lock(&mutexCache);
		for ( i = 0 ; i < entradas_cache ; i++) {
			memCache[i].pid = CACHE_LIBRE;
			memCache[i].numPag = CACHE_LIBRE;
			memCache[i].numBloque = 0; //numero de bloque en memoria posta
			memCache[i].vecesUsado = 0;
		}
		pthread_mutex_unlock(&mutexCache);
		log_trace(logger, "Memoria cache creada");
	} else {
		log_trace(logger, "Sistema con cache desactivada");
	}
}

int cacheActivada() {
	if (entradas_cache > 0) {
		return 1;
	} else {
		return 0;
	}
}

void incializarListaCPU() {
	listaCPUs = list_create();
}

void crearListaProcActivos() {
	listaProcActivos = list_create();
}

void crearHiloKernel(int sockKernel) {
	pthread_create(&hiloKernel, NULL, (void *)funcionHiloKernel, sockKernel);
}

void funcionHiloKernel(int sockKernel) {

	t_mensaje *mensaje = com_recibirMensaje(sockKernel, logger);

	while(mensaje != NULL) {
		t_mensaje *respuesta;
		char* pagina;
		char* contenidoPagina;
		int32_t resultadoDeLaReserva;
		int32_t resultadoDeGuardar;
		int32_t resultadoDeLiberar;
		int32_t resultadoDeFinalizar;
		int32_t pid = com_leerIntAMensaje(mensaje, "pid", logger);
		int32_t numPag = com_leerIntAMensaje(mensaje, "numPag", logger);
		log_trace(logger,"EEl mensaje que me llego %d",mensaje->header->tipo);

		switch (mensaje->header->tipo) {
			case KM_RESERVAR_PAGINA:
				log_trace(logger, "Inicio funcion reservar pagina...");
				log_trace(logger, "Marce quiere reservar la pagina: %d para el pid: %d", numPag, pid);
				resultadoDeLaReserva = reservarPagina(pid, numPag);
				respuesta = com_nuevoMensaje(MK_RESERVA_PAGINA_RESPUESTA, logger);
				com_agregarIntAMensaje(respuesta, "resultadoReserva", resultadoDeLaReserva, logger);
				com_agregarIntAMensaje(respuesta, "size", marco_size, logger);
				com_enviarMensaje(respuesta, sockKernel, logger);
				com_destruirMensaje(mensaje, logger);
				com_destruirMensaje(respuesta, logger);
				break;
			case KM_GUARDAR_PAGINA:
				log_trace(logger, "Inicio funcion guardar pagina...");
				t_mensaje *mensajePagina = com_recibirMensaje(sockKernel, logger);
				if (mensajePagina->header->tipo != KM_LA_PAGINA) {
					log_error(logger, "Hubo un error al recibir el contenido de la pagina");
					break;
				}
				pagina = com_leerPaginaDeMensaje(mensajePagina, logger);
				resultadoDeGuardar = guardarPagina(pid, numPag, pagina);
				respuesta = com_nuevoMensaje(MK_GUARDAR_PAGINA_RESPUESTA, logger);
				com_agregarIntAMensaje(respuesta, "resultadoGuardar", resultadoDeGuardar, logger);
				com_enviarMensaje(respuesta, sockKernel, logger);
				com_destruirMensaje(mensaje, logger);
				com_destruirMensaje(respuesta, logger);
				break;
			case KM_LIBERAR_PAGINA:
				log_trace(logger, "Inicio funcion liberar pagina...");
				resultadoDeLiberar = liberarPagina(pid, numPag);
				respuesta = com_nuevoMensaje(MK_LIBERAR_PAGINA_RESPUESTA, logger);
				com_agregarIntAMensaje(respuesta, "resultadoLiberar", resultadoDeLiberar, logger);
				com_enviarMensaje(respuesta, sockKernel, logger);
				com_destruirMensaje(mensaje, logger);
				com_destruirMensaje(respuesta, logger);
				break;
			case KM_CONSULTAR_PAGINA:
				contenidoPagina = consultarPagina(pid, numPag);
				respuesta = com_nuevoMensaje(MK_CONSULTAR_PAGINA_RESPUESTA, logger);
				com_agregarIntAMensaje(respuesta, "resultadoConsulta", 1, logger);
				com_enviarMensaje(respuesta, sockKernel, logger);
				t_mensaje *laPagina = com_nuevoMensaje(KM_LA_PAGINA, logger);
				com_cargarPaginaAMensaje(laPagina, contenidoPagina, marco_size, logger);
				com_enviarMensaje(laPagina, sockKernel, logger);
				com_destruirMensaje(mensaje, logger);
				com_destruirMensaje(respuesta, logger);
				com_destruirMensaje(laPagina, logger);
				break;
			case KM_FIN_PROCESO:
				log_trace(logger, "Inicio funcion finalizar proceso...");
				resultadoDeFinalizar = finalizarProceso(pid);
				respuesta = com_nuevoMensaje(MK_FIN_PROCESO_RESPUESTA, logger);
				com_agregarIntAMensaje(respuesta, "resultadoFinalizar", resultadoDeFinalizar, logger);
				com_enviarMensaje(respuesta, sockKernel, logger);
				com_destruirMensaje(mensaje, logger);
				com_destruirMensaje(respuesta, logger);
				break;
			default:
				log_trace(logger, "Mensaje Incorrecto");
				break;
		}

		mensaje = com_recibirMensaje(sockKernel, logger);
	}

	log_info(logger, "Se desconecto el kernel");

}//end funcionHiloKernel

int reservarPagina(int pid, int numPag) {
	int pos = buscarPaginaLibre();
	if (pos != -1) {
		pthread_mutex_lock(&mutexTablaDePaginasInv);
		tablaPagsInvert[pos].pid = pid;
		tablaPagsInvert[pos].pag = numPag;
		pthread_mutex_unlock(&mutexTablaDePaginasInv);
		framesOcupados++;
		actualizarListaDeProcesos(pid);
		log_trace(logger, "Pagina reservada correctamente para el proceso: %d \n", pid);
		return EXIT_SUCCESS;
	} else {
		log_trace(logger, "No hay paginas disponibles para el proceso: %d \n", pid);
		return EXIT_FAILURE;
	}
}

void actualizarListaDeProcesos(int pid) {
	int *nuevoPid;
	bool esPidBuscado(int* pidBuscado){ return  (*pidBuscado) == pid ;}

	int *resultado = (int*) list_find(listaProcActivos, (void *) esPidBuscado);

	if (resultado == NULL) {
		nuevoPid = malloc(sizeof(int));
		*nuevoPid = pid;
		list_add(listaProcActivos, nuevoPid);
	}

}

int buscarPaginaLibre() {
	int i;
	for (i = 0; i < marcos; i ++) {
		if ( (tablaPagsInvert[i].pid == -1) && (tablaPagsInvert[i].pag == -1)) {
			//entonces la pagina esta libre
			return i;
		}
	}
	log_trace(logger, "no se encontro ninguna pagina libre");
	//no hay paginas libres para reservar
	return -1;
}

int guardarPagina(int pid, int numPag, char* pagina) {

	int pos = funcionHash(pid, numPag);
	log_trace(logger, "PID: [%d], NUM: [%d], POS: [%d]", pid, numPag, pos);

	int posPagina = verificarPosHash(pid, numPag, pos);
	log_trace(logger, "pos pagina verificada: %d", posPagina);
	if (posPagina != -1) {
		escribirEnBloque(posPagina, pagina);
		log_trace(logger, "Se escribio la pagina correctamente \n");
		return EXIT_SUCCESS;
	} else {
		log_error(logger, "La pagina a escribir no existe");
		return EXIT_FAILURE;
	}
}

int funcionHash(int pid, int numPag) {
	return bloquesAdm; //podria ser mejor
}

int verificarPosHash(int pid, int numPag, int pos) {
	int condicion = 0;
	int posActual = pos;
	log_trace(logger, "se pidio el pid: %d y la pag: %d", pid, numPag);
	while (condicion == 0) {
		if ((tablaPagsInvert[posActual].pid == pid) && (tablaPagsInvert[posActual].pag == numPag)) {
			condicion = 1;
		} else {
			if (posActual < marcos-1) {
				//verifico el siguiente
				posActual++;
			} else {
				//ya barri toda la tabla
				posActual = -1;
				condicion = 1;
			}
		}
	}
	return posActual;
}

void escribirEnBloque(int pos, char* contenido) {
	char *paginaAEscribir = inicioMemoriaProcesos;
	paginaAEscribir = paginaAEscribir + (pos * marco_size);
	pthread_mutex_lock(&mutexMemoriaProcesos);
	memcpy(paginaAEscribir, contenido, marco_size);
	pthread_mutex_unlock(&mutexMemoriaProcesos);
	log_trace(logger, "la pagina que acabo de escribir >%s<", paginaAEscribir);
	log_trace(logger, "ya escribi en el bloque");
}

int liberarPagina(int pid, int numPag) {
	int pos = funcionHash(pid, numPag);
	int posPagina = verificarPosHash(pid, numPag, pos);
	if (posPagina != -1) {
		pthread_mutex_lock(&mutexMemoriaProcesos);
		tablaPagsInvert[posPagina].pid = -1;
		tablaPagsInvert[posPagina].pag = -1;
		pthread_mutex_unlock(&mutexMemoriaProcesos);
		framesOcupados--;
		log_trace(logger, "Pagina liberada correctamente");
		return EXIT_SUCCESS;
	} else {
		log_trace(logger, "La pagina que desea liberar no existe");
		return EXIT_FAILURE;
	}
}

char* consultarPagina(int pid, int numPag) {
	int pos = funcionHash(pid, numPag);
	int posPagina = verificarPosHash(pid, numPag, pos);
	return irABloqueMemoria(posPagina);
}

int finalizarProceso(int pid) {
	if (cacheActivada()){
		eliminarProcesoEnCache(pid);
	}
	eliminarProcesoDeActivos(pid);
	eliminarProcesoEnMemoria(pid);
	return EXIT_SUCCESS;
}

void eliminarProcesoDeActivos(int pid) {
	bool esPidBuscado(int* pidBuscado){ return  (*pidBuscado) == pid ;}

	list_remove_by_condition(listaProcActivos, (void *) esPidBuscado);
}

void eliminarProcesoEnCache(int pid) {
	int i;
	int cantPaginas = 0;
	pthread_mutex_lock(&mutexCache);
	for (i = 0; i < entradas_cache; i++) {
		if (memCache[i].pid == pid) {
			memCache[i].pid = CACHE_LIBRE;
			memCache[i].numPag = CACHE_LIBRE;
			memCache[i].numBloque = 0;
			memCache[i].vecesUsado = 0;
			cantPaginas++;
		}
	}
	pthread_mutex_unlock(&mutexCache);
	if (cantPaginas != 0) {
		log_trace(logger, "Se eliminaron: %d paginas de la cache asociadas al pid: %d", cantPaginas, pid);
		return;
	}
	log_trace(logger, "No habia paginas en la cache asociadas al pid: %d", pid);
}

void eliminarProcesoEnMemoria(int pid) {
	int numPag = 0;
	int condicion = liberarPagina(pid, numPag);
	while (condicion == 0) {
		numPag++;
		condicion = liberarPagina(pid, numPag);
	}
	log_trace(logger, "Proceso con pid: %d eliminado de la memoria Principal", pid);
}

void crearHiloCPU(int sockNuevoCPU) {
	t_elem_hiloCpu *nodo = malloc(sizeof(t_elem_hiloCpu));
	nodo->socket = sockNuevoCPU;
	pthread_create(&(nodo->hilo), NULL, (void *)funcionHiloCPU, nodo->socket);
	list_add(listaCPUs, nodo);
}

void eliminarHiloCPU(int sockCPU) {
	int i;
	int elQueTermino;
	for( i = 0; i < list_size(listaCPUs); i++) {
		t_elem_hiloCpu *aux = list_get(listaCPUs, i);
		if (aux->socket == sockCPU) {
			elQueTermino = i;
			break;
		}
	}
	list_remove(listaCPUs, elQueTermino);
}

void funcionHiloCPU(int sockCPU) {

	t_mensaje *mensaje = com_recibirMensaje(sockCPU, logger);

	while (mensaje != NULL) {

		int pid = com_leerIntAMensaje(mensaje, "pid", logger);
		int numPag = com_leerIntAMensaje(mensaje, "pagina", logger);
		int offset = com_leerIntAMensaje(mensaje, "offset", logger);
		int size = com_leerIntAMensaje(mensaje, "size", logger);
		int esNum = com_leerIntAMensaje(mensaje, "esNum", logger);
		int valor;
		t_mensaje *respuesta;
		char* buffer = NULL;
		uint32_t resultadoDeGuardar;
		int32_t enteroAEnviar;
		char* datosAEscribir;
		log_trace(logger,"EEl mensaje que me llego %d",mensaje->header->tipo);

		switch (mensaje->header->tipo) {
			case CM_PEDIR_DATO_MEMORIA:
				respuesta = com_nuevoMensaje(MC_DATO_RESPUESTA, logger);
				if(esNum) {
					enteroAEnviar = leerDatoInt(pid, numPag, offset, size);

					if(enteroAEnviar == -1){
						com_agregarIntAMensaje(respuesta, "entero", enteroAEnviar, logger);
						com_agregarIntAMensaje(respuesta, "resultadoLectura", 0, logger);
					}else{
						com_agregarIntAMensaje(respuesta, "entero", enteroAEnviar, logger);
						com_agregarIntAMensaje(respuesta, "resultadoLectura", 1, logger);
					}
				} else {
					buffer = leerDatoString(pid, numPag, offset, size);

					if(buffer == NULL){
						com_agregarStringAMensaje(respuesta, "buffer", " ", logger);
						com_agregarIntAMensaje(respuesta, "resultadoLectura", 1, logger);
					} else if(strcmp(buffer, "") == 0 || strcmp(buffer, "\0") == 0) {
						com_agregarStringAMensaje(respuesta, "buffer", " ", logger);
						com_agregarIntAMensaje(respuesta, "resultadoLectura", 1, logger);
						} else {
						com_agregarStringAMensaje(respuesta, "buffer", buffer, logger);
						com_agregarIntAMensaje(respuesta, "resultadoLectura", 1, logger);
					}
					free(buffer);
				}
				com_enviarMensaje(respuesta, sockCPU, logger);
				com_destruirMensaje(mensaje, logger);
				com_destruirMensaje(respuesta, logger);
				break;
			case CM_GUARDAR_EN_MEMORIA:
				log_trace(logger, "El mensaje que me llego: %s", com_imprimirMensaje(mensaje));
				if (esNum) {
					valor = com_leerIntAMensaje(mensaje, "valor", logger);
					resultadoDeGuardar = guardarDatoInt(pid, numPag, offset, size, valor);
				} else {
					datosAEscribir = (char *) com_leerStringAMensaje(mensaje, "texto", logger);
					resultadoDeGuardar = guardarDatoString(pid, numPag, offset, size, datosAEscribir);
				}
				respuesta = com_nuevoMensaje(MC_GUARDAR_RESPUESTA, logger);
				com_agregarIntAMensaje(respuesta, "resultadoDeGuardar", resultadoDeGuardar, logger);
				com_enviarMensaje(respuesta, sockCPU, logger);
				com_destruirMensaje(mensaje, logger);
				com_destruirMensaje(respuesta, logger);
				break;
			default:
				log_trace(logger, "Mensaje Incorrecto");
				break;
		}
		mensaje = com_recibirMensaje(sockCPU, logger);
	}

	log_trace(logger, "Se desconecto la CPU");

	eliminarHiloCPU(sockCPU);
}

void sumarVecesUsada(int pos) {
	pthread_mutex_lock(&mutexCache);
	memCache[pos].vecesUsado++;
	pthread_mutex_unlock(&mutexCache);
}

int estaLaPagEnCache(int pid, int numPag) {
	int i;
	if (cacheActivada()) {
		for (i = 0 ; i < entradas_cache ; i++) {
			if (memCache[i].pid == pid && memCache[i].numPag == numPag) {
				return i;
			}
		}
	}
	return -1;
}

int paginaAReemplazarEnCache() {
	int i;
	int min = memCache[0].vecesUsado;
	int pos = 0;
	for ( i = 1; i < entradas_cache ; i++) {
		if (memCache[i].vecesUsado < min) {
			min = memCache[i].vecesUsado;
			pos = i;
		}
	}
	return pos;
}

int paginaAReemplazarEnCacheDelPid(int pid) {
	int i;
	int pos = 0;
	for ( i = 0; i < entradas_cache ; i++) {
		if (memCache[i].pid == pid) {
			pos = i;
			break;
		}
	}
	return pos;
}

char* irABloqueMemoria(int pos) {
	return &inicioMemoriaProcesos[pos * marco_size];
}

void aplicarRetardo() {
	usleep(1000 * retardoMemoria);
}

void actualizarCache(int pos, int pid, int numPag, int posBloque) {
	pthread_mutex_lock(&mutexCache);
	memCache[pos].pid = pid;
	memCache[pos].numPag = numPag;
	memCache[pos].numBloque = posBloque;
	memCache[pos].vecesUsado = 1;
	pthread_mutex_unlock(&mutexCache);
}

int cantVecesPidEnCache(int pid) {
	int i;
	int cant = 0;
	for (i = 0; i < entradas_cache; i ++) {
		if (memCache[i].pid == pid) {
			cant++;
		}
	}
	return cant;
}

int leerDatoInt(int pid, int numPag, int offset, int size) {

	char *contenido;
	int posHash;
	int posBloque;
	int estaEnCache = estaLaPagEnCache(pid, numPag);
	int paginaAReemplazar;
	int resultado;

	if (size > marco_size) {
		log_error(logger, "No se puede leer mas de marco size \n");
		return -1;
	}

	if (estaEnCache != -1 && cacheActivada()) { //esta en cache
		posBloque = memCache[estaEnCache].numBloque;
		sumarVecesUsada(estaEnCache);
	} else { //no esta en cache, busco en memoria
		aplicarRetardo();
		posHash = funcionHash(pid, numPag);
		posBloque = verificarPosHash(pid, numPag, posHash);
		if (posBloque != -1 && cacheActivada()) {
			if (cantVecesPidEnCache(pid) < entradas_x_proc) {
				//puedo agregar la pagina tranquilo a la cache
				paginaAReemplazar = paginaAReemplazarEnCache();
			} else {
				//hago un reemplazo local del pid porque se llego al max por proc
				paginaAReemplazar = paginaAReemplazarEnCacheDelPid(pid);
			}
			actualizarCache(paginaAReemplazar, pid, numPag, posBloque);
		}
	}

	if (posBloque != -1) {

		if (offset + size < marco_size) {
			log_trace(logger, "el contenido esta todo en la misma pagina");
			//entonces lo que quiero leer esta en la misma pagina
			contenido = irABloqueMemoria(posBloque);
			log_trace(logger,"el contenido >%s<", contenido);
			memcpy(&resultado, &contenido[offset], size);
			log_trace(logger,"el size >%d<", size);
			log_trace(logger,"el offset >%d<", offset);
			log_trace(logger,"el resultado >%d<", resultado);
		}
	} else {
		log_error(logger, "La pagina a la que se intenta acceder no existe \n");
		return -1;
	}

	return resultado;
}

char* leerDatoString(int pid, int numPag, int offset, int size) {

	char *contenido;
	char *buffer;
	buffer = malloc(size+1);
	memset(buffer, '\0', size+1);
	int posHash;
	int posHash2;
	int posBloque;
	int posBloque2;
	int estaEnCache = estaLaPagEnCache(pid, numPag);
	int paginaAReemplazar;

	if (size > marco_size) {
		log_error(logger, "No se puede leer mas de marco size \n");
		free(buffer);
		buffer = NULL;
		return buffer;
	}

	if (estaEnCache != -1 && cacheActivada()) { //esta en cache
		posBloque = memCache[estaEnCache].numBloque;
		sumarVecesUsada(estaEnCache);
	} else { //no esta en cache, busco en memoria
		aplicarRetardo();
		posHash = funcionHash(pid, numPag);
		posBloque = verificarPosHash(pid, numPag, posHash);
		if (posBloque != -1 && cacheActivada()) {
			if (cantVecesPidEnCache(pid) < entradas_x_proc) {
				//puedo agregar la pagina tranquilo a la cache
				paginaAReemplazar = paginaAReemplazarEnCache();
			} else {
				//hago un reemplazo local del pid porque se llego al max por proc
				paginaAReemplazar = paginaAReemplazarEnCacheDelPid(pid);
			}
			actualizarCache(paginaAReemplazar, pid, numPag, posBloque);
		}
	}

	if (posBloque != -1) {

		if (offset + size < marco_size) {
			log_trace(logger, "el contenido esta todo en la misma pagina");
			//entonces lo que quiero leer esta en la misma pagina
			contenido = irABloqueMemoria(posBloque);
			log_trace(logger,"el contenido >%s<", contenido);
			memcpy(buffer, contenido + offset, size);
			log_trace(logger,"el size >%d<", size);
			log_trace(logger,"el offset >%d<", offset);
			log_trace(logger,"el buffer >%s<", buffer);
		} else {
			//tengo que leer una pagina y despues la otra
			int primeraLectura = marco_size - offset;
			int segundaLectura = size - primeraLectura;
			contenido = irABloqueMemoria(posBloque);
			memcpy(buffer, contenido + offset, primeraLectura);
			numPag++; //aumento para leer la proxima pagina
			posHash2 = funcionHash(pid, numPag);
			posBloque2 = verificarPosHash(pid, numPag, posHash2);
			if (posBloque2 != -1) {
				contenido = irABloqueMemoria(posBloque2);
				memcpy(buffer + primeraLectura, contenido, segundaLectura);
			} else {
				log_trace(logger, "Hubo un error al intentar leer la siguiente pagina");
				free(buffer);
				buffer = NULL;
			}
		}
	} else {
		log_error(logger, "La pagina a la que se intenta acceder no existe \n");
		free(buffer);
		buffer = NULL;
	}

	return buffer;
}

int guardarDatoInt(int pid, int numPag, int offset, int size, int valor) {

	int posHash;
	int posBloque;
	int estaEnCache = estaLaPagEnCache(pid, numPag);
	int paginaAReemplazar;
	int resultado;
	char* contenido;

	if (size > marco_size) {
		log_error(logger, "No se puede escribir mas de marco size \n");
		return 1;
	}

	if (estaEnCache != -1 && cacheActivada()) { //esta en cache
		posBloque = memCache[estaEnCache].numBloque;
		sumarVecesUsada(estaEnCache);
	} else { //no esta en cache, busco en memoria
		aplicarRetardo();
		posHash = funcionHash(pid, numPag);
		posBloque = verificarPosHash(pid, numPag, posHash);
		if (posBloque != -1 && cacheActivada()) {
			if (cantVecesPidEnCache(pid) < entradas_x_proc) {
				//puedo agregar la pagina tranquilo a la cache
				paginaAReemplazar = paginaAReemplazarEnCache();
			} else {
				//hago un reemplazo local del pid porque se llego al max por proc
				paginaAReemplazar = paginaAReemplazarEnCacheDelPid(pid);
			}
			actualizarCache(paginaAReemplazar, pid, numPag, posBloque);
		}
	}

	if (posBloque != -1) {
		contenido = irABloqueMemoria(posBloque);
		pthread_mutex_lock(&mutexMemoriaProcesos);
		memcpy(&contenido[offset], &valor, size);
		pthread_mutex_unlock(&mutexMemoriaProcesos);
		resultado = 0;
	} else {
		log_trace(logger, "La pagina a la que se intenta acceder no existe \n");
		resultado = 1;
	}

	return resultado;
}

int guardarDatoString(int pid, int numPag, int offset, int size, char* buffer) {

	int posHash;
	int posHash2;
	int posBloque;
	int posBloque2;
	int estaEnCache = estaLaPagEnCache(pid, numPag);
	int paginaAReemplazar;
	int resultado;
	char* contenido;

	if (size > marco_size) {
		log_error(logger, "No se puede escribir mas de marco size \n");
		return 1;
	}

	if (estaEnCache != -1 && cacheActivada()) { //esta en cache
		posBloque = memCache[estaEnCache].numBloque;
		sumarVecesUsada(estaEnCache);
	} else { //no esta en cache, busco en memoria
		aplicarRetardo();
		posHash = funcionHash(pid, numPag);
		posBloque = verificarPosHash(pid, numPag, posHash);
		if (posBloque != -1 && cacheActivada()) {
			if (cantVecesPidEnCache(pid) < entradas_x_proc) {
				//puedo agregar la pagina tranquilo a la cache
				paginaAReemplazar = paginaAReemplazarEnCache();
			} else {
				//hago un reemplazo local del pid porque se llego al max por proc
				paginaAReemplazar = paginaAReemplazarEnCacheDelPid(pid);
			}
			actualizarCache(paginaAReemplazar, pid, numPag, posBloque);
		}
	}

	if (posBloque != -1) {

		if (offset + size < marco_size) {
			contenido = irABloqueMemoria(posBloque);
			pthread_mutex_lock(&mutexMemoriaProcesos);
			memcpy(contenido + offset, buffer, size);
			pthread_mutex_unlock(&mutexMemoriaProcesos);
			resultado = 0;
		} else {
			//tengo que leer una pagina y despues la otra
			int primeraEscritura = marco_size - offset;
			int segundaEscritura = size - primeraEscritura;
			contenido = irABloqueMemoria(posBloque);
			pthread_mutex_lock(&mutexMemoriaProcesos);
			memcpy(contenido, buffer, primeraEscritura);
			pthread_mutex_unlock(&mutexMemoriaProcesos);
			numPag++; //aumento para leer la proxima pagina
			posHash2 = funcionHash(pid, numPag);
			posBloque2 = verificarPosHash(pid, numPag, posHash2);
			if (posBloque2 != -1) {
				contenido = irABloqueMemoria(posBloque2);
				pthread_mutex_lock(&mutexMemoriaProcesos);
				memcpy(contenido, buffer + primeraEscritura, segundaEscritura);
				pthread_mutex_unlock(&mutexMemoriaProcesos);
			} else {
				log_trace(logger, "Hubo un error al intentar leer la siguiente pagina");
				resultado = 1;
			}
		}
	} else {
		log_trace(logger, "La pagina a la que se intenta acceder no existe \n");
		resultado = 1;
	}

	return resultado;
}

// --------------------------------
// Funciones de Consola Memoria

void consolaMemoria() {
	while(1) {
		homeConsola();
		leerComandos();
	}
}

void homeConsola() {
	printf("------------------------------------\n");
	printf("Bienvenido a la consola de Memoria \n");
	printf("------------------------------------\n");
	printf("Por favor, ingrese una de las siguientes opciones para operar: \n");
	printf("1 - MODIFICAR RETARDO. \n");
	printf("2 - DUMP CACHE. \n");
	printf("3 - DUMP ESTRUCTURA MEMORIA. \n");
	printf("4 - DUMP CONTENIDO MEMORIA. \n");
	printf("5 - FLUSH CACHE. \n");
	printf("6 - SIZE MEMORIA. \n");
	printf("7 - SIZE PID. \n");
	printf("------------------------------------\n");
	printf("Introduzca el numero de opcion y presione ENTER\n\n");
}

void leerComandos() {
	char *comando = detectaIngresoConsola("Ingrese una opcion: ");
	printf("\n");

	if (comando[0]!=' ' && comando[0]!='\0'){

		int opcion = detectarComandoConsola(comando);

		switch(opcion) {
			case RETARDO: {
				log_trace(logger, "ELEGISTE MODIFICAR RETARDO");
				modificarRetardo();
				break;
			}
			case DUMP_CACHE: {
				log_trace(logger, "ELEGISTE DUMP CACHE");
				dumpCache();
				break;
			}
			case DUMP_EST_MEM: {
				log_trace(logger, "ELEGISTE DUMP ESTRUCTURA MEMORIA");
				dumpEstrucMem();
				break;
			}
			case DUMP_CONT_MEM: {
				log_trace(logger, "ELEGISTE DUMP CONTENIDO MEMORIA");
				dumpContMem();
				break;
			}
			case FLUSH: {
				log_trace(logger, "ELEGISTE FLUSH");
				flushCache();
				break;
			}
			case SIZE_MEM: {
				log_trace(logger, "ELEGISTE SIZE MEMORIA");
				sizeMemoria();
				break;
			}
			case SIZE_PID: {
				log_trace(logger, "ELEGISTE SIZE PID");
				sizePID();
				break;
			}
			default: {
				printf("\n\n");
				printf("Comando invalido. \n\n");
				leerComandos();
			}
		}
	}  else{
		leerComandos();
	}

	free(comando);
}

char* detectaIngresoConsola(char* const mensaje) {
	char * entrada = malloc(sizeof(char));

	printf("%s", mensaje);

	char c = fgetc(stdin);
	int i=0;

	while (c!= '\n') {
		entrada[i] = c;
		i++;
		entrada = realloc(entrada, (i+1) * sizeof(char));
		c = fgetc(stdin);
	}

	entrada[i]='\0';

	return entrada;
}

int detectarComandoConsola(char* comando) {
	if(string_equals_ignore_case(comando,"1") == true) return RETARDO;
	if(string_equals_ignore_case(comando,"2") == true) return DUMP_CACHE;
	if(string_equals_ignore_case(comando,"3") == true) return DUMP_EST_MEM;
	if(string_equals_ignore_case(comando,"4") == true) return DUMP_CONT_MEM;
	if(string_equals_ignore_case(comando,"5") == true) return FLUSH;
	if(string_equals_ignore_case(comando,"6") == true) return SIZE_MEM;
	if(string_equals_ignore_case(comando,"7") == true) return SIZE_PID;
	return -1;
}

void modificarRetardo() {
	char * retardo = detectaIngresoConsola("Ingrese el retardo en milisegundos: ");
	printf("\n");

	if (retardo[0]!=' ' && retardo[0]!='\0') {
		retardoMemoria = atoi(retardo);
		printf("El retardo ahora es de: %d ms \n\n", retardoMemoria);
	} else {
		modificarRetardo();
	}

}

void sizeMemoria() {
	 int framesLibres = marcos - framesOcupados;
	 printf("El size de la memoria es de: %d frames \n", marcos);
	 printf("Cantidad de frames ocupados: %d \n", framesOcupados);
	 printf("Cantidad de frames libres: %d \n", framesLibres);
}

void flushCache() {

	int i;
	if (entradas_cache <= 0) {
		printf("[Operacion Cancelada] La memoria cache esta deshabilitada. \n");
		return;
	}

	pthread_mutex_lock(&mutexCache);
	for (i = 0; i < entradas_cache; i++) {
		memCache[i].pid = CACHE_LIBRE;
		memCache[i].numPag = CACHE_LIBRE;
		memCache[i].numBloque = 0;
		memCache[i].vecesUsado = 0;
	}
	pthread_mutex_unlock(&mutexCache);

	printf("La memoria cache se limpio exitosamente. \n");

}

void sizePID() {

	char* comando = detectaIngresoConsola("Ingrese el PID del proceso: ");
	printf("\n");

	if (comando[0]!=' ' && comando[0]!='\0') {
		if (es_entero(comando)) {
			int pid = atoi(comando);
			int size = calcularSizePID(pid);
			if (size > 0) {
				printf("El pid: %d ocupa %d marcos\n\n", pid, size);
			} else {
				printf("El pid: %d no existe en el sistema.\n\n", pid);
			}
		} else {
			printf("Ingresar un numero entero\n\n");
			sizePID();
		}
	}

}

int calcularSizePID(int pid) {
	int i;
	int cantBloquesPID = 0;
	for (i = bloquesAdm; i < marcos; i++) {
		if (tablaPagsInvert[i].pid == pid) {
			cantBloquesPID++;
		}
	}
	return cantBloquesPID;
}

int es_entero(char *cadena) {
	int i, valor;

	for(i=0; i < string_length(cadena); i++) {
		valor = cadena[ i ] - '0';
		if(valor < 0 || valor > 9) {
			if(i==0 && valor==-3) continue;
			return 0;
		}
	}
	return 1;
}

void dumpCache() {
	printf("\n========== [Inicio] Datos almacenados en la memoria Cache ========== \n");
	char* fileName = string_new();
	string_append(&fileName, "DumpContenido_Cache_");
	FILE *fp;
	char* mensaje = string_new();
	char* titulo = string_new();
	time_t tiempo = time(0);
	struct tm *tlocal = localtime(&tiempo);
	char output[128];
	strftime(output, 128, "%Y%m%d_%H%M%S", tlocal);
	string_append_with_format(&fileName, "%s", output);
	string_append(&fileName, ".txt");
	fp = fopen(fileName, "w");
	int pid;
	int numPag;
	int numBloque;
	int vecesUsado;
	int i;
	string_append_with_format(&titulo, "Cantidad de entradas: %d\n", entradas_cache);
	fputs(titulo, fp);
	free(fileName);
	free(titulo);
	printf("Cantidad de entradas: %d\n", entradas_cache);
	for (i = 0; i < entradas_cache; i++) {
		pid = memCache[i].pid;
		numPag = memCache[i].numPag;
		numBloque = memCache[i].numBloque;
		vecesUsado = memCache[i].vecesUsado;
		mensaje = string_new();
		string_append_with_format(&mensaje, "pid: %d \t Num pagina: %d \t Marco en Memoria: %d \t Timestamp: %d \n",
				pid, numPag, numBloque, vecesUsado);
		fputs(mensaje, fp);
		printf("pid: %d \t", pid);
		printf("Num pagina: %d \t", numPag);
		printf("Marco en Memoria: %d \t", numBloque);
		printf("Timestamp: %d \n", vecesUsado);
		free(mensaje);
	}
	fclose(fp);
	printf("\n Archivo Guardado en disco\n");
	printf("\n========== [Fin] Datos almacenados en la memoria Cache ========== \n");

}

void dumpEstrucMem() {
	char* fileName = string_new();
	string_append(&fileName, "DumpEstruc_Mem_");
	FILE *fp;
	time_t tiempo = time(0);
	struct tm *tlocal = localtime(&tiempo);
	char output[128];
	strftime(output, 128, "%Y%m%d_%H%M%S", tlocal);
	string_append_with_format(&fileName, "%s", output);
	string_append(&fileName, ".txt");
	fp = fopen(fileName, "w");
	free(fileName);
	imprimirListaDeProcActivos(fp);
	imprimirTablaDePaginas(fp);
	fclose(fp);
}

void imprimirListaDeProcActivos(FILE* fp) {
	char* titulo = string_new();
	char* mensaje;
	string_append_with_format(&titulo, "Lista de procesos activos \n");
	fputs(titulo, fp);
	free(titulo);
	void imprimirElem(int* pid) {
		mensaje = string_new();
		string_append_with_format(&mensaje, "pid: %d \n", *pid);
		fputs(mensaje,fp);
		free(mensaje);
		printf("pid: %d\n", *pid);
	}
	printf("\n========== [Inicio] Lista de procesos activos ========== \n");
	list_iterate(listaProcActivos, (void *) imprimirElem);
	printf("\n========== [Fin] Lista de procesos activos ========== \n");
}

void imprimirTablaDePaginas(FILE* fp) {
	fputs("\n", fp);
	char* titulo = string_new();
	char* mensaje;
	string_append_with_format(&titulo, "Contenido Tabla de paginas \n");
	fputs(titulo, fp);
	free(titulo);
	int i;
	printf("\n");
	printf("\n========== [Inicio] Contenido Tabla de paginas ========== \n");
	for (i = 0; i < marcos; i++) {
		mensaje = string_new();
		string_append_with_format(&mensaje, "pid: %d \t Num pagina: %d \t Marco en Memoria: %d \n",
				tablaPagsInvert[i].pid, tablaPagsInvert[i].pag, tablaPagsInvert[i].frame);
		fputs(mensaje, fp);
		free(mensaje);
		printf("pid: %d\t", tablaPagsInvert[i].pid);
		printf("Num Pagina: %d\t", tablaPagsInvert[i].pag);
		printf("Marco en memoria: %d\n", tablaPagsInvert[i].frame);
	}
	printf("\n========== [Fin] Contenido Tabla de paginas ========== \n");
}

void dumpContMem() {
	char* fileName = string_new();
	string_append(&fileName, "DumpCont_Mem_");
	FILE *fp;
	time_t tiempo = time(0);
	struct tm *tlocal = localtime(&tiempo);
	char output[128];
	strftime(output, 128, "%Y%m%d_%H%M%S", tlocal);
	string_append_with_format(&fileName, "%s", output);
	string_append(&fileName, ".txt");
	fp = fopen(fileName, "w");
	free(fileName);
	char * comando = detectaIngresoConsola("Ingrese P(Proceso) o T(Todos): ");

	if (comando[0]!=' ' && comando[0]!='\0') {
		if (string_equals_ignore_case(comando,"P")) {
			dumpEstructurasProceso(fp);
		} else if(string_equals_ignore_case(comando,"T")) {
			dumpEstructurasTodos(fp);
		} else {
			dumpContMem();
		}
	}else{
		dumpContMem();
	}
	fclose(fp);
}

void dumpEstructurasTodos(FILE* fp) {
	printf("\n========== [Inicio] Contenido todos los procesos ========== \n");
	fputs("========== [Inicio] Contenido todos los procesos ========== \n\n", fp);
	int j;
	for (j = 0; j < marcos; j++) {
		if (tablaPagsInvert[j].pid != -1 && tablaPagsInvert[j].pid != -2) {
			char* titulo = string_new();
			string_append_with_format(&titulo, "pid: %d numPag: %d\n", tablaPagsInvert[j].pid, tablaPagsInvert[j].pag);
			fputs(titulo, fp);
			free(titulo);
			printf("pid: %d numPag: %d\n", tablaPagsInvert[j].pid, tablaPagsInvert[j].pag);
			int pos = tablaPagsInvert[j].frame;
			char* subtitulo = string_new();
			string_append_with_format(&subtitulo, "Bloque: %d\n", pos);
			fputs(subtitulo,fp);
			free(subtitulo);
			printf("Bloque: %d\n", pos);
			printf("Contenido: ");
			fputs("Contenido: ",fp);
			char *bloqueAMostrar = inicioMemoriaProcesos;
			bloqueAMostrar = bloqueAMostrar + (pos * marco_size);
			int i;
			char *mensaje = string_new();
			for (i = 0 ; i < marco_size; i++) {
				string_append_with_format(&mensaje, "%c", bloqueAMostrar[i]);
				printf("%c", bloqueAMostrar[i]);
			}
			fputs(mensaje, fp);
			free(mensaje);
			fputs("\n\n", fp);
			printf("\n\n");
		}
	}
	printf("\n========== [Fin] Contenido todos los procesos ========== \n\n");
}

void dumpEstructurasProceso(FILE *fp) {
	char * comando = detectaIngresoConsola("Ingrese el PID del Proceso: ");

	printf("\n========== [Inicio] Contenido proceso ========== \n");
	fputs("========== [Inicio] Contenido proceso ========== \n\n", fp);

	if (comando[0]!=' ' && comando[0]!='\0'){
		if(es_entero(comando)) {
			int j;
			for (j = 0; j < marcos; j++) {
				if (tablaPagsInvert[j].pid == atoi(comando)) {
					char* titulo = string_new();
					string_append_with_format(&titulo, "pid: %d numPag: %d\n", atoi(comando), tablaPagsInvert[j].pag);
					fputs(titulo, fp);
					free(titulo);
					printf("pid: %d numPag: %d\n", atoi(comando), tablaPagsInvert[j].pag);
					int pos = tablaPagsInvert[j].frame;
					char* subtitulo = string_new();
					string_append_with_format(&subtitulo, "Bloque: %d\n", pos);
					fputs(subtitulo,fp);
					free(subtitulo);
					printf("Bloque: %d\n", pos);
					printf("Contenido: ");
					fputs("Contenido: ",fp);
					char *bloqueAMostrar = inicioMemoriaProcesos;
					bloqueAMostrar = bloqueAMostrar + (pos * marco_size);
					int i;
					char *mensaje = string_new();
					for (i = 0 ; i < marco_size; i++) {
						string_append_with_format(&mensaje, "%c", bloqueAMostrar[i]);
						printf("%c", bloqueAMostrar[i]);
					}
					fputs(mensaje, fp);
					free(mensaje);
					fputs("\n\n", fp);
					printf("\n\n");
				}
			}
		} else {
			dumpEstructurasProceso(fp);
		}
	}else{
		dumpEstructurasProceso(fp);
	}

	printf("\n========== [Fin] Contenido proceso ========== \n");
}
