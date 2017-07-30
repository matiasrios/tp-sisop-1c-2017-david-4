/*
 * cpu.c
 *
 *  Created on: 1/4/2017
 *      Author: utnso
 */
#include "cpu.h"

AnSISOP_funciones funciones = {

	.AnSISOP_definirVariable = definirVariable,
	.AnSISOP_obtenerPosicionVariable = obtenerPosicionVariable,
	.AnSISOP_dereferenciar = dereferenciar,
	.AnSISOP_asignar = asignar,
	.AnSISOP_irAlLabel = irAlLabel,
	.AnSISOP_finalizar = finalizar,
	.AnSISOP_obtenerValorCompartida = obtenerValorCompartida,
	.AnSISOP_asignarValorCompartida = asignarValorCompartida,
	.AnSISOP_llamarConRetorno =	llamarConRetorno,
	.AnSISOP_llamarSinRetorno = llamarSinRetorno,
	.AnSISOP_retornar = retornar,
};

AnSISOP_kernel kernel = {

	.AnSISOP_reservar = alocar,
	.AnSISOP_liberar = liberar,
	.AnSISOP_abrir = abrir,
	.AnSISOP_borrar = borrar,
	.AnSISOP_cerrar = cerrar,
	.AnSISOP_moverCursor = moverCursor,
	.AnSISOP_escribir =	escribir,
	.AnSISOP_leer = leer,
	.AnSISOP_wait = wait,
	.AnSISOP_signal = signalUP,
};

int main(int argc, char **argv) {

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	signal(SIGUSR1, capturarSigusr1);

	saludar();

	crearLogger();

	t_config *config;

	if (argc != 2) {
		log_error(logger, "Error en la cantidad de parametros: %d, (cpu arch.cfg)", argc);
	}

	config = config_create(argv[1]);

	if (config == NULL) {
		log_error(logger, "Error al abrir el archivo de configuracion: %s, (cpu arch.cfg)", argv[1]);
	}

	sockKernel = com_conectarseA(config_get_string_value(config, "IP_KERNEL"), config_get_int_value(config, "PUERTO_KERNEL"));
	if (sockKernel <= 0) {
		log_error(logger, "Error al conectarse al kernel IP: %s PUERTO: %d", config_get_string_value(config, "IP_KERNEL"), config_get_int_value(config, "PUERTO_KERNEL"));
	}

	if (!com_cpuker_handshake_cpu(sockKernel, logger)) {
		log_error(logger, "Error en el handshake con el kernel");
	}


	sockMemoria = com_conectarseA(config_get_string_value(config, "IP_MEMORIA"), config_get_int_value(config, "PUERTO_MEMORIA"));
	if (sockMemoria <= 0) {
		log_error(logger, "Error al conectarse a la memoria IP: %s PUERTO: %d", config_get_string_value(config, "IP_MEMORIA"), config_get_int_value(config, "PUERTO_MEMORIA"));
	}

	if (!com_cpumem_handshake_cpu(sockMemoria, logger)) {
		log_error(logger, "Error en el handshake con la memoria");
		return -1;
	}

	imprimirConexionKernel(sockKernel, sockMemoria);
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	while (!sigusr1) {

		t_mensaje *mensaje = com_recibirMensaje(sockKernel, logger);
		log_trace(logger, "Recibi mensaje>>>> %s", com_imprimirMensaje(mensaje));
		log_info(logger, "");
		log_info(logger, "╔══════════════════════════════════════════════╗");
		log_info(logger, "║------------   Proceso Iniciado   ------------║");
		log_info(logger, "╚══════════════════════════════════════════════╝");
		log_info(logger, "");
		sleep(1);
		if (mensaje->header->tipo != CK_ENVIAR_CONTEXTO) {
			log_error(logger, "El mensaje no es del tipo esperado");
		}

		pcbActivo = malloc(sizeof(t_pcbActivo));
		cargarPCB(mensaje);
		pcbActivo->cantidadRafagas = 0;

		if(pcbActivo->algoritmo == "FIFO"){
			pcbActivo->quantum = 10000; // TODO
		}

		pcbActivo->estadoEnCPU = EJECUTANDO;

		continuarEjecutando = 0;

		while (continuarEjecutando < pcbActivo->quantum) {

			ejecutarRafaga();
			pcbActivo->cantidadRafagas++;

			if (pcbActivo->estadoEnCPU != EJECUTANDO) {
				continuarEjecutando = pcbActivo->quantum + 1000;
			}
		}

		log_trace(logger, "continuarEjecutando despues del while: %d", continuarEjecutando);

		if (pcbActivo->volvioEntradaSalida == 1) {// PORQUE YA HABRE ENVIADO AL KERNEL EN SU RESPECTIVA FUNCION, NO LO VUELVO A ENVIAR

		} else {
			switch (pcbActivo->estadoEnCPU) {
			case EJECUTANDO:
				log_info(logger, " • Fin de Quantum");
				respoderAlKernel(CK_FINALIZO_QUANTUM);
				break;

			case CK_FINALIZO_STACKOVERFLOW:
				log_error(logger, " • Stack Overflow");
				respoderAlKernel(CK_FINALIZO_STACKOVERFLOW);
				break;

			case CK_FINALIZO_EJECUCION:
				respoderAlKernel(CK_FINALIZO_EJECUCION);
				break;

			case CK_PIDIO_ABRIR_ARCHIVO:

				break;

			case CK_PIDIO_LEER_ARCHIVO:

				break;

			case CK_PIDIO_ESCRIBIR_ARCHIVO:

				break;

			case CK_PIDIO_CERRAR_ARCHIVO:

				break;

			case CK_PIDIO_BORRAR_ARCHIVO:

				break;

			case CK_PIDIO_MOVER_CURSOR:

				break;

			case CK_WAIT:

				break;

			default:
				log_error(logger, " • Finalizo con Error");
				respoderAlKernel(CK_FINALIZO_CON_ERROR);
				break;
			}
		}

	}

	log_info(logger, ">>>>>>>>Fin del proceso CPU<<<<<<<<");
	log_destroy(logger);

	return 0;
}

///////////////////////////////////////////FUNCIONES/////////////////////////////////////
void crearLogger() {
	logger = log_create("../logs/cpu.log", "CPU", true, LOG_LEVEL_INFO);

	log_trace(logger, ">>>>>>>>Inicio del proceso CPU<<<<<<<<");
}

void saludar(){

	printf("                ╔════════════════════════════════════════════════╗\n");
	printf("                ║                   DAVID + 4                    ║\n");
	printf("                ║            Bienvenido al proceso CPU           ║\n");
	printf("                ╚════════════════════════════════════════════════╝\n");

}

void imprimirConexionKernel(int sockKernel, int sockMemoria){

	printf("                 ╔══════════════════════════════════════════════╗\n");
	printf("                 ║  • Conectado al Kernel -----> Socket N° %d    ║\n", sockKernel);
	printf("                 ║  • Conectado a la Memoria --> Socket N° %d    ║\n", sockMemoria);
	printf("                 ╚══════════════════════════════════════════════╝\n");

}

void cargarPCB(t_mensaje* mensaje) {
	log_trace(logger, "Voy a cargar el PCB");

	int contador;
	int contadorStack;
	int contadorArgumento;
	int subArg;
	int contadorVariable;
	int subVar;
	int i;
	int s;
	char clavePagina[100];
	char claveOffset[100];
	char claveSize[100];
	char claveArgID[100];
	char claveArgPag[100];
	char claveArgOff[100];
	char claveArgSize[100];
	char claveVarID[100];
	char claveVarPag[100];
	char claveVarOff[100];
	char claveVarSize[100];

	char claveID[100];
	char claveRetPos[100];
	char claveArgumento[100];
	char claveVariable[100];

	char claveRetVarPag[100];
	char claveRetVarOff[100];
	char claveRetVarSiz[100];

	char claveEtiqueta[100];
	char claveUbicacion[100];


	pcbActivo->pid = com_leerIntAMensaje(mensaje, "pid", logger);
	pcbActivo->programCounter = com_leerIntAMensaje(mensaje, "progCounter", logger);
	pcbActivo->quantum = com_leerIntAMensaje(mensaje, "quantum", logger);
	pcbActivo->stackPointer.pagina = com_leerIntAMensaje(mensaje, "stackPagina", logger);
	pcbActivo->stackPointer.offset = com_leerIntAMensaje(mensaje, "stackOffset", logger);
	pcbActivo->stackPointer.size = com_leerIntAMensaje(mensaje, "stackSize", logger);
	pcbActivo->stackLevel = com_leerIntAMensaje(mensaje, "stackLevel", logger);
	pcbActivo->tamPagina = com_leerIntAMensaje(mensaje, "tamPagina", logger);
	pcbActivo->quantumRetardo = com_leerIntAMensaje(mensaje, "quantumSleep", logger);
	pcbActivo->cantidadRafagas = com_leerIntAMensaje(mensaje, "cantidad_rafagas", logger);
	pcbActivo->volvioEntradaSalida = com_leerIntAMensaje(mensaje, "volviEntradaSalida", logger);
	pcbActivo->algoritmo = (char*) com_leerStringAMensaje(mensaje, "algoritmo", logger);
	pcbActivo->stackMaxSize = com_leerIntAMensaje(mensaje, "stackMaxSize", logger);
	pcbActivo->primerPaginaHeap = com_leerIntAMensaje(mensaje, "primerPaginaHeap", logger);

	pcbActivo->nombreSemaforo = string_new();

	pcbActivo->descriptor = com_leerIntAMensaje(mensaje, "descriptor", logger);
	pcbActivo->escritura = 0;
	pcbActivo->creacion = 0;
	pcbActivo->lectura = 0;
	pcbActivo->offsetArch = 0;
	pcbActivo->tamAEscribir = 0;

	pcbActivo->direccion = malloc(1000);
	memset(pcbActivo->direccion, '\0', 1000);
	pcbActivo->informacionEscribir = malloc(1000);
	memset(pcbActivo->informacionEscribir, '\0', 1000);
//	pcbActivo->informacionLeer = malloc(1000);
//	memset(pcbActivo->informacionLeer, '\0', 1000);


	pcbActivo->informacionLeer = (char*) com_leerStringAMensaje(mensaje, "infoLectura", logger);

	if (pcbActivo->programCounter == 0) {
		pcbActivo->cantidadRafagas = 0;
	}

	pcbActivo->indiceStack = list_create();


	contador = com_leerIntAMensaje(mensaje, "contEtiquetas", logger);

	pcbActivo->indiceEtiquetas = list_create();

	for (i = 0; i < contador; i++) {
		t_indiceDeEtiquetas* nuevo = malloc(sizeof(t_indiceDeEtiquetas));

		sprintf(claveEtiqueta, "claveEtiqueta%d", i);
		nuevo->etiquetas = (char*) com_leerStringAMensaje(mensaje, claveEtiqueta, logger);

		sprintf(claveUbicacion, "claveUbicacion%d", i);
		nuevo->ubicacion = com_leerIntAMensaje(mensaje, claveUbicacion, logger);

		list_add(pcbActivo->indiceEtiquetas, nuevo);
	}

	contadorStack = com_leerIntAMensaje(mensaje, "contStack", logger);

	for (s = 0; s < contadorStack; s++) {
		t_stack* level = malloc(sizeof(t_stack));

		sprintf(claveID, "claveID%d", s);
		level->id = com_leerIntAMensaje(mensaje, claveID, logger);

		sprintf(claveRetPos, "retPos%d", s);
		level->retPos = com_leerIntAMensaje(mensaje, claveRetPos, logger);

		sprintf(claveRetVarPag, "retVarPag%d", s);
		level->retVar.pagina = com_leerIntAMensaje(mensaje, claveRetVarPag,
				logger);
		sprintf(claveRetVarOff, "retVarOff%d", s);
		level->retVar.offset = com_leerIntAMensaje(mensaje, claveRetVarOff,
				logger);
		sprintf(claveRetVarSiz, "retVarSiz%d", s);
		level->retVar.size = com_leerIntAMensaje(mensaje, claveRetVarSiz,
				logger);

		sprintf(claveArgumento, "contArgumento%d", s);
		contadorArgumento = com_leerIntAMensaje(mensaje, claveArgumento,
				logger);

		level->argumento = list_create();
		level->variable = list_create();

		for (subArg = 0; subArg < contadorArgumento; subArg++) {
			t_posicion *arg = malloc(sizeof(t_posicion));

			sprintf(claveArgPag, "argPag%d.%d", s, subArg);
			arg->pagina = com_leerIntAMensaje(mensaje, claveArgPag, logger);

			sprintf(claveArgOff, "argOff%d.%d", s, subArg);
			arg->offset = com_leerIntAMensaje(mensaje, claveArgOff, logger);

			sprintf(claveArgSize, "argSize%d.%d", s, subArg);
			arg->size = com_leerIntAMensaje(mensaje, claveArgSize, logger);

			list_add(level->argumento, arg);
		}

		sprintf(claveVariable, "contVariable%d", s);
		contadorVariable = com_leerIntAMensaje(mensaje, claveVariable, logger);

		for (subVar = 0; subVar < contadorVariable; subVar++) {
			t_vars_stack *var = malloc(sizeof(t_vars_stack));

			char* ID;

			sprintf(claveVarID, "varID%d.%d", s, subVar);
			ID = (char*) com_leerStringAMensaje(mensaje, claveVarID, logger);

			var->id = ID[0];

			sprintf(claveVarPag, "varPag%d.%d", s, subVar);
			var->posicion.pagina = com_leerIntAMensaje(mensaje, claveVarPag,
					logger);

			sprintf(claveVarOff, "varOff%d.%d", s, subVar);
			var->posicion.offset = com_leerIntAMensaje(mensaje, claveVarOff,
					logger);

			sprintf(claveVarSize, "varSize%d.%d", s, subVar);
			var->posicion.size = com_leerIntAMensaje(mensaje, claveVarSize,
					logger);

			list_add(level->variable, var);
		}

		list_add(pcbActivo->indiceStack, level);
	}

	contador = com_leerIntAMensaje(mensaje, "cantInst", logger);

	pcbActivo->indiceCodigo = list_create();

	for (i = 0; i < contador; i++) {
		t_codigo* nuevo = malloc(sizeof(t_codigo));

		sprintf(clavePagina, "pagina%d", i);
		nuevo->pagina = com_leerIntAMensaje(mensaje, clavePagina, logger);

		sprintf(claveOffset, "offset%d", i);
		nuevo->offset = com_leerIntAMensaje(mensaje, claveOffset, logger);

		sprintf(claveSize, "size%d", i);
		nuevo->size = com_leerIntAMensaje(mensaje, claveSize, logger);

		list_add(pcbActivo->indiceCodigo, nuevo);
	}
}

void ejecutarRafaga() {
	log_trace(logger, "Voy a ejecutar una rafaga");

	t_codigo* pos;
	char *sentencia = NULL;

	pos = list_get(pcbActivo->indiceCodigo, pcbActivo->programCounter);

	sentencia = pedirAMemoriaString(pos->pagina, pos->offset, pos->size);

	char* begin = "begin";
	if (strcmp(sentencia, begin) == 0) { // El Begin esta al principio
		log_trace(logger, "Llego el BEGIN");
		pcbActivo->programCounter++;
	} else {
		if (pcbActivo->programCounter == 0) { // El BEGIN no esta al principio
			log_trace(logger, "Voy a buscar el Begin, no esta al principio");
			pcbActivo->programCounter = villereada(begin, 0);
			log_trace(logger, "Encontre el Begin en la pos: [%d]", pcbActivo->programCounter);
			t_codigo* poss = list_get(pcbActivo->indiceCodigo, pcbActivo->programCounter);
			sentencia = pedirAMemoriaString(poss->pagina, poss->offset, poss->size);
			usleep(pcbActivo->quantumRetardo * 10000);

			printf("                 ╔══════════════════════════════════════════════╗\n");
			printf("                 ║  • Conectado al Kernel -----> Socket N° %d    ║\n", sockKernel);
			printf("                 ║  • Conectado a la Memoria --> Socket N° %d    ║\n", sockMemoria);
			printf("                 ╚══════════════════════════════════════════════╝\n");

			t_stack* test = list_get(pcbActivo->indiceStack, pcbActivo->stackLevel);
			log_info(logger, "");
			log_info(logger, "╔══════════════════════════════════════════════╗");
			log_info(logger, "║          •  SENTENCIA: %s", sentencia);
			log_info(logger, "║          •  PROGRAM COUNTER: %d               ", pcbActivo->programCounter);
			log_info(logger, "║          •  RAFAGAS: %d                       ", pcbActivo->cantidadRafagas);
			log_info(logger, "║          •  ID STACK: %d                      ", test->id);
			log_info(logger, "╚══════════════════════════════════════════════╝");
			log_info(logger, "");

			usleep(pcbActivo->quantumRetardo * 10000);
			analizadorLinea(sentencia, &funciones, &kernel);
		} else { // El BEGIN esta al principio

			if (sentencia != NULL) {
				log_trace(logger, "Voy a analizar una linea");

				t_stack* test = list_get(pcbActivo->indiceStack, pcbActivo->stackLevel);
				log_info(logger, "");
				log_info(logger, "╔══════════════════════════════════════════════╗");
				log_info(logger, "║          •  SENTENCIA: %s", sentencia);
				log_info(logger, "║          •  PROGRAM COUNTER: %d               ", pcbActivo->programCounter);
				log_info(logger, "║          •  RAFAGAS: %d                       ", pcbActivo->cantidadRafagas);
				log_info(logger, "║          •  ID STACK: %d                      ", test->id);
				log_info(logger, "╚══════════════════════════════════════════════╝");
				log_info(logger, "");

				usleep(pcbActivo->quantumRetardo * 10000);
				analizadorLinea(sentencia, &funciones, &kernel);

				log_trace(logger, "Termine de analizar una linea");

				if (pcbActivo->volvioEntradaSalida == 0) {
					pcbActivo->programCounter++;
					continuarEjecutando++;
				}

			} else {
				pcbActivo->estadoEnCPU = CK_FINALIZO_CON_ERROR;
			}
		}
	}
}

void almacenarEnMemoriaUnValor(int pid, t_posicion pos, int valor, char* texto, int tipo) {
	log_trace(logger, "Voy a almacenar un valor en la memoria");
	log_trace(logger, "Almacenar en Pag[%d], Off[%d], Siz[%d]", pos.pagina, pos.offset, pos.size);

	if(texto != NULL){
		log_trace(logger, "Es distinto de NULL");
	}

	t_mensaje *mensaje = com_nuevoMensaje(CM_GUARDAR_EN_MEMORIA, logger);

	com_agregarIntAMensaje(mensaje, "pid", pcbActivo->pid, logger);
	com_agregarIntAMensaje(mensaje, "pagina", pos.pagina, logger);
	com_agregarIntAMensaje(mensaje, "offset", pos.offset, logger);
	com_agregarIntAMensaje(mensaje, "size", pos.size, logger);
	com_agregarIntAMensaje(mensaje, "esNum", tipo, logger);

	if(tipo == 1){
		com_agregarIntAMensaje(mensaje, "valor", valor, logger);
	} else{
		com_agregarStringAMensaje(mensaje, "texto", texto, logger);
	}


	log_trace(logger, "Mensaje que voy a enviar>>>> \n%s", com_imprimirMensaje(mensaje));

	com_enviarMensaje(mensaje, sockMemoria, logger);

	log_trace(logger, "Voy a destruir el mensaje");
	com_destruirMensaje(mensaje, logger);

	log_trace(logger, "Voy a recibir un mensaje de la memoria");
	t_mensaje *mensajeRecibido = com_recibirMensaje(sockMemoria, logger);

	if (mensajeRecibido->header->tipo != MC_GUARDAR_RESPUESTA) {
		log_error(logger, "Error, el tipo de mensaje no es el que esperaba");
		abortar = 1;
		respoderAlKernel(CK_FINALIZO_CON_ERROR);
	}
	if (mensajeRecibido->header->len == 0) {
		log_error(logger, "Error, el mensaje deberia tener un body");
		abortar = 1;
	}

	log_trace(logger, "Mensaje que recibi>>>> \n%s", com_imprimirMensaje(mensajeRecibido));

}

char* pedirAMemoriaString(int pagina, int offset, int size) {
	log_trace(logger, "Empiezo a pedir a Memoria");

	t_mensaje *mensaje = com_nuevoMensaje(CM_PEDIR_DATO_MEMORIA, logger);
	com_agregarIntAMensaje(mensaje, "pid", pcbActivo->pid, logger);
	com_agregarIntAMensaje(mensaje, "pagina", pagina, logger);
	com_agregarIntAMensaje(mensaje, "offset", offset, logger);
	com_agregarIntAMensaje(mensaje, "size", size, logger);
	com_agregarIntAMensaje(mensaje, "esNum", 0, logger);

	//log_trace(logger, "Mensaje que voy a enviar>>>> \n%s", com_imprimirMensaje(mensaje));
	com_enviarMensaje(mensaje, sockMemoria, logger);

	log_trace(logger, "Voy a destruir el mensaje");
	com_destruirMensaje(mensaje, logger);

	log_trace(logger, "Voy a recibir un mensaje");
	t_mensaje *mensajeRecibido = com_recibirMensaje(sockMemoria, logger);

	if (mensajeRecibido->header->tipo != MC_DATO_RESPUESTA) {
		log_error(logger, "Error, el tipo de mensaje no es el que esperaba");
		abortar = 1;
		respoderAlKernel(CK_FINALIZO_CON_ERROR);
		return NULL;
	}

	if (mensajeRecibido->header->len == 0) {
		log_error(logger, "Error, el mensaje deberia tener un body");
		abortar = 1;
		return NULL;
	}

		log_trace(logger, "Mensaje que recibi>>>> \n%s", com_imprimirMensaje(mensajeRecibido));

		log_trace(logger, "Parseo el mensaje");

		int resultadoLectura = com_leerIntAMensaje(mensajeRecibido, "resultadoLectura", logger);

		if(resultadoLectura == 1){
			char* resultado = (char*) com_leerStringAMensaje(mensajeRecibido, "buffer",	logger);

			if (resultado == "rompi") {
				log_trace(logger, "ME PASASTE ROMPI");
				pcbActivo->estadoEnCPU = CK_FINALIZO_CON_ERROR;
			}

			return resultado;
		}else{
			pcbActivo->estadoEnCPU = CK_FINALIZO_CON_ERROR;
			respoderAlKernel(CK_FINALIZO_CON_ERROR);
			return "";
		}

}

int pedirAMemoriaEntero(int pagina, int offset, int size) {
	log_trace(logger, "Empiezo a pedir a Memoria");

	t_mensaje *mensaje = com_nuevoMensaje(CM_PEDIR_DATO_MEMORIA, logger);
	com_agregarIntAMensaje(mensaje, "pid", pcbActivo->pid, logger);
	com_agregarIntAMensaje(mensaje, "pagina", pagina, logger);
	com_agregarIntAMensaje(mensaje, "offset", offset, logger);
	com_agregarIntAMensaje(mensaje, "size", size, logger);
	com_agregarIntAMensaje(mensaje, "esNum", 1, logger);

	//log_trace(logger, "Mensaje que voy a enviar>>>> \n%s", com_imprimirMensaje(mensaje));
	com_enviarMensaje(mensaje, sockMemoria, logger);

	log_trace(logger, "Voy a destruir el mensaje");
	com_destruirMensaje(mensaje, logger);

	log_trace(logger, "Voy a recibir un mensaje");
	t_mensaje *mensajeRecibido = com_recibirMensaje(sockMemoria, logger);

	if (mensajeRecibido->header->tipo != MC_DATO_RESPUESTA) {
		log_error(logger, "Error, el tipo de mensaje no es el que esperaba");
		abortar = 1;
		respoderAlKernel(CK_FINALIZO_CON_ERROR);
		return -1;
	}

		log_trace(logger, "Mensaje que recibi>>>> \n%s", com_imprimirMensaje(mensajeRecibido));

		log_trace(logger, "Parseo el mensaje");

		int resultado = com_leerIntAMensaje(mensajeRecibido, "entero", logger);
		int resultadoLectura = com_leerIntAMensaje(mensajeRecibido, "resultadoLectura", logger);

		if(resultadoLectura == 0){
			pcbActivo->estadoEnCPU = CK_FINALIZO_CON_ERROR;
		}

		return resultado;

}

t_puntero posicionConvertirAPuntero(t_posicion pos) {
	t_puntero ptr = (pos.pagina * pcbActivo->tamPagina) + pos.offset;

	return ptr;
}

t_posicion punteroConvertirAPosicion(t_puntero ptr) {

	t_posicion pos;

	pos.pagina = ptr / pcbActivo->tamPagina;
	pos.offset = abs((pcbActivo->tamPagina * pos.pagina) - ptr);
	pos.size = 4;
	return pos;

}

void respoderAlKernel(int valor) {
	log_trace(logger, "Finalizo la ejecucion del proceso");
	int i;
	int a;
	int v;
	int c;
	char claveID[100];
	char claveRetPos[100];
	char claveRetVarPag[100];
	char claveRetVarOff[100];
	char claveRetVarSiz[100];
	char claveArgumento[100];
	char claveArgID[100];
	char claveArgPag[100];
	char claveArgOff[100];
	char claveArgSize[100];
	char claveVariable[100];
	char claveVarID[100];
	char claveVarPag[100];
	char claveVarOff[100];
	char claveVarSize[100];
	char clavePagina[100];
	char claveOffset[100];
	char claveSize[100];

	char claveEtiqueta[100];
	char claveUbicacion[100];

	t_mensaje *mensaje = com_nuevoMensaje(valor, logger);
	com_agregarIntAMensaje(mensaje, "pid", pcbActivo->pid, logger);
	com_agregarIntAMensaje(mensaje, "progCounter", pcbActivo->programCounter, logger);
	com_agregarIntAMensaje(mensaje, "quantum", pcbActivo->quantum, logger);
	com_agregarIntAMensaje(mensaje, "stackPagina", pcbActivo->stackPointer.pagina, logger);
	com_agregarIntAMensaje(mensaje, "stackOffset", pcbActivo->stackPointer.offset, logger);
	com_agregarIntAMensaje(mensaje, "stackSize", pcbActivo->stackPointer.size, logger);
	com_agregarIntAMensaje(mensaje, "stackLevel", pcbActivo->stackLevel, logger);
	com_agregarIntAMensaje(mensaje, "continua_activo", pcbActivo->estadoEnCPU, logger);

	com_agregarIntAMensaje(mensaje, "cantidad_rafagas",	pcbActivo->cantidadRafagas, logger);

	com_agregarIntAMensaje(mensaje, "descriptor", pcbActivo->descriptor, logger);

	if(strcmp(pcbActivo->direccion, "") != 0){
		com_agregarStringAMensaje(mensaje, "direccion", pcbActivo->direccion, logger);
	}else{
		com_agregarStringAMensaje(mensaje, "direccion", "-", logger);
	}

	com_agregarIntAMensaje(mensaje, "creacion", pcbActivo->creacion, logger);
	com_agregarIntAMensaje(mensaje, "escritura", pcbActivo->escritura, logger);
	com_agregarIntAMensaje(mensaje, "lectura", pcbActivo->lectura, logger);
	com_agregarIntAMensaje(mensaje, "offsetArch", pcbActivo->offsetArch, logger);
	com_agregarIntAMensaje(mensaje, "tamAEscribir", pcbActivo->tamAEscribir, logger);

	if(strcmp(pcbActivo->informacionEscribir, "") != 0){
		com_agregarStringAMensaje(mensaje, "infoAEscribir", pcbActivo->informacionEscribir, logger);
	}else{
		com_agregarStringAMensaje(mensaje, "infoAEscribir", "-", logger);
	}

	com_agregarIntAMensaje(mensaje, "contEtiquetas", list_size(pcbActivo->indiceEtiquetas), logger);

	if(strcmp(pcbActivo->nombreSemaforo, "") != 0) {
		com_agregarStringAMensaje(mensaje, "nombre_semaforo", pcbActivo->nombreSemaforo, logger);
	}else{
		com_agregarStringAMensaje(mensaje, "nombre_semaforo", "-", logger);
	}

	for (i = 0; i < list_size(pcbActivo->indiceEtiquetas); i++) {
		t_indiceDeEtiquetas* nuevo = list_get(pcbActivo->indiceEtiquetas,i);

		sprintf(claveEtiqueta, "claveEtiqueta%d", i);
		com_agregarStringAMensaje(mensaje, claveEtiqueta, nuevo->etiquetas, logger);

		sprintf(claveUbicacion, "claveUbicacion%d", i);
		com_agregarIntAMensaje(mensaje, claveUbicacion, nuevo->ubicacion, logger);

	}

	com_agregarIntAMensaje(mensaje, "contStack", list_size(pcbActivo->indiceStack), logger);

	for (i = 0; i < list_size(pcbActivo->indiceStack); i++) {
		t_stack* level = (t_stack*) list_get(pcbActivo->indiceStack, i);

		sprintf(claveID, "claveID%d", i);
		com_agregarIntAMensaje(mensaje, claveID, level->id, logger);

		sprintf(claveRetPos, "retPos%d", i);
		com_agregarIntAMensaje(mensaje, claveRetPos, level->retPos, logger);

		sprintf(claveRetVarPag, "retVarPag%d", i);
		com_agregarIntAMensaje(mensaje, claveRetVarPag, level->retVar.pagina, logger);

		sprintf(claveRetVarOff, "retVarOff%d", i);
		com_agregarIntAMensaje(mensaje, claveRetVarOff, level->retVar.offset, logger);

		sprintf(claveRetVarSiz, "retVarSiz%d", i);
		com_agregarIntAMensaje(mensaje, claveRetVarSiz, level->retVar.size, logger);

		sprintf(claveArgumento, "contArgumento%d", i);
		com_agregarIntAMensaje(mensaje, claveArgumento, list_size(level->argumento), logger);

		for (a = 0; a < list_size(level->argumento); a++) {
			t_posicion* arg = (t_posicion*) list_get(level->argumento, a);

			sprintf(claveArgPag, "argPag%d.%d", i, a);
			com_agregarIntAMensaje(mensaje, claveArgPag, arg->pagina, logger);

			sprintf(claveArgOff, "argOff%d.%d", i, a);
			com_agregarIntAMensaje(mensaje, claveArgOff, arg->offset, logger);

			sprintf(claveArgSize, "argSize%d.%d", i, a);
			com_agregarIntAMensaje(mensaje, claveArgSize, arg->size, logger);

		}

		sprintf(claveVariable, "contVariable%d", i);
		com_agregarIntAMensaje(mensaje, claveVariable, list_size(level->variable), logger);

		for (v = 0; v < list_size(level->variable); v++) {
			t_vars_stack* var = (t_vars_stack*) list_get(level->variable, v);

			sprintf(claveVarID, "varID%d.%d", i, v);
			char *ID = malloc(10);
			memset(ID, '\0', 10);
			ID[0] = var->id;

			com_agregarStringAMensaje(mensaje, claveVarID, ID, logger);

			free(ID);

			sprintf(claveVarPag, "varPag%d.%d", i, v);
			com_agregarIntAMensaje(mensaje, claveVarPag, var->posicion.pagina, logger);

			sprintf(claveVarOff, "varOff%d.%d", i, v);
			com_agregarIntAMensaje(mensaje, claveVarOff, var->posicion.offset, logger);

			sprintf(claveVarSize, "varSize%d.%d", i, v);
			com_agregarIntAMensaje(mensaje, claveVarSize, var->posicion.size, logger);

		}
	}

	log_trace(logger, "Mensaje que voy a enviar>>>> \n%s", com_imprimirMensaje(mensaje));

	com_enviarMensaje(mensaje, sockKernel, logger);

	log_trace(logger, "Voy a destruir el mensaje");
	com_destruirMensaje(mensaje, logger);

	log_info(logger, " • CPU libre");

}

/////////////////////////////////////////// PRIMITIVAS//////////////////////////////////////////////////////////////////

void actualizarStackPointer() {

	int paginaID = pcbActivo->stackPointer.pagina;
	int offset = pcbActivo->stackPointer.offset;
	int size = 4;

	if ((offset + size) >= pcbActivo->tamPagina) {
		pcbActivo->stackPointer.pagina = paginaID + 1;
		pcbActivo->stackPointer.offset = (offset + size) - pcbActivo->tamPagina;
	} else {
		pcbActivo->stackPointer.pagina = paginaID;
		pcbActivo->stackPointer.offset = offset + size;
	}
}

t_puntero definirVariable(t_nombre_variable variable) {

	log_trace(logger, "  • Definir Variable [%c]", variable);

	t_puntero ptr;
	ptr = posicionConvertirAPuntero(pcbActivo->stackPointer);

	t_stack* item = (t_stack*) list_get(pcbActivo->indiceStack,	pcbActivo->stackLevel);

	if (variable >= '0' && variable <= '9') {
		// La variable es un argumento
		t_posicion* posArg = malloc(sizeof(t_posicion));

		posArg->pagina = pcbActivo->stackPointer.pagina;
		posArg->offset = pcbActivo->stackPointer.offset;
		posArg->size = pcbActivo->stackPointer.size;

		list_add(item->argumento, posArg);
		//list_replace(pcbActivo->indiceStack, pcbActivo->stackLevel, item);

	} else {
		//La variable es una variable... redundante no?
		log_trace(logger, "Definir Variable");
		t_vars_stack* nuevaVar = malloc(sizeof(t_vars_stack));

		nuevaVar->id = variable;
		nuevaVar->posicion = pcbActivo->stackPointer;
		nuevaVar->posicion.size = 4;

		list_add(item->variable, nuevaVar);

		//list_replace(pcbActivo->indiceStack, pcbActivo->stackLevel, item);
	}

	actualizarStackPointer(); // Actualizo la pos del Stack para cuando venga la prox variable

	return ptr;
}

t_puntero obtenerPosicionVariable(t_nombre_variable nombre_variable) {
	log_trace(logger, "Obtener posicion variable [%c]", nombre_variable);
//
//	if (pcbActivo->volvioEntradaSalida == 0) {

		t_stack* item = (t_stack*) list_get(pcbActivo->indiceStack,
				pcbActivo->stackLevel);

		if (nombre_variable >= '0' && nombre_variable <= '9') {

			t_posicion* arg = (t_posicion*) list_get(item->argumento,
					(int) nombre_variable);

			if (arg == 0) {
				log_error(logger, "No se encontro el argumento %c",
						nombre_variable);
				return -1;
			}

			t_puntero ptrArg = posicionConvertirAPuntero(*arg);

			log_trace(logger,
					"Encontre la posicion del argumento: %d ... |%d|%d|%d|",
					ptrArg, arg->pagina, arg->offset, arg->size);
			return ptrArg;

		} else { //ES una variable de las comunes

			bool queEsVar(t_vars_stack* var) {
				return var->id == nombre_variable;
			}

			t_vars_stack* var = (t_vars_stack*) list_find(item->variable,
					(void*) queEsVar);

			if (var == 0) {
				log_error(logger, "La variable [%c] no se encuentra.",
						nombre_variable);
				return -1;
			}

			t_puntero ptrVar = posicionConvertirAPuntero(var->posicion);
			log_trace(logger," • Posicion Encontrada: PAG -► %d, OFF -► %d, SIZ -► %d ", var->posicion.pagina, var->posicion.offset, var->posicion.size);


			return ptrVar;
		}
//	} else {
//		log_trace(logger, "Paso de largo");
//		return 1;
//	}
}

t_valor_variable dereferenciar(t_puntero puntero) {
	log_trace(logger, " • Dereferenciar Puntero [%p]", puntero);

	if(pcbActivo->estadoEnCPU != EJECUTANDO){
		log_trace(logger, "No hago nada");
		return 0;
	}
	t_posicion pos = punteroConvertirAPosicion(puntero);

	if(pos.pagina >= pcbActivo->primerPaginaHeap){

		char* buff = pedirAMemoriaString(pos.pagina, pos.offset, 1);

		if (buff != -1) {
			log_trace(logger, "Solicitud de pos a Memoria exitosa. --> %s", buff);

			string_append(&(pcbActivo->informacionEscribir), buff);
			construccion = 10;

		} else {
			log_trace(logger, "Fallo la solicitud de una pos a Memoria");

			pcbActivo->estadoEnCPU = CK_FINALIZO_CON_ERROR;
		}

		return 0;

	}else{
		int buff = pedirAMemoriaEntero(pos.pagina, pos.offset, pos.size);
		if (buff != -1) {
			log_trace(logger, "Solicitud de pos a Memoria exitosa. --> %d", buff);
		} else {
			log_error(logger, "Fallo la solicitud de una pos a Memoria");

			pcbActivo->estadoEnCPU = CK_FINALIZO_CON_ERROR;
		}

		log_trace(logger, " • El valor de [%p] es [%d]", puntero, buff);
		return buff;
	}

}

void asignar(t_puntero puntero, t_valor_variable valor_variable) {
	log_trace(logger, " • Asignar al puntero [%p] el valor [%d]", puntero, valor_variable);

		t_posicion pos = punteroConvertirAPosicion(puntero);
		pos.size = 4;

		if(pos.pagina >= pcbActivo->primerPaginaHeap){
			char buffer[1000];
			sprintf(buffer, "%d\0", valor_variable);
			pos.size = strlen(buffer) + 1;

			almacenarEnMemoriaUnValor(pcbActivo->pid, pos, 0, buffer, 0);

		}else{
			almacenarEnMemoriaUnValor(pcbActivo->pid, pos, valor_variable, NULL, 1);
		}

}

void irAlLabel(t_nombre_etiqueta nombre_etiqueta){
	log_trace(logger, " • Ir a Lebel");
	irAFuncion(nombre_etiqueta);
}

void irAFuncion(t_nombre_etiqueta nombre_etiqueta) {

	log_trace(logger, "Voy a buscar una funcion");
	log_trace(logger, " • Saltar a la funcion: [%s]", nombre_etiqueta);
	int ubicacion;

//	t_puntero_instruccion instruccion = metadata_buscar_etiqueta(nombre_etiqueta, pcbActivo->indiceEtiquetas->etiquetas,  pcbActivo->indiceEtiquetas->etiquetas_size);
	log_trace(logger, "Index Etiqueta: "/*%d", instruccion*/);

	ubicacion = villereada(nombre_etiqueta, 1);

	if (ubicacion == -1) {
		log_trace(logger, "No se encontro la funcion solicitada");
	} else {

		log_trace(logger, "Encontre la funcion en la linea: [%d]", ubicacion);

		pcbActivo->programCounter = (int) ubicacion;
	}
}

t_puntero alocar(t_valor_variable espacio) {
	log_trace(logger, "Reserva [%d] espacio", espacio);

	t_mensaje *mensaje = com_nuevoMensaje(CK_ALOCAR, logger);
	com_agregarIntAMensaje(mensaje, "pid", pcbActivo->pid, logger);
	com_agregarIntAMensaje(mensaje, "espacio", espacio, logger);

	log_trace(logger, "Mensaje que voy a enviar>>>> \n%s", com_imprimirMensaje(mensaje));

	com_enviarMensaje(mensaje, sockKernel, logger);

	log_trace(logger, "Voy a destruir el mensaje");
	com_destruirMensaje(mensaje, logger);

	log_trace(logger, "Voy a recibir un mensaje");
	t_mensaje *mensajeRecibido = com_recibirMensaje(sockKernel, logger);

	if (mensajeRecibido->header->tipo != KC_ALOCAR_RESPUESTA) {
		log_error(logger, "Error, el tipo de mensaje no es el que esperaba");
		respoderAlKernel(CK_FINALIZO_CON_ERROR);
		return -1;
	}

	if (mensajeRecibido->header->len == 0) {
		log_error(logger, "Error, el mensaje deberia tener un body");
		return -1;
	}

	log_trace(logger, "Mensaje que recibi>>>> \n%s", com_imprimirMensaje(mensajeRecibido));

	log_trace(logger, "Parseo el mensaje");
	t_puntero resultado = (t_puntero) com_leerIntAMensaje(mensajeRecibido, "posicion", logger);

	return resultado;
}

void liberar(t_puntero puntero) {
	log_trace(logger, "Libera [%p] espacio", puntero);

	t_mensaje *mensaje = com_nuevoMensaje(CK_LIBERAR, logger);
	com_agregarIntAMensaje(mensaje, "pid", pcbActivo->pid, logger);

	com_agregarIntAMensaje(mensaje, "puntero", puntero, logger);

	log_trace(logger, "Mensaje que voy a enviar>>>> \n%s", com_imprimirMensaje(mensaje));

	com_enviarMensaje(mensaje, sockKernel, logger);

	log_trace(logger, "Voy a destruir el mensaje");
	com_destruirMensaje(mensaje, logger);

	log_trace(logger, "Voy a recibir un mensaje");
	t_mensaje *mensajeRecibido = com_recibirMensaje(sockKernel, logger);

	if (mensajeRecibido->header->tipo != KC_LIBERAR_RESPUESTA) {
		log_error(logger, "Error, el tipo de mensaje no es el que esperaba");
		respoderAlKernel(CK_FINALIZO_CON_ERROR);
	}

	log_trace(logger, "Mensaje que recibi>>>> \n%s", com_imprimirMensaje(mensajeRecibido));
}

char* boolToChar(bool boolean) {
	return boolean ? "✔" : "✖";
}

t_descriptor_archivo abrir(t_direccion_archivo direccion, t_banderas banderas) {
	log_trace(logger, "Abrir [%s] Lectura: %s. Escritura: %s, Creacion: %s", direccion, boolToChar(banderas.lectura), boolToChar(banderas.escritura), boolToChar(banderas.creacion));

	t_descriptor_archivo descriptor;
	log_trace(logger, "[Volvi de entrada salida]: %d", pcbActivo->volvioEntradaSalida);
	if (pcbActivo->volvioEntradaSalida == 0) {

		pcbActivo->direccion = malloc(100);

		strcpy(pcbActivo->direccion, direccion);

		pcbActivo->escritura = banderas.escritura;
		pcbActivo->creacion = banderas.creacion;
		pcbActivo->lectura = banderas.lectura;

		pcbActivo->estadoEnCPU = CK_PIDIO_ABRIR_ARCHIVO;

		voyAEntradaSalida = 1;
		pcbActivo->volvioEntradaSalida = 1;

		respoderAlKernel(CK_PIDIO_ABRIR_ARCHIVO);
	} else {
		descriptor = pcbActivo->descriptor;
		voyAEntradaSalida = 0;
		pcbActivo->volvioEntradaSalida = 0;
	}
	pcbActivo->volvioEntradaSalida = 0;
	return descriptor;
}

void escribir(t_descriptor_archivo desc, void* informacion,	t_valor_variable tamanio) {

	if(pcbActivo->volvioEntradaSalida == 0){
		log_trace(logger, "Aumento construccion: %d", construccion);

		if(construccion == 10){
			log_trace(logger, " • Escribir [%s] en el descriptor [%d] con tamaño [%d]", pcbActivo->informacionEscribir, desc, tamanio);
			construccion = 0;
		}else{
			log_trace(logger, " • Escribir [%s] en el descriptor [%d] con tamaño [%d]", informacion, desc, tamanio);
			pcbActivo->informacionEscribir = informacion;
			construccion = 0;
		}

		pcbActivo->descriptor = desc;
		pcbActivo->tamAEscribir = strlen(pcbActivo->informacionEscribir) + 1;
		pcbActivo->estadoEnCPU = CK_PIDIO_ESCRIBIR_ARCHIVO;

		voyAEntradaSalida = 0;
		pcbActivo->volvioEntradaSalida = 0;

		respoderAlKernel(CK_PIDIO_ESCRIBIR_ARCHIVO);

	}else{
		construccion = 0;
		voyAEntradaSalida = 0;
		pcbActivo->volvioEntradaSalida = 0;
	}
	pcbActivo->volvioEntradaSalida = 0;

}

void leer(t_descriptor_archivo descriptor, t_puntero puntero, t_valor_variable tamanio) {
	log_trace(logger, "Leer [%d] a [%p] con tamaño [%d]", descriptor, puntero, tamanio);

	if(pcbActivo->volvioEntradaSalida == 0){

		pcbActivo->descriptor = descriptor;
		pcbActivo->tamAEscribir = tamanio;
		pcbActivo->estadoEnCPU = CK_PIDIO_LEER_ARCHIVO;

		voyAEntradaSalida = 1;
		pcbActivo->volvioEntradaSalida = 1;

		respoderAlKernel(CK_PIDIO_LEER_ARCHIVO);

	}else{

		t_posicion pos = punteroConvertirAPosicion(puntero);
		char *buffer = malloc(tamanio + 1);
		memset(buffer, '\0', tamanio +1);
		memset(buffer, ' ', tamanio);
		memcpy(buffer, pcbActivo->informacionLeer, strlen(pcbActivo->informacionLeer));
		pos.size = tamanio;
		almacenarEnMemoriaUnValor(pcbActivo->pid, pos, 0, buffer, 0);

		voyAEntradaSalida = 0;
		pcbActivo->volvioEntradaSalida = 0;
	}
	pcbActivo->volvioEntradaSalida = 0;
}

void borrar(t_descriptor_archivo descriptor) {
	log_trace(logger, "Borrar [%d]", descriptor);

	if(pcbActivo->volvioEntradaSalida == 0){

		pcbActivo->descriptor = descriptor;
		pcbActivo->estadoEnCPU = CK_PIDIO_BORRAR_ARCHIVO;

		voyAEntradaSalida = 1;
		pcbActivo->volvioEntradaSalida = 1;

		respoderAlKernel(CK_PIDIO_BORRAR_ARCHIVO);

	}else{

		voyAEntradaSalida = 0;
		pcbActivo->volvioEntradaSalida = 0;
	}
	pcbActivo->volvioEntradaSalida = 0;
}

void cerrar(t_descriptor_archivo descriptor) {
	log_trace(logger, "Cerrar [%d]", descriptor);

	if(pcbActivo->volvioEntradaSalida == 0){

		pcbActivo->descriptor = descriptor;
		pcbActivo->estadoEnCPU = CK_PIDIO_CERRAR_ARCHIVO;

		voyAEntradaSalida = 1;
		pcbActivo->volvioEntradaSalida = 1;

		respoderAlKernel(CK_PIDIO_CERRAR_ARCHIVO);

	}else{

		voyAEntradaSalida = 0;
		pcbActivo->volvioEntradaSalida = 0;
	}
	pcbActivo->volvioEntradaSalida = 0;

}

void moverCursor(t_descriptor_archivo descriptor, t_valor_variable posicion) {
	log_trace(logger, "Mover descriptor [%d] a [%d]", descriptor, posicion);

	pcbActivo->descriptor = descriptor;
	pcbActivo->offsetArch = posicion;
	pcbActivo->estadoEnCPU = CK_PIDIO_MOVER_CURSOR;
}


t_valor_variable obtenerValorCompartida(t_nombre_compartida variable) {
	log_trace(logger, "Voy a obtener el valor de una variable compartida: [%s]", variable);

	char * variableCompartida = malloc(100);

	sprintf(variableCompartida, "!%s", variable);

	t_mensaje *mensaje = com_nuevoMensaje(CK_VARIABLE_LECTURA, logger);
	com_agregarStringAMensaje(mensaje, "variable", variableCompartida, logger);
	com_agregarIntAMensaje(mensaje, "pid", pcbActivo->pid, logger);

	log_trace(logger, "Mensaje que voy a enviar>>>> \n%s", com_imprimirMensaje(mensaje));

	com_enviarMensaje(mensaje, sockKernel, logger);

	log_trace(logger, "Voy a destruir el mensaje");
	com_destruirMensaje(mensaje, logger);

	log_trace(logger, "Voy a recibir un mensaje");
	t_mensaje *mensajeRecibido = com_recibirMensaje(sockKernel, logger);

	if (mensajeRecibido->header->tipo != KC_VARIABLE_LECTURA_RESPUESTA) {
		log_error(logger, "Error, el tipo de mensaje no es el que esperaba");
		respoderAlKernel(CK_FINALIZO_CON_ERROR);
		//TODO return -1;
	}

	if (com_leerIntAMensaje(mensajeRecibido, "resultado_respuesta", logger) == 0) {
		log_error(logger, "Error, en leer la var compartida");
		//TODO return -1;
	}

	log_trace(logger, "Mensaje que recibi>>>> \n%s", com_imprimirMensaje(mensajeRecibido));
	log_trace(logger, "Parseo el mensaje");
	t_valor_variable resultado = com_leerIntAMensaje(mensajeRecibido, "valor", logger);

	return resultado;
}

t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor) {
	log_trace(logger, "Voy a obtener el valor de una variable compartida: [%d]", valor);

	char * variableCompartida = malloc(100);

	sprintf(variableCompartida, "!%s", variable);

	t_mensaje *mensaje = com_nuevoMensaje(CK_VARIABLE_ESCRITURA, logger);
	com_agregarStringAMensaje(mensaje, "nombre_variable", variableCompartida, logger);
	com_agregarIntAMensaje(mensaje, "valor", valor, logger);
	com_agregarIntAMensaje(mensaje, "pid", pcbActivo->pid, logger);

	log_trace(logger, "Mensaje que voy a enviar>>>> \n%s", com_imprimirMensaje(mensaje));

	com_enviarMensaje(mensaje, sockKernel, logger);

	log_trace(logger, "Voy a destruir el mensaje");
	com_destruirMensaje(mensaje, logger);

	log_trace(logger, "Voy a recibir un mensaje");
	t_mensaje *mensajeRecibido = com_recibirMensaje(sockKernel, logger);

	if (mensajeRecibido->header->tipo != KC_VARIABLE_ESCRITURA_RESPUESTA) {
		log_error(logger, "Error, el tipo de mensaje no es el que esperaba");
		respoderAlKernel(CK_FINALIZO_CON_ERROR);
		//TODO return -1;
	}

	if (mensajeRecibido->header->len == 0) {
		log_error(logger, "Error, el mensaje deberia tener un body");
		//TODO return -1;
	}

	log_trace(logger, "Mensaje que recibi>>>> \n%s", com_imprimirMensaje(mensajeRecibido));

	log_trace(logger, "Parseo el mensaje");
	t_valor_variable resultado = (t_valor_variable) com_leerIntAMensaje(mensajeRecibido, "resultado_respuesta", logger);

	if (resultado == 0) {
		pcbActivo->estadoEnCPU = CK_FINALIZO_CON_ERROR;
	}

	return resultado;
}

void finalizar() {
	log_info(logger, "╔══════════════════════════════════════════════╗");
	log_info(logger, "║-----------   Proceso Finalizado   -----------║");
	log_info(logger, "╚══════════════════════════════════════════════╝");

	char* sentencia;

	t_stack* nivel = list_get(pcbActivo->indiceStack, pcbActivo->stackLevel);

	if (pcbActivo->stackLevel > 0) {

		pcbActivo->programCounter = nivel->retPos;
		log_trace(logger, "STACK LEVEL: %d", pcbActivo->stackLevel);
		log_trace(logger, "STACK LEVEL SIZE: %d",
				list_size(pcbActivo->indiceStack));

		//Libero los recursos del contexto!!!
		if (list_size(nivel->argumento) > 0) {
			void liberarArgs(t_posicion* item) {
				free(item);
			}
			list_destroy_and_destroy_elements(nivel->argumento,
					(void*) liberarArgs);
		} else {
			list_destroy(nivel->argumento);
		}
		log_trace(logger, "Destrui Argumentos");

		if (list_size(nivel->variable) > 0) {
			void liberarVars(t_vars_stack* item) {
				free(item);
			}
			list_destroy_and_destroy_elements(nivel->variable,
					(void*) liberarVars);
		} else {
			list_destroy(nivel->variable);
		}
		log_trace(logger, "Destrui Variables");

		void destroyItem(t_stack * nivel) {
			free(nivel);
		}
		list_remove_and_destroy_element(pcbActivo->indiceStack,
				pcbActivo->stackLevel, (void *) destroyItem);
		log_trace(logger, "Destrui TODO");

		pcbActivo->stackLevel--;

		log_trace(logger, "STACK LEVEL SIZE: %d",
				list_size(pcbActivo->indiceStack));

		log_info(logger, "Se reestablecio el Stack - Sigo en: %d", pcbActivo->programCounter);


	} else {
		pcbActivo->estadoEnCPU = CK_FINALIZO_EJECUCION;
	}
}

void wait(t_nombre_semaforo identificador_semaforo) {
	log_trace(logger, "Voy a hacer un WAIT");

	if(pcbActivo->volvioEntradaSalida == 0){

		pcbActivo->estadoEnCPU = CK_WAIT;

		voyAEntradaSalida = 1;
		pcbActivo->volvioEntradaSalida = 1;
		pcbActivo->nombreSemaforo = string_new();
		strcpy(pcbActivo->nombreSemaforo, identificador_semaforo);

		log_info(logger, " • Wait");
		respoderAlKernel(CK_WAIT);

	}else{
		voyAEntradaSalida = 0;
		pcbActivo->volvioEntradaSalida = 0;
	}
	pcbActivo->volvioEntradaSalida = 0;

	/*t_mensaje *mensaje = com_nuevoMensaje(CK_WAIT, logger);
	com_agregarIntAMensaje(mensaje, "pid", pcbActivo->pid, logger);
	com_agregarStringAMensaje(mensaje, "nombre_semaforo",
			identificador_semaforo, logger);

	log_trace(logger, "Mensaje que voy a enviar>>>> \n%s",
			com_imprimirMensaje(mensaje));

	com_enviarMensaje(mensaje, sockKernel, logger);

	log_trace(logger, "Voy a destruir el mensaje");
	com_destruirMensaje(mensaje, logger);

	log_trace(logger, "Voy a recibir un mensaje");
	t_mensaje *mensajeRecibido = com_recibirMensaje(sockKernel, logger);

	if (mensajeRecibido->header->tipo != KC_WAIT_RESPUESTA) {
		log_error(logger, "Error, el tipo de mensaje no es el que esperaba");
		//TODO return -1;
	}

	if (mensajeRecibido->header->len == 0) {
		log_error(logger, "Error, el mensaje deberia tener un body");
		//TODO return -1;
	}

	log_trace(logger, "Mensaje que recibi>>>> \n%s",
			com_imprimirMensaje(mensajeRecibido));

	log_trace(logger, "Parseo el mensaje");
	int resultado = com_leerIntAMensaje(mensajeRecibido, "resultado_respuesta",
			logger);

	if (resultado == 0) {
		pcbActivo->estadoEnCPU = CK_FINALIZO_CON_ERROR;
	}*/
}

void signalUP(t_nombre_semaforo identificador_semaforo) {
	log_trace(logger, "Voy a hacer un SIGNAL");

	t_mensaje *mensaje = com_nuevoMensaje(CK_SIGNAL, logger);
	com_agregarIntAMensaje(mensaje, "pid", pcbActivo->pid, logger);
	com_agregarStringAMensaje(mensaje, "nombre_semaforo",
			identificador_semaforo, logger);

	log_trace(logger, "Mensaje que voy a enviar>>>> \n%s",
			com_imprimirMensaje(mensaje));

	com_enviarMensaje(mensaje, sockKernel, logger);

	log_trace(logger, "Voy a destruir el mensaje");
	com_destruirMensaje(mensaje, logger);

	log_trace(logger, "Voy a recibir un mensaje");
	t_mensaje *mensajeRecibido = com_recibirMensaje(sockKernel, logger);

	if (mensajeRecibido->header->tipo != KC_SIGNAL_RESPUESTA) {
		log_error(logger, "Error, el tipo de mensaje no es el que esperaba");
		respoderAlKernel(CK_FINALIZO_CON_ERROR);
	}

	if (mensajeRecibido->header->len == 0) {
		log_error(logger, "Error, el mensaje deberia tener un body");
		//TODO return -1;
	}

	log_trace(logger, "Mensaje que recibi>>>> \n%s",
			com_imprimirMensaje(mensajeRecibido));

	log_trace(logger, "Parseo el mensaje");
	int resultado = com_leerIntAMensaje(mensajeRecibido, "resultado_respuesta",
			logger);

	if (resultado == 0) {
		pcbActivo->estadoEnCPU = CK_FINALIZO_CON_ERROR;
	}

}

void llamarSinRetorno(t_nombre_etiqueta etiqueta) {
	log_trace(logger, "Voy a llamar sin retorno");
	log_trace(logger, "LlamarFuncion %s || Donde Retorno: %d || ProgramCounter: %d", etiqueta, pcbActivo->programCounter);

//	pcbActivo->stackLevel++;
//
//	if(pcbActivo->stackLevel > pcbActivo->stackMaxSize){
//
//		pcbActivo->estadoEnCPU = CK_FINALIZO_STACKOVERFLOW;
//
//	} else{
//
//		t_stack* level = malloc(sizeof(t_stack));
//		level->id = (list_size(pcbActivo->indiceStack));
//		level->argumento = list_create();
//		level->variable = list_create();
//		level->retPos = pcbActivo->programCounter;
//		list_add(pcbActivo->indiceStack, level);


		irAFuncion(etiqueta); //TODO ir a funcion
//	}
}

void llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar) {
	log_trace(logger, "Voy a llamar con retorno");
	log_trace(logger, "LlamarFuncion %s || Donde Retorno: %d || ProgramCounter: %d", etiqueta, donde_retornar, pcbActivo->programCounter);

	pcbActivo->stackLevel++;

	if(pcbActivo->stackLevel > pcbActivo->stackMaxSize){

		pcbActivo->estadoEnCPU = CK_FINALIZO_STACKOVERFLOW;

	} else{

		t_stack* level = malloc(sizeof(t_stack));
		level->id = (list_size(pcbActivo->indiceStack));
		level->argumento = list_create();
		level->variable = list_create();
		level->retPos = pcbActivo->programCounter;
		level->retVar = punteroConvertirAPosicion(donde_retornar);
		list_add(pcbActivo->indiceStack, level);


		irAFuncion(etiqueta); //TODO ir a funcion
	}
}

void retornar(t_valor_variable retorno) {
	log_trace(logger, "Voy a retornar");

//	int index = list_size(pcbActivo->indiceStack) - 1;

	t_stack* item = (t_stack*) list_get(pcbActivo->indiceStack, pcbActivo->stackLevel);

	almacenarEnMemoriaUnValor(pcbActivo->pid, item->retVar, retorno, NULL, 1);

//		//Actualizar variable con retorno
//		t_puntero ptr = posicionConvertirAPuntero(item->retVar);
//		asignar(ptr, retorno);
//
//		if(abortar == 0){
//			pcbActivo->programCounter = item->retPos;
//
//			//stackpointer = de los argumentos el primero sino de variables (porque se calcula adelantado)
//			if(list_size(item->argumento) > 0)
//			{
//				t_posicion * arg = (t_posicion *) list_get(item->argumento, 0);
//				pcbActivo->stackPointer.pagina = arg->pagina;
//				pcbActivo->stackPointer.offset = arg->offset;
//				pcbActivo->stackPointer.size = arg->size;
//
//			}else if(list_size(item->variable) > 0){
//				t_vars_stack * var = (t_vars_stack *) list_get(item->variable, 0);
//				pcbActivo->stackPointer.pagina = var->posicion.pagina;
//				pcbActivo->stackPointer.offset = var->posicion.offset;
//				pcbActivo->stackPointer.size = var->posicion.size;
//			}

//			//Libero los recursos del contexto!!
//			if(list_size(item->argumento) > 0){
//				void liberarArgs(t_posicion* item){ free(item) ;}
//				list_destroy_and_destroy_elements(item->argumento, (void*)liberarArgs);
//			}else{
//				list_destroy(item->argumento);
//			}
//
//			if(list_size(item->variable) > 0){
//				void liberarVars(t_vars_stack* item){ free(item) ;}
//				list_destroy_and_destroy_elements(item->variable, (void*)liberarVars);
//			}else{
//				list_destroy(item->variable);
//			}
//
//			void destroyItem(t_stack * item){ free(item); }
//			list_remove_and_destroy_element(pcbActivo->indiceStack, index, (void *) destroyItem);
//
//			log_info(logger, "Se reestablecio el Stack - OK");

}

void capturarSigusr1() {
	log_info(logger, "Recibi SIGUSR1");
	desconectarse();
}

void desconectarse() {
	sigusr1 = 1;
}

int villereada(t_nombre_etiqueta nombre_etiqueta, int flag) {

	log_trace(logger, "Voy a hacer una villereada: %s", nombre_etiqueta);
	int contador = 0;
	t_codigo* pos;
	char* sentencia = NULL;


	if(pcbActivo->estadoEnCPU != EJECUTANDO){
		return -1;
	}

	if (flag == 0) { // Si flag == 0 entonces tengo que buscar donde esta el BEGIN para empezar
		for (contador = 0; contador < list_size(pcbActivo->indiceCodigo); contador++) {
			pos = list_get(pcbActivo->indiceCodigo, contador);

			sentencia = pedirAMemoriaString(pos->pagina, pos->offset, pos->size);
			if(strcmp(sentencia, "") == 0){
				return -1;
			}
			log_trace(logger, "Interprete la sentencia: [%s]", sentencia);

			if (string_equals_ignore_case(sentencia, nombre_etiqueta) == true) {

				return contador;

			}
		}

		return -1;
	} else {

		int eraUnaEtiqueta = 1;

		if(nombre_etiqueta != NULL){
			char* primerCaracter;
			primerCaracter = string_substring(nombre_etiqueta, 0, 1);
			char* dosPuntos = ":";

			if(string_equals_ignore_case(primerCaracter, dosPuntos) == true){
				log_trace(logger, "Voy a agregar [%s] a la lista de etiquetas", nombre_etiqueta);
				eraUnaEtiqueta = 0;

				t_indiceDeEtiquetas* nuevo = malloc(sizeof(t_indiceDeEtiquetas));
				nuevo->etiquetas = string_substring_from(nombre_etiqueta, 1);
				nuevo->ubicacion = pcbActivo->programCounter;

				list_add(pcbActivo->indiceEtiquetas, nuevo);
				log_trace(logger, "Termino de agregar [%s], listSize: [%d]", nuevo->etiquetas, list_size(pcbActivo->indiceEtiquetas));

				return pcbActivo->programCounter++;
			}
		}

		if(eraUnaEtiqueta == 1){
			log_trace(logger, "Size de Indice de Etiquetas: [%d]", list_size(pcbActivo->indiceEtiquetas));
			//Primero busco que no este en el indice de etiquetas
			if(list_size(pcbActivo->indiceEtiquetas) > 0){
				log_trace(logger, "Voy a buscar dentro de el Indice  de Etiquetas");

			for (contador = 0; contador < list_size(pcbActivo->indiceCodigo); contador++) {
				t_indiceDeEtiquetas* etiqueta = list_get(pcbActivo->indiceEtiquetas, contador);

				log_trace(logger, "Interprete la etiqueta: [%s]", etiqueta->etiquetas);

				if (string_equals_ignore_case(etiqueta->etiquetas, nombre_etiqueta) == true) {

					return etiqueta->ubicacion;

				}
			}
			}


			char* buffer = string_new();

			string_append_with_format(&buffer, "function %s", nombre_etiqueta);

			log_trace(logger, "Armo para comparar: [%s]", buffer);

			for (contador = 0; contador < list_size(pcbActivo->indiceCodigo); contador++) {
				pos = list_get(pcbActivo->indiceCodigo, contador);

				sentencia = pedirAMemoriaString(pos->pagina, pos->offset, pos->size);
				log_trace(logger, "Interprete la sentencia: [%s]", sentencia);

				if (string_equals_ignore_case(sentencia, buffer) == true) {

					free(buffer);
					return contador;

				}
			}
			free(buffer);
			return -1;
		}
	}

}

