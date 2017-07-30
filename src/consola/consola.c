/*
 * consola.c
 *
 *  Created on: 1/4/2017
 *      Author: utnso
 */
#include "consola.h"
t_log* LOGGER;
int pid;

int main(int argc, char **argv) {
	t_config *config;
	int error;
	listaPrograma = list_create();

	LOGGER = log_create("../logs/consola.log", "CONSOLA", true,
			LOG_LEVEL_INFO);
	log_info(LOGGER, ">>>>>>>>Inicio del proceso CONSOLA<<<<<<<<");

	signal(SIGINT, tratarCtrlC);

	if (argc != 2) {
		log_error(LOGGER,
				"Error en la cantidad de parametros: %d, (consola arch.cfg)",
				argc);
	}
	config = config_create(argv[1]);
	if (config == NULL) {
		log_error(LOGGER,
				"Error al abrir el archivo de configuracion: %s, (consola arch.cfg)",
				argv[1]);
	}

	log_info(LOGGER, ">>>>>>>>INICIO DE HILO<<<<<<<<");

	pid = fork();
	switch (pid) {
	case -1:
		perror("No se forkeo");
		break;
	case 0:
		iniciarFork(config);
		break;
	default:
		wait(pid);
		log_destroy(LOGGER);

	}
}

void listarProgramas() {
	int p;
	printf("Lista de Programas en ejecucion (%d)\n", list_size(listaPrograma));
	for (p = 0; p < list_size(listaPrograma); ++p) {
		t_programas *programa = list_get(listaPrograma, p);
		printf("PID: <%d> nombre: %s sock: %d state: %d\n", programa->pid,
				"para el proximo cuatri", programa->sock, programa->state);
	}

}
void iniciarFork(t_config* config) {
	int i;
	int c;
	char lineaComando[256];

	while (1) {
		printf("$comandoAnsiSop>");
		i = 0;
		c = getc(stdin);
		while (c != 10) {
			lineaComando[i++] = c;
			c = getc(stdin);
		}
		fflush(stdin);
		lineaComando[i] = '\0';

		int command = leerComando(lineaComando);
		switch (command) {
		case INICIAR_PROGRAMA:
			iniciarHiloPrograma(lineaComando, config);
			break;
		case FINALIZAR_PROGRAMA:
			finalizarPrograma(lineaComando);
			break;
		case DESCONECTAR_CONSOLA:
			desconectarConsola();
			break;
		case LIMPIAR_MENSAJES:
			limpiarConsola();
			break;
		case MANUAL:
			manuales();
			break;
		case LISTA:
			listarProgramas();
			break;

		default:
			errorComando();
			break;
		}
	}

}

void tratarCtrlC() {
	kill(pid, SIGKILL);
	log_info(LOGGER, ">>>>>>>>Fin del proceso CONSOLA fork<<<<<<<<");
	desconectarConsola();
}

int leerComando(char *lineaComando) {
	char separador[4] = " \t\n";
	char *comando = strtok(lineaComando, separador);

	if (string_equals_ignore_case(comando, "INICIO") == true) {
		return INICIAR_PROGRAMA;
	}
	if (string_equals_ignore_case(comando, "FINALIZAR") == true) {
		return FINALIZAR_PROGRAMA;
	}
	if (string_equals_ignore_case(comando, "DESCONECTAR") == true) {
		return DESCONECTAR_CONSOLA;
	}
	if (string_equals_ignore_case(comando, "LIMPIAR") == true) {
		return LIMPIAR_MENSAJES;
	}
	if (string_equals_ignore_case(comando, "MAN") == true) {
		return MANUAL;
	}
	if (string_equals_ignore_case(comando, "LISTA") == true) {
		return LISTA;
	}
	return -1;
}

void iniciarHiloPrograma(char *lineaComando, t_config *config) {
	char separador[4] = " \t\n";
	lineaComando = strtok(NULL, separador);
	if (lineaComando == NULL) {
		log_error(LOGGER,
				"Error: no hay parametros requiere ruta para INICIAR\n");
		return;
	}
	log_trace(LOGGER, "PARAMETROS>%s\n", lineaComando);
	char* programa = cargarPrograma(lineaComando);
	if (programa != NULL)
		crearHilo(config, programa, hiloPrograma);
	else
		log_error(LOGGER, "Error: NO SE ENCONTRO PROGRAMA>%s", lineaComando);
}

void *hiloPrograma(void *params) {
	int socket = 0;
	t_hilo_params_consol *p = (t_hilo_params_consol *) params;
	t_config *config = (t_config *) p->extra;
	char *programa = (char *) p->programa;
	t_list *listaHilos = (t_list *) p->listas;

	int pid = 0;

	socket = conectarProgramaConKernel(config);
	if (socket != -1) {
		pid = enviarMensaje(programa, socket);
	}

	//cargarProgramaEnLista(pid);
	t_programas *nodoPrograma = malloc(sizeof(t_programas));
	nodoPrograma->pid = pid;
	nodoPrograma->sock = socket;
	list_add(listaHilos, nodoPrograma);
	log_trace(LOGGER, "AGREGO pROCESO A LISTA >%d...\n",
			listaHilos->elements_count);

	esperoMensajesdeKernel(socket, LOGGER, listaHilos);

	//TODO mostrar resultado de finalizacion del proceso, bien o mal

	return NULL;
}

char* getMensajeError(int code) {
	char *mensajeError = malloc(100);
	switch (code) {
	case EXIT_CODE_CORRECTO:
		mensajeError = "El programa finalizo correctamente el proceso";
		break;
	case EXIT_CODE_ERROR_RESERVA_RECURSOS:
		mensajeError =
				"No se pudieron reservar recursos para ejecutar el programa";
		break;
	case EXIT_CODE_ERROR_ARCHIVO_INEXISTENTE:
		mensajeError = "El programa intento acceder a un archivo que no existe";
		break;
	case EXIT_CODE_ERROR_LEER_SIN_PERMISO:
		mensajeError = "El programa intento leer un archivo sin permisos";
		break;
	case EXIT_CODE_ERROR_ESCRIBIR_SIN_PERMISO:
		mensajeError = "El programa intento escribir un archivo sin permisos";
		break;
	case EXIT_CODE_ERROR_EXCEPCION_MEMORIA:
		mensajeError = "Excepcion de memoria";
		break;
	case EXIT_CODE_ERROR_DESCONEXION_CONSOLA:
		mensajeError = "Finalizado a traves de desconexion de consola";
		break;
	case EXIT_CODE_ERROR_COMANDO_FINALIZAR:
		mensajeError =
				"Finalizado a traves del comando Finalizar Programa de la consola";
		break;
	case EXIT_CODE_ERROR_LIMITE_PAGINA:
		mensajeError =
				"Se intento reservar mas memoria que el tamaño de la pagina";
		break;
	case EXIT_CODE_ERROR_PAGINAS_DISPONIBLES:
		mensajeError = "No se pueden asignar mas paginas al proceso";
		break;
	case EXIT_CODE_ERROR_STACKOVERFLOW:
		mensajeError = "El programa finalizo por STACK OVERFLOW";
		break;
	case EXIT_CODE_ERROR_SIN_DEFINICION:
		mensajeError = "Error sin definicion";
		break;
	default:
		mensajeError = "Entre en el Default";
	}
	return mensajeError;
}

int esperoMensajesdeKernel(int socket, t_log* LOGGER, t_list * lista) {
	t_mensaje *respuestaConsola;
	int32_t codigo;
	char* texto;
	t_mensaje *respuestaKernel = com_recibirMensaje(socket, LOGGER);
	int finDeEspera = 1;
	while (finDeEspera) {
		switch (respuestaKernel->header->tipo) {
		case PK_FINALIZO_PROGRAMA:
			log_trace(LOGGER, "FINALIZO El programa...");
			codigo = com_leerIntAMensaje(respuestaKernel, "codigo_retorno",
					LOGGER);
			log_trace(LOGGER, "EL PROCESO FINALIZO CON RESPUESTA:%d", codigo);
			texto = getMensajeError(codigo);
			printf("FINALIZO PROGRAMA >> ::::%s\n\n", texto);
			int i;
			for (i = 0; i < list_size(listaPrograma); ++i) {
				t_programas *p = list_get(lista, i);
				if (p->sock == socket) {
					list_remove(lista, i);
					log_trace(LOGGER, "Elimine programa de la lista");
					break;
				}
			}
			finDeEspera = 0;
			break;
		case PK_PRINT_CONSOLA:
			log_trace(LOGGER, "MENSAJE DE PRINT EN PANTALLA");
			texto = (char *) com_leerStringAMensaje(respuestaKernel, "texto",
					LOGGER);
			printf("MENSAJE >> ::::%s\n\n", texto);
			break;
		default:
			break;
		}
		respuestaKernel = com_recibirMensaje(socket, LOGGER);
	}
	return 1;
}
void crearHilo(void *extraParams, void *programa, void *(*funcion)(void *)) {
	t_hilo_params_consol *params = malloc(sizeof(t_hilo_params_consol));
	params->extra = extraParams;
	params->programa = programa;
	params->listas = listaPrograma;

	pthread_t newDispatcher;
	pthread_create(&newDispatcher, NULL, funcion, params);

	log_trace(LOGGER, "TENGO RETURN: %d", list_size(listaPrograma));
}

int enviarMensaje(char *programa, int socket) {
	t_mensaje *mensaje = com_nuevoMensaje(PK_ENVIAR_PROGRAMA, LOGGER);
	com_agregarStringAMensaje(mensaje, "script", programa, LOGGER);
	com_enviarMensaje(mensaje, socket, LOGGER);
	com_destruirMensaje(mensaje, LOGGER);

	t_mensaje *mensajeRetorno = com_recibirMensaje(socket, LOGGER);

	log_trace(LOGGER, "RECIBI RESPUESTA DEL KERNEL: %s",
			com_imprimirMensaje(mensajeRetorno));
	if (mensajeRetorno->header->tipo != PK_RESPUESTA_PROGRAMA_OK) {
		log_error(LOGGER,
				"Error, el tipo de mensaje no es el que esperaba (PID)");
		return -1;
	}

	int32_t pid = com_leerIntAMensaje(mensajeRetorno, "pid", LOGGER);

	log_trace(LOGGER, "MI PID Es %d", pid);

	return pid;
}

void finalizarPrograma(char *lineaComando) {
	log_info(LOGGER, "$Ejecutando>Finalizar: %s\n", lineaComando);
	log_trace(LOGGER, "Cantidad de programas en lista:%d...\n",
			list_size(listaPrograma));
	char separador[4] = " \t\n";
	int pid;
	lineaComando = strtok(NULL, separador);
	if (lineaComando == NULL) {
		log_error(LOGGER, "Error: no hay parametros de PID para finalizar\n");
		return;
	}

	pid = atoi(lineaComando);
	log_trace(LOGGER, "FINALIZANDO PROCESO >%d...\n", pid);
	bool validaPid(void* proceso) {
		return ((t_programas*) proceso)->pid == pid;
	}

	t_programas *programa = list_find(listaPrograma, validaPid);

	if (avisar_al_kernel_que_ya_corte_con(programa) == 1)
		return;
}

int avisar_al_kernel_que_ya_corte_con(t_programas *programa) {
	log_trace(LOGGER, "MENSAJE DE FIN");
	t_mensaje *mensaje = com_nuevoMensaje(PK_FINALIZAR_PROGRAMA, LOGGER);
	com_agregarIntAMensaje(mensaje, "pid", programa->pid, LOGGER);
	com_enviarMensaje(mensaje, programa->sock, LOGGER);
	com_destruirMensaje(mensaje, LOGGER);
	return 1;
}

void desconectarConsola() {
	int i = 0;
	log_trace(LOGGER, "$Ejecutando>Desconexion de la consola\n");
	log_trace(LOGGER, "Lista de programa %d\n", listaPrograma->elements_count);
	for (i = 0; i < listaPrograma->elements_count; i++) {
		avisar_al_kernel_que_ya_corte_con(list_get(listaPrograma, i));
	}
}
void limpiarConsola() {
	printf("\033[H\033[J");
	log_trace(LOGGER, "$Ejecutando> limpieza de consola\n");
}

void errorComando() {
	log_error(LOGGER, "$error> Comando no valido\n");
}

void manuales() {
	log_trace(LOGGER, "Ejecutando Manual ANSISop");
	printf("***************MANUAL ANSISop***************\n");
	printf(
			"*1) INICIO [URL del programa] Lee y envia un programa al kernel. Ejemplo: INICIO ../programa/program1 \n");
	printf(
			"*2) FINALIZAR [PID del programa]: Envia un mensaje al kernel de finalizar un programa. Ejemplo: FINALIZAR 3\n");
	printf(
			"*3) DESCONECTAR: Desconecta la consola terminando los procesos en ejecución\n");
	printf("*4) LIMPIAR :Limpia la pantalla\n");
	printf("*5) MAN: Muestra este manual\n");
	printf(
			"*6) LISTA: Lista los programas que estan corriendo en esta consola\n");
}

int conectarProgramaConKernel(t_config *config) {

	char *iP = config_get_string_value(config, "IP_KERNEL");
	int puerto = config_get_int_value(config, "PUERTO_KERNEL");

	int sockKernel = com_conectarseA(iP, puerto);
	if (sockKernel <= 0) {
		log_error(LOGGER, "Error al conectarse al kernel IP: %s PUERTO: %d", iP,
				puerto);
		return -1;
	}
	log_trace(LOGGER, "Conectando a %s> Puerto> %d \n", iP, puerto);

	if (!com_prgker_handshake_programa(sockKernel, LOGGER)) {
		log_error(LOGGER, "Error en hadshake en sock: %d", sockKernel);
		return -1;
	}
	return sockKernel;
}

char* cargarPrograma(char* parametro) {
	FILE* archivo = fopen(parametro, "r");
	if (archivo == 0)
		return NULL;
	log_trace(LOGGER, "Cargando programa: %s...\n", parametro);
	int tamanio = tamanioArchivo(archivo);
	log_trace(LOGGER, "Tamanio: %d bytes\n", tamanio);
	if (tamanio < 0)
		return NULL;
	char* programa = malloc(tamanio + 1);
	fread(programa, 1, tamanio, archivo);
	programa[tamanio] = '\0';
	return programa;
}

int tamanioArchivo(FILE* fp) {
	struct stat st;
	if (fp == NULL)
		return -1;

	fstat(fileno(fp), &st);
	return st.st_size;
}

