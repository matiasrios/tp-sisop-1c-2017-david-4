/*
 * virtualmemo.c
 *
 *  Created on: 21/6/2017
 *      Author: utnso
 */
#include "heapmemo.h"
int tamPage = 0;
//#define TAM_PAGE 512

//char* pagina;
char* _crearPagina();
char* _compactar(char* pagina);
t_bloque_heap* _crearBloque();
char * pedirPaginaAMemoria(int pid, int numPag, int sockMemoria);
int guardarPaginaEnMemoria(int pid, int paginasPedidas, int sockMemoria,
		char* buffer, t_log* logger, uint32_t tamanioPagina);

int _getTamanioBloque();
t_bloque_heap * _getHeapBlock(char* pagina, int inicio);
char* _setHeapBlock(char* pagina, t_bloque_heap* bloque, int inicio);
t_bloque_heap * _obtenerBloqueLibre(char* pagina, int espacio);
t_bloque_heap * _siguienteBloque(char* pagina, int puntero);
char* _liberarEspacio(char* pagina, int puntero);
int _reservarEspacioPagina(int pid, int numPage, int sockMemoria,
		char* miPagina, int cantidad);
int _pedirPaginaParaProceso(t_pcb *pcb, int sockMemoria);
void _guardarPaginaProceso(t_pcb *pcb, int sockMemoria, char *buffer, int size);

//DEBUG
void _mostrarBloque(t_bloque_heap * bloque);
t_log* logger;

void _mostrarBloque(t_bloque_heap * bloque) {
	log_trace(logger, "BLOQUE::USED=%s SIZE=%d PUNTERO=%d\n",
			bloque->isUsed == 1 ? "true" : "false", bloque->size,
			bloque->puntero);
}

char* _liberarEspacio(char* pagina, int puntero) {
	log_trace(logger, "\n\n Liberar espacio: ");
	t_bloque_heap * bloqALiberar;
	int offset = puntero - _getTamanioBloque();
	log_trace(logger, "Tamaño del bloque: %d", _getTamanioBloque());
	log_trace(logger, "El offset es: %d", offset);
	bloqALiberar = _getHeapBlock(pagina, offset);
	log_trace(logger, "LEIDO A LIBERAR: %c, size: %d, puntero: %d",
			bloqALiberar->isUsed, bloqALiberar->size, bloqALiberar->puntero);
	log_trace(logger, "El puntero es: %d", puntero);
	bloqALiberar->isUsed = false;
	pagina = _setHeapBlock(pagina, bloqALiberar, offset);
	log_trace(logger, "Se liberaron %d bytes de la pagina", bloqALiberar->size);

	log_trace(logger, "VOY A COMPACTAR");
	pagina=_compactar(pagina);
	return pagina;
}

char* _compactar(char* pagina){
	t_bloque_heap *bloqueAux = NULL;
	t_bloque_heap *sigBloque;
	int puntero = 0;
	int punteroAux =0;
	while (puntero < tamPage - _getTamanioBloque()) {
		log_trace(logger, "PUNTERO :%d PUNTEROAUX: %d", puntero, punteroAux);
		sigBloque = _siguienteBloque(pagina, puntero);
		if(sigBloque->isUsed==false) {
			if(bloqueAux == NULL){
				bloqueAux = sigBloque;
				punteroAux = puntero;
			}else{
				_mostrarBloque(bloqueAux);
				_mostrarBloque(sigBloque);
				bloqueAux->size = bloqueAux->size + sigBloque->size +_getTamanioBloque();
				log_trace(logger, ">>>>>>>>>PUNTERO :%d PUNTEROAUX: %d", puntero, punteroAux);
				t_bloque_heap *blockToSave = _crearBloque();
				memcpy(blockToSave,bloqueAux,_getTamanioBloque());
				pagina=_setHeapBlock(pagina,blockToSave,punteroAux);
				log_trace(logger, "COMPACTACION :%d ", bloqueAux->size);
			}
		}else{
			bloqueAux = NULL;
			punteroAux = 0;
		}
		puntero = puntero + _getTamanioBloque() + sigBloque->size;
	}
	return pagina;
}

int _espacioLibrePagina(char *pagina) {
	char* paginaAux = pagina;
	t_bloque_heap * bloqueAux;
	int espacioLibre = 0;
	int cont = 0;
	while (espacioLibre < tamPage - _getTamanioBloque()) {
		bloqueAux = _obtenerBloqueLibre(paginaAux, 50000);
		if (bloqueAux == NULL) {
			log_trace(logger,
					"Espacio libre encontrado en la pagina %d bytes cant bloques libres: %d",
					espacioLibre, cont);
			return espacioLibre;
		}
		espacioLibre = espacioLibre + bloqueAux->size;
		paginaAux = (paginaAux + bloqueAux->puntero) - _getTamanioBloque();
		cont = cont + 1;
	}
	free(bloqueAux);
	log_trace(logger,
			"Espacio libre encontrado en la pagina %d bytes cant bloques libres: %d",
			espacioLibre, cont);
	return espacioLibre;
}

int _reservarEspacioPagina(int pid, int numPage, int sockMemoria,
		char* miPagina, int cantidad) {
	int punteroAux = 0;
	int espacioLibreAux = 0;
	log_trace(logger, "\n++RESERVA  Bloque libre++ de %d bytes", cantidad);
	t_bloque_heap* bloqueLibre = _obtenerBloqueLibre(miPagina, cantidad);
	if (cantidad > _getTamanioMaximo()) {
		log_error(logger,
				"No se pudo reservar: espacio MAXIMO de pagina %d no se puede almacenar %d bytes",
				_getTamanioMaximo(), cantidad);
		return -1;
	}

	if (bloqueLibre == NULL) {
		log_trace(logger,
				"No se pudo reservar no se encuentra bloque libre para almacenar %d bytes",
				cantidad);
		return -2;
	}

	if (bloqueLibre->size - _getTamanioBloque() < cantidad) {
		log_trace(logger,
				"No se pudo reservar: espacio libre %d no se puede almacenar %d bytes",
				bloqueLibre->size - _getTamanioBloque(), cantidad);
		return -2;
	}

	log_trace(logger, "Bloque libre!! :");
	_mostrarBloque(bloqueLibre);

	punteroAux = bloqueLibre->puntero;
	espacioLibreAux = bloqueLibre->size;
	bloqueLibre->isUsed = true;
	bloqueLibre->size = cantidad;
	log_trace(logger, "PEDIDO: %c, size: %d, puntero: %d", bloqueLibre->isUsed,
			bloqueLibre->size, bloqueLibre->puntero);

	punteroAux = punteroAux - _getTamanioBloque();
	miPagina = _setHeapBlock(miPagina, bloqueLibre, punteroAux);

	bloqueLibre = _crearBloque();
	punteroAux = punteroAux + _getTamanioBloque() + cantidad;
	bloqueLibre->isUsed = false;
	bloqueLibre->size = espacioLibreAux - (_getTamanioBloque() + cantidad);

	miPagina = _setHeapBlock(miPagina, bloqueLibre, punteroAux);
	log_trace(logger,
			"Se reservo exitosamente %d bytes de la pagina bloque libre: %d",
			cantidad, bloqueLibre->size);
	log_trace(logger, "Puntero final %d de la pagina", punteroAux - cantidad);

	//enviarMensajeAMemoria(pid, numPage, sockMemoria, logger);

	return punteroAux - cantidad;
}

int enviarMensajeAMemoria(int pid, int numPage, int sockMemoria, t_log *logger) {
	t_mensaje *mensajeAMemoria = com_nuevoMensaje(KM_RESERVAR_PAGINA, logger);
	com_agregarIntAMensaje(mensajeAMemoria, "pid", pid, logger);
	com_agregarIntAMensaje(mensajeAMemoria, "numPag", numPage, logger);
	com_enviarMensaje(mensajeAMemoria, sockMemoria, logger);
	com_destruirMensaje(mensajeAMemoria, logger);
	//Recibir respuesta de Memoria

	t_mensaje *respuestaReserva = com_recibirMensaje(sockMemoria, logger);
	if (respuestaReserva->header->tipo != MK_RESERVA_PAGINA_RESPUESTA) {
		log_error(logger, "reserva de pagina, mensaje no esperado \n");
		return 0;
	}
	int32_t resultadoReserva = com_leerIntAMensaje(respuestaReserva,
			"resultadoReserva", logger);
	int32_t size = com_leerIntAMensaje(respuestaReserva, "size", logger);
	if (resultadoReserva != EXIT_SUCCESS) {
		log_error(logger, "error al reservar pagina \n");
		return 0;
	}
	//TODO meter datos nuevo en el PCB
	log_trace(logger, "pagina %d reservada para %d", numPage, pid);

	com_destruirMensaje(respuestaReserva, logger);
	return size;

}
t_bloque_heap * _obtenerBloqueLibre(char* pagina, int espacio) {
	t_bloque_heap *bloqueAux;
	t_bloque_heap *sigBloque;
	int puntero = 0;

	while (puntero < tamPage - _getTamanioBloque()) {
		sigBloque = _siguienteBloque(pagina, puntero);

		_mostrarBloque(sigBloque);
		if (_isBloqueFreeAndAvailable(sigBloque, espacio)) {
			return sigBloque;
		}

		puntero = puntero + _getTamanioBloque() + sigBloque->size;
		if (tamPage - puntero < espacio) {
			return NULL;
		}
		bloqueAux = sigBloque;
	}
	return NULL;
}


int _isBloqueFreeAndAvailable(t_bloque_heap *bloque, int espacio) {
	if (bloque->isUsed == 0 && bloque->size > espacio + _getTamanioBloque())
		return 1;
	return 0;
}
int _getTamanioMaximo() {
	return tamPage - (2 * _getTamanioBloque());
}

int _getTamanioBloque() {
	return sizeof(_Bool) + sizeof(uint32_t);
}

t_bloque_heap * _siguienteBloque(char* pagina, int puntero) {
	return _getHeapBlock(pagina, puntero);
}

char* _crearPagina() {
	t_bloque_heap* bloqueInicial;
	log_trace(logger, "Creo una pagina de %d bytes espacio maximo %d", tamPage,
			tamPage - (2 * _getTamanioBloque()));
	char *pagina = (char *) malloc(tamPage * sizeof(char));
	memset(pagina, '\0', tamPage * sizeof(char));
	bloqueInicial = _crearBloque();
	bloqueInicial->isUsed = false;
	bloqueInicial->size = tamPage - _getTamanioBloque();
	bloqueInicial->puntero = 0;
	pagina = _setHeapBlock(pagina, bloqueInicial, 0);
	return pagina;
}

t_bloque_heap* _crearBloque() {
	t_bloque_heap* bloque = malloc(sizeof(t_bloque_heap));
	if (bloque == NULL)
		log_error(logger,"Falla creacion bloque");
	return bloque;
}

t_bloque_heap * _getHeapBlock(char* pagina, int inicio) {
	t_bloque_heap *bloque = malloc(sizeof(t_bloque_heap));
	char* paginaAux = pagina;
	paginaAux = paginaAux + inicio;
	memcpy(&(bloque->isUsed), paginaAux, sizeof(_Bool));
	paginaAux = paginaAux + sizeof(_Bool);
	memcpy(&(bloque->size), paginaAux, sizeof(uint32_t));
	bloque->puntero = inicio + _getTamanioBloque();

	return bloque;
}

char* _setHeapBlock(char* pagina, t_bloque_heap* bloque, int inicio) {
	if (inicio < 0) {
		log_error(logger, "Error de asignacion puntero < a 0");
		return NULL;
	}
	_mostrarBloque(bloque);
	char *paginaAux = pagina + inicio;
	memcpy(paginaAux, &(bloque->isUsed), sizeof(_Bool));
	paginaAux = paginaAux + sizeof(_Bool);
	memcpy(paginaAux, &(bloque->size), sizeof(uint32_t));
	free(bloque);
	return pagina;
}

void _enviarMensajeACPU(int resultado, int posicion, int sockCPU) {
	t_mensaje* respuestaACPU;
	if (resultado == 0){
		respuestaACPU = com_nuevoMensaje(KC_FINALIZO_PROGRAMA, logger);
	}
	else{
		respuestaACPU = com_nuevoMensaje(KC_ALOCAR_RESPUESTA, logger);
	}
	com_agregarIntAMensaje(respuestaACPU, "resultado_respuesta", resultado, logger);
	com_agregarIntAMensaje(respuestaACPU, "posicion", posicion, logger);
	com_enviarMensaje(respuestaACPU, sockCPU, logger);
	com_destruirMensaje(respuestaACPU, logger);
}

int reservarMemoriaHeap(t_cpu* cpu, t_pcb* pcbCPU, t_mensaje *mensajeDeCPU,
		int sockMemoria, t_log* logger, uint32_t tamanioPagina) {
	tamPage = tamanioPagina;
	int pid = pcbCPU->pid;
	int espacio = com_leerIntAMensaje(mensajeDeCPU, "espacio", logger);

	log_trace(logger, "voy a reservar <%d> bytes para el proceso <%d>", espacio,
			pid);

	//VirtualMemo
	char * paginaAReservar;
	/*if (tamPage == 0)
	 tamPage = pedirTamanioAMemoria(pid, pcbCPU->paginasPedidas,
	 sockMemoria);*/
	int posicion;
	int paginaHeap = pcbCPU->primerPaginaHeap;

	if (pcbCPU->cantPaginasHeap == 0) {
		if (_pedirPaginaParaProceso(pcbCPU, sockMemoria) > 0) {
			paginaAReservar = malloc(tamPage);
			memset(paginaAReservar, '\0', tamPage);
			paginaAReservar = _crearPagina();
		} else {
			log_error(logger, "ERROR al pedir pagina para proceso");

			return -1;
		}
		//TODO talvez se pueda validar con los punteros que me dicen
		if (_isPaginaValida(paginaAReservar) == EXIT_FAILURE) {
			paginaAReservar = _crearPagina();
		}
		pcbCPU->primerPaginaHeap = pcbCPU->paginasPedidas;
		paginaHeap = pcbCPU->primerPaginaHeap;
		pcbCPU->cantPaginasHeap++;
		pcbCPU->paginasPedidas++;
	} else {

		paginaAReservar = pedirPaginaAMemoria(pid, pcbCPU->primerPaginaHeap,
				sockMemoria);
	}

	posicion = _reservarEspacioPagina(pid, paginaHeap, sockMemoria,
			paginaAReservar, espacio);
	while (posicion < 0) {
		if (posicion == -1) {
			log_error(logger,
					"ERROR el tamaño pedido supera el maximo permitido");
			_enviarMensajeACPU(0, 0, cpu->sock);
			return -1;
		}
		paginaHeap++;
		if (paginaHeap < pcbCPU->primerPaginaHeap + pcbCPU->cantPaginasHeap) {
			paginaAReservar = pedirPaginaAMemoria(pid, paginaHeap, sockMemoria);
		} else {
			if (_pedirPaginaParaProceso(pcbCPU, sockMemoria) > 0) {
				paginaAReservar = malloc(tamPage);
				memset(paginaAReservar, '\0', tamPage);
				paginaAReservar = _crearPagina();
			} else {
				log_error(logger, "ERROR al pedir pagina para proceso");
				_enviarMensajeACPU(0, 0, cpu->sock);
				return -1;
			}
			//TODO talvez se pueda validar con los punteros que me dicen
			if (_isPaginaValida(paginaAReservar) == EXIT_FAILURE) {
				paginaAReservar = _crearPagina();
			}
			pcbCPU->cantPaginasHeap++;
			pcbCPU->paginasPedidas++;
		}

		posicion = _reservarEspacioPagina(pid, paginaHeap, sockMemoria,
				paginaAReservar, espacio);
	}

	guardarPaginaEnMemoria(pid, paginaHeap, sockMemoria, paginaAReservar,
			logger, tamanioPagina);

	log_trace(logger,
			">>>>>>>>>>>>>>> paginaHeap: %d, tamPag: %d, posicion: %d, cuenta: %d",
			paginaHeap, tamPage, posicion, (paginaHeap * tamPage) + posicion);

	_enviarMensajeACPU(1, (paginaHeap * tamPage) + posicion, cpu->sock);

	return 0;
}

void* liberarMemoriaHeap(t_cpu* cpu, t_pcb* pcbCPU, t_mensaje *mensajeDeCPU,
		int sockMemoria, t_log* logger) {

	int pid = com_leerIntAMensaje(mensajeDeCPU, "pid", logger);
	int posicion = com_leerIntAMensaje(mensajeDeCPU, "puntero", logger);
	int paginaCalculada = posicion / tamPage;
	int ptr = posicion - (tamPage * paginaCalculada);

	log_trace(logger, "***INICIO LIBERACION POSTA!*****");
	log_trace(logger,
			"Mensaje de CPU a KERNEL pid: %d, puntero: %d, paginaCalc: %d ",
			pid, posicion, paginaCalculada);

	log_trace(logger, "Pase a LIBERAR memmoria, para el pid %d, posicion%d ",
			pid, ptr);

	//VirtualMemo
	char * paginaVirtual;
	paginaVirtual = pedirPaginaAMemoria(pid, paginaCalculada, sockMemoria);
	paginaVirtual = _liberarEspacio(paginaVirtual, ptr);
	int response = guardarPaginaEnMemoria(pid, paginaCalculada, sockMemoria,
			paginaVirtual, logger, tamPage);

	t_mensaje* respuesta = com_nuevoMensaje(KC_LIBERAR_RESPUESTA, logger);
	com_agregarIntAMensaje(respuesta, "resultado_respuesta", response, logger);

	com_enviarMensaje(respuesta, cpu->sock, logger);
	com_destruirMensaje(respuesta, logger);

	return 0;
}

int _pedirPaginaParaProceso(t_pcb *pcb, int sockMemoria) {
	t_mensaje *mensaje = com_nuevoMensaje(KM_RESERVAR_PAGINA, logger);
	com_agregarIntAMensaje(mensaje, "pid", pcb->pid, logger);
	com_agregarIntAMensaje(mensaje, "numPag", pcb->paginasPedidas, logger);
	com_enviarMensaje(mensaje, sockMemoria, logger);
	com_destruirMensaje(mensaje, logger);
	t_mensaje *respuestaReserva = com_recibirMensaje(sockMemoria, logger);
	if (respuestaReserva->header->tipo != MK_RESERVA_PAGINA_RESPUESTA) {
		log_error(logger, "reserva de pagina, mensaje no esperado \n");
		return 0;
	}
	int32_t resultadoReserva = com_leerIntAMensaje(respuestaReserva,
			"resultadoReserva", logger);
	int32_t size = com_leerIntAMensaje(respuestaReserva, "size", logger);
	if (resultadoReserva != EXIT_SUCCESS) {
		log_error(logger, "error al reservar pagina \n");
		return 0;
	}
	log_trace(logger, "pagina %d reservada para %d", pcb->paginasPedidas,
			pcb->pid);
	com_destruirMensaje(respuestaReserva, logger);
	return size;
}

void _guardarPaginaProceso(t_pcb *pcb, int sockMemoria, char *buffer, int size) {
	t_mensaje *mensajePagina = com_nuevoMensaje(KM_GUARDAR_PAGINA, logger);
	com_agregarIntAMensaje(mensajePagina, "pid", pcb->pid, logger);
	com_agregarIntAMensaje(mensajePagina, "numPag", pcb->paginasPedidas,
			logger);
	com_enviarMensaje(mensajePagina, sockMemoria, logger);
	com_destruirMensaje(mensajePagina, logger);

	// TODO falta controlar si el script ocupa mas de una pagina!!!!!!!!!!!!!
	t_mensaje *pagina = com_nuevoMensaje(KM_LA_PAGINA, logger);

	com_cargarPaginaAMensaje(pagina, buffer, size, logger);
	com_enviarMensaje(pagina, sockMemoria, logger);
	com_destruirMensaje(pagina, logger);

	t_mensaje *respuestaDeGuardar = com_recibirMensaje(sockMemoria, logger);
	if (respuestaDeGuardar->header->tipo != MK_GUARDAR_PAGINA_RESPUESTA) {
		log_error(logger, "no se pudo guardar la pagina");
		return;
	}
	log_trace(logger, "se guardo la pagina correctamente");
}

int pedirTamanioAMemoria(int pid, int numPag, int sockMemoria) {
	int resultado = 0;
	int tamanio = -1;

	t_mensaje * mensajeAMemoReserva = com_nuevoMensaje(KM_RESERVAR_PAGINA,
			logger);
	com_agregarIntAMensaje(mensajeAMemoReserva, "pid", pid, logger);
	com_agregarIntAMensaje(mensajeAMemoReserva, "numPage", numPag, logger);
	com_enviarMensaje(mensajeAMemoReserva, sockMemoria, logger);
	com_destruirMensaje(mensajeAMemoReserva, logger);

	// espero respuesta
	t_mensaje * mensajeDeReservaMemoria = com_recibirMensaje(sockMemoria,
			logger);
	switch (mensajeDeReservaMemoria->header->tipo) {
	case MK_RESERVA_PAGINA_RESPUESTA:
		resultado = com_leerIntAMensaje(mensajeDeReservaMemoria,
				"resultadoReserva", logger);
		if (resultado == EXIT_FAILURE)
			return -1;
		tamanio = com_leerIntAMensaje(mensajeDeReservaMemoria, "size", logger);

		break;
	default:
		break;
	}
	return tamanio;
}
char * pedirPaginaAMemoria(int pid, int numPag, int sockMemoria) {
	int resultado = 0;
	int tamanio = 0;
	char * pagina;
	log_trace(logger, "Pase a pedir a memmoria, para el pid %d, num pag %d ",
			pid, numPag);

	t_mensaje * mensajeAMemoria = com_nuevoMensaje(KM_CONSULTAR_PAGINA, logger);
	com_agregarIntAMensaje(mensajeAMemoria, "pid", pid, logger);
	com_agregarIntAMensaje(mensajeAMemoria, "numPag", numPag, logger);
	com_enviarMensaje(mensajeAMemoria, sockMemoria, logger);
	com_destruirMensaje(mensajeAMemoria, logger);

	// Esperar respuesta

	t_mensaje * mensajeDeMemoria = com_recibirMensaje(sockMemoria, logger);
	t_mensaje * laPagina;
	int res;
	switch (mensajeDeMemoria->header->tipo) {
	case MK_CONSULTAR_PAGINA_RESPUESTA:
		res = com_leerIntAMensaje(mensajeDeMemoria, "resultadoConsulta",
				logger);
		// res deberia ser 1
		laPagina = com_recibirMensaje(sockMemoria, logger);
		//if( chequear que sea KM_LA_PAGINA)
		pagina = com_leerPaginaDeMensaje(laPagina, logger);

		break;
	default:
		break;
	}
	return pagina;
}

int _isPaginaValida(char *pagina) {
	t_bloque_heap *bloque = _getHeapBlock(pagina, 0);

	if (bloque->isUsed >= 2 || bloque->size >= tamPage) {
		log_trace(logger, "La pagina qe se quizo consultar NO es valida");
		return EXIT_FAILURE;
	}
	log_trace(logger, "La pagina que se quizo consultar VALIDA");
	return EXIT_SUCCESS;
}

int guardarPaginaEnMemoria(int pid, int paginasPedidas, int sockMemoria,
		char* buffer, t_log* logger, uint32_t tamanioPagina) {
	t_mensaje *mensajePagina = com_nuevoMensaje(KM_GUARDAR_PAGINA, logger);
	com_agregarIntAMensaje(mensajePagina, "pid", pid, logger);
	com_agregarIntAMensaje(mensajePagina, "numPag", paginasPedidas, logger);
	com_enviarMensaje(mensajePagina, sockMemoria, logger);
	com_destruirMensaje(mensajePagina, logger);

	// TODO falta controlar si el script ocupa mas de una pagina!!!!!!!!!!!!!
	t_mensaje *pagina = com_nuevoMensaje(KM_LA_PAGINA, logger);

	com_cargarPaginaAMensaje(pagina, buffer, tamanioPagina, logger); // strlen(buffer)
	com_enviarMensaje(pagina, sockMemoria, logger);
	com_destruirMensaje(pagina, logger);

	t_mensaje *respuestaDeGuardar = com_recibirMensaje(sockMemoria, logger);
	if (respuestaDeGuardar->header->tipo != MK_GUARDAR_PAGINA_RESPUESTA) {
		log_error(logger, "no se pudo guardar la pagina");
		return -1;
	}
	log_trace(logger, "se guardo la pagina correctamente");
	return 1;
}

