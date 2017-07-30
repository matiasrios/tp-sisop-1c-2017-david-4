/*
 * kernel.c
 *
 *  Created on: 1/4/2017
 *      Author: utnso
 */
#include "kernel.h"

uint32_t gradoMultiprogramacion;
uint32_t procesosEnSistema = 0;

uint32_t tamanioPagina = 0;

pthread_t hiloConsolaKernel;
int sockMemoria;
int sockFS;
int sistemaPausado = 0;
pthread_mutex_t mutexSistemaPausado;

t_log* logger;
t_listas *listas;
char* algoritmo;
uint32_t stackMaxSize;

int main(int argc, char **argv) {

	logger = log_create("../logs/kernel.log", "KERNEL", true, LOG_LEVEL_INFO);

	log_info(logger, ">>>>>>>>Inicio del proceso KERNEL<<<<<<<<");

	t_config *config;
	if (argc != 2) {
		log_error(logger,
				"Error en la cantidad de parametros: %d, (kernel arch.cfg)",
				argc);
	}

	pthread_mutex_init(&mutexSistemaPausado, NULL);

	log_info(logger, "LEER CONFIG");
	config = config_create(argv[1]);
	log_info(logger, "LEIDO EL CONFIG");
	if (config == NULL) {
		log_error(logger,
				"Error al abrir el archivo de configuracion: %s, (kernel arch.cfg)",
				argv[1]);
	}

	uint32_t quantum = config_get_int_value(config, "QUANTUM");
	uint32_t quantumSleep = config_get_int_value(config, "QUANTUM_SLEEP");
	stackMaxSize = config_get_int_value(config, "STACK_SIZE");
	algoritmo = config_get_string_value(config, "ALGORITMO");
	gradoMultiprogramacion = config_get_int_value(config, "GRADO_MULTIPROG");

	listas = inicializarListas(listas);

	char **variables = config_get_array_value(config, "SHARED_VARS");
	int i;
	for (i = 0; variables[i] != NULL; i++) {
		t_var_comp *var = malloc(sizeof(t_var_comp));
		var->nombre = malloc(100);
		strcpy(var->nombre, variables[i]);
		var->valor = 0;
		list_add(listas->variablesComp->lista, var);
	}
	char **semaforos_comp = config_get_array_value(config, "SEM_IDS");
	char **init_semaforos_comp = config_get_array_value(config, "SEM_INIT");
	for (i = 0; semaforos_comp[i] != NULL; i++) {
		t_var_comp *sem = malloc(sizeof(t_var_comp));
		sem->nombre = malloc(100);
		strcpy(sem->nombre, semaforos_comp[i]);
		sem->valor = atoi(init_semaforos_comp[i]);
		sem->pilaEspera = queue_create();
		list_add(listas->semaforosComp->lista, sem);
	}

	int listenerCPUs = com_escucharEn(
			config_get_int_value(config, "PUERTO_CPU"));
	if (listenerCPUs <= 0) {
		log_error(logger, "Error al crear listener para las cpus en PUERTO: %d",
				config_get_int_value(config, "PUERTO_CPU"));
	}
	log_info(logger, "Se creo el listener para cpus en el sock: %d",
			listenerCPUs);

	int listenerProgramas = com_escucharEn(
			config_get_int_value(config, "PUERTO_PROG"));
	if (listenerCPUs <= 0) {
		log_error(logger,
				"Error al crear listener para los programas en PUERTO: %d",
				config_get_int_value(config, "PUERTO_PROG"));
		return -1;
	}
	log_info(logger, "Se creo el listener para programas en el sock: %d",
			listenerProgramas);

	sockMemoria = com_conectarseA(config_get_string_value(config, "IP_MEMORIA"),
			config_get_int_value(config, "PUERTO_MEMORIA"));
	if (sockMemoria <= 0) {
		log_error(logger, "Error al conectarse a la memoria IP: %s PUERTO: %d",
				config_get_string_value(config, "IP_MEMORIA"),
				config_get_int_value(config, "PUERTO_MEMORIA"));
		return -1;
	}

	if (!com_kermem_handshake_kernel(sockMemoria, logger)) {
		log_error(logger, "Error en el handshake con la memoria sock: %d",
				sockMemoria);
		return -1;
	}
	log_info(logger, "Me conecte a la memoria en el sock: %d", sockMemoria);

	sockFS = com_conectarseA(config_get_string_value(config, "IP_FS"),
			config_get_int_value(config, "PUERTO_FS"));
	if (sockFS <= 0) {
		log_error(logger,
				"Error al conectarse a el file system IP: %s PUERTO: %d",
				config_get_string_value(config, "IP_FS"),
				config_get_int_value(config, "PUERTO_FS"));
		return -1;
	}

	if (!com_kerfs_handshake_kernel(sockFS, logger)) {
		log_error(logger, "Error en el handshake con el file system sock: %d",
				sockFS);
		return -1;
	}
	log_info(logger, "Me conecte al file system en el sock: %d", sockFS);

	pthread_create(&hiloConsolaKernel, NULL, (void *) consolaKernel, NULL);

	uint32_t numeroPid = 1;
	fd_set master;
	fd_set readfds;
	FD_ZERO(&master);
	FD_ZERO(&readfds);
	FD_SET(0, &master);
	FD_SET(listenerCPUs, &master);
	FD_SET(listenerProgramas, &master);
	int maxSock = 100; // deberia ser un numero mayor al de todos los descriptores
	while (1) {
		log_trace(logger, "esperando eventos \n");
		readfds = master;
		select(maxSock, &readfds, NULL, NULL, NULL);
		log_trace(logger, "me llego algo \n");
		if (sistemaPausado) {
			pthread_mutex_lock(&mutexSistemaPausado);
		}
		if (FD_ISSET(listenerCPUs, &readfds)) {
			log_trace(logger, "Se conecto un CPU");
			int sock = nuevoCPU(listenerCPUs, listas);
			FD_SET(sock, &master);
			if (sock > 0 && sock > maxSock) {
				maxSock = sock;
			}
			log_trace(logger, "Se agrego un nuevo CPU");
		}
		if (FD_ISSET(listenerProgramas, &readfds)) {
			int sock = nuevoPrograma(listenerProgramas, listas, numeroPid,
					quantum, quantumSleep);
			numeroPid++;
			FD_SET(sock, &master);
			if (sock > 0 && sock > maxSock) {
				maxSock = sock;
			}
		}
		int i;
		log_trace(logger,
				"Voy a ver si un programa me esta enviando algo, size: %d",
				list_size(listas->programas->lista));
		for (i = 0; i < list_size(listas->programas->lista); i++) {
			t_pcb *programa = (t_pcb *) list_get(listas->programas->lista, i);
			if (FD_ISSET(programa->sock, &readfds)) {
				log_trace(logger,
						"El programa en el sock: %d me envio un mensaje",
						programa->sock);
				t_mensaje *mensaje = com_recibirMensaje(programa->sock, logger);
				if (mensaje != NULL) {
					switch (mensaje->header->tipo) {
					case PK_FINALIZAR_PROGRAMA /* MENSAJES DEL PROGRAMA - FINALIZAR */:
						finalizarPrograma(listas, programa->sock,
								com_leerIntAMensaje(mensaje, "pid", logger),EXIT_CODE_ERROR_COMANDO_FINALIZAR);
						break;
					default:
						break;
					}
				} else {
					FD_CLR(programa->sock, &master);
					log_trace(logger, "Se desconecto el programa en el sock %d",
							programa->sock);
					finalizarPrograma(listas, -1, programa->pid,EXIT_CODE_ERROR_COMANDO_FINALIZAR);
				}
			}
		}
		log_trace(logger, "Voy a ver si un cpu me esta enviando algo, size: %d",
				list_size(listas->cpus->lista));
		for (i = 0; i < list_size(listas->cpus->lista); ++i) {
			t_cpu *cpu = (t_cpu *) list_get(listas->cpus->lista, i);
			if (FD_ISSET(cpu->sock, &readfds)) {
				log_trace(logger, "El cpu en el sock: %d me envio un mensaje",
						cpu->sock);
				log_trace(logger, "Voy a recv el mensaje");
				t_mensaje *mensaje = com_recibirMensaje(cpu->sock, logger);
				if (mensaje != NULL) {
					log_trace(logger,
							"Recibi el mensaje, voy al switch - tipo: %d",
							mensaje->header->tipo);
					bool actPCB = true;
					int pid = com_leerIntAMensaje(mensaje, "pid", logger);
					int posicion;
					int espacio;
					char *nombre;
					int valor;
					int descriptorEscribir;
					t_mensaje *respuesta;
					bool esVarComp(t_var_comp* var) {
						//log_trace(logger, "el nombre: %s", var->nombre);
						//log_trace(logger, "el valor: %d", var->valor);
						return strcmp(var->nombre, nombre) == 0;
					}
					t_var_comp* var;
					t_cpu *cpuSem;
					bool esPidBuscado(int* pidBuscado) {
						return (*pidBuscado) == pid;
					}

					t_pcb *pcbCPU = (t_pcb*) list_find(listas->programas->lista,
							(void *) esPidBuscado);
					if (pcbCPU != NULL) {
						switch (mensaje->header->tipo) {
						case CK_FINALIZO_EJECUCION /* MENSAJES DEL CPU - FINALIZO EJECUCION */:
							log_trace(logger,
									">>>>>>FINALIZO PROGRAMAAAAAAAAAA!!!!!!!!!!!!");
							moverProgramaDeExecAExit(listas, pid, sockMemoria);
							cpu->libre = 1;
							break;
						case CK_FINALIZO_QUANTUM /* MENSAJES DEL CPU - FINALIZO QUANTUM */:
							log_trace(logger, ">>>>>>TERMINO QUANTUM!!!!!");
							moverProgramaDeExecAReady(listas, pid);
							cpu->libre = 1;
							break;
						case CK_PIDIO_ABRIR_ARCHIVO /* MENSAJES DEL CPU - PIDIO IO */:
							pedidoDeAbrirArchivo(listas,
									com_leerIntAMensaje(mensaje, "pid", logger),
									com_leerStringAMensaje(mensaje, "direccion",
											logger));
							pcbCPU->cantInstPriv++;
							pcbCPU->cantSyscalls++;
							pcbCPU->operacionES = OPERACION_ABRIR_ARCHIVO;
							cpu->libre = 1;
							break;
						case CK_PIDIO_LEER_ARCHIVO /* MENSAJES DEL CPU - PIDIO IO */:
							pedidoDeLeerArchivo(listas,
									com_leerIntAMensaje(mensaje, "pid", logger),
									com_leerIntAMensaje(mensaje, "descriptor",
											logger),
									com_leerIntAMensaje(mensaje, "offsetArch",
											logger),
									com_leerIntAMensaje(mensaje, "tamAEscribir",
											logger));
							pcbCPU->cantInstPriv++;
							pcbCPU->cantSyscalls++;
							pcbCPU->operacionES = OPERACION_LEER_ARCHIVO;
							cpu->libre = 1;
							break;
						case CK_PIDIO_ESCRIBIR_ARCHIVO /* MENSAJES DEL CPU - PIDIO IO */:
							pedidoEscribirArchivo(listas,
									com_leerIntAMensaje(mensaje, "pid", logger),
									com_leerIntAMensaje(mensaje, "descriptor",
											logger),
									com_leerIntAMensaje(mensaje, "offsetArch",
											logger),
									com_leerStringAMensaje(mensaje,
											"infoAEscribir", logger));
							pcbCPU->cantInstPriv++;
							pcbCPU->cantSyscalls++;
							pcbCPU->operacionES = OPERACION_ESCRIBIR_ARCHIVO;
							cpu->libre = 1;
							break;
						case CK_PIDIO_CERRAR_ARCHIVO /* MENSAJES DEL CPU - PIDIO IO */:
							pedidoCerrarArchivo(listas,
									com_leerIntAMensaje(mensaje, "pid", logger),
									com_leerIntAMensaje(mensaje, "descriptor",
											logger));
							pcbCPU->cantInstPriv++;
							pcbCPU->cantSyscalls++;
							pcbCPU->operacionES = OPERACION_CERRAR_ARCHIVO;
							cpu->libre = 1;
							break;
						case CK_PIDIO_BORRAR_ARCHIVO /* MENSAJES DEL CPU - PIDIO IO */:
							pedidoBorrarArchivo(listas,
									com_leerIntAMensaje(mensaje, "pid", logger),
									com_leerIntAMensaje(mensaje, "descriptor",
											logger));
							pcbCPU->cantInstPriv++;
							pcbCPU->cantSyscalls++;
							pcbCPU->operacionES = OPERACION_BORRAR_ARCHIVO;
							cpu->libre = 1;
							break;
						case CK_FINALIZO_CON_ERROR /* MENSAJES DEL CPU - ERROR EJECUCION */:
							pcbCPU->exitCode =
									EXIT_CODE_ERROR_EXCEPCION_MEMORIA;
							moverProgramaDeExecAExit(listas, pid, sockMemoria);
							cpu->libre = 1;
							break;
						case CK_FINALIZO_STACKOVERFLOW /* MENSAJES DEL CPU - ERROR EJECUCION */:
							pcbCPU->exitCode = EXIT_CODE_ERROR_STACKOVERFLOW;
							moverProgramaDeExecAExit(listas, pid, sockMemoria);
							cpu->libre = 1;
							break;
						case CK_ALOCAR /* RESERVAR MEMORIA HEAP */:
							if (reservarMemoriaHeap(cpu, pcbCPU, mensaje,
									sockMemoria, logger, tamanioPagina) == -1) {
								log_error(logger,"Error al reservar memoria heap");
								finalizarPrograma(listas,-1,pcbCPU->pid,EXIT_CODE_ERROR_LIMITE_PAGINA);
								cpu->libre = 1;
							} else {
								actPCB = false;
								pcbCPU->cantInstPriv++;
							}
							break;
						case CK_LIBERAR /* LIBERAR MEMORIA HEAP */:
							liberarMemoriaHeap(cpu, pcbCPU, mensaje,
									sockMemoria, logger);
							actPCB = false;
							pcbCPU->cantInstPriv++;
							break;
						case CK_WAIT /* WAIT SEMAFORO */:
							nombre = com_leerStringAMensaje(mensaje,
									"nombre_semaforo", logger);
							pcbCPU->nombreSemaforo = string_new();
							strcpy(pcbCPU->nombreSemaforo, nombre);
							pcbCPU->operacionES = OPERACION_WAIT_SEMAFORO;
							moverProgramaDeExecABlocked(listas, pid);
							pcbCPU->cantInstPriv++;
							break;
						case CK_SIGNAL /* SIGNAL SEMAFORO */:
							nombre = com_leerStringAMensaje(mensaje,
									"nombre_semaforo", logger);
							pcbCPU->nombreSemaforo = string_new();
							strcpy(pcbCPU->nombreSemaforo, nombre);
							//pcbCPU->operacionES = OPERACION_SIGNAL_SEMAFORO;
							log_trace(logger,
									"Me enviaron un signal del semaforo <%s>",
									nombre);
							var = (t_var_comp*) list_find(
									listas->semaforosComp->lista,
									(void*) esVarComp);
							if (var != NULL) {
								respuesta = com_nuevoMensaje(
										KC_SIGNAL_RESPUESTA, logger);
								com_agregarIntAMensaje(respuesta,
										"resultado_respuesta", 1, logger);
								com_enviarMensaje(respuesta, cpu->sock, logger);
								com_destruirMensaje(respuesta, logger);
								var->valor++;
								if (queue_size(var->pilaEspera) > 0) {
									var->valor--;
									t_pcb *pcbEsperando = queue_pop(
											var->pilaEspera);
									pcbEsperando->exitCode = TERMINO_ESPERA;
								}
							} else {
								log_trace(logger,
										"No se encontro el semaforo pedido");
								respuesta = com_nuevoMensaje(
										KC_SIGNAL_RESPUESTA, logger);
								com_agregarIntAMensaje(respuesta,
										"resultado_respuesta", 0, logger);
								com_enviarMensaje(respuesta, cpuSem->sock,
										logger);
								com_destruirMensaje(respuesta, logger);
							}
							actPCB = false;
							pcbCPU->cantInstPriv++;
							break;
						case CK_VARIABLE_LECTURA /* CONSULTA VARIABLE COMPARTIDA */:
							nombre = com_leerStringAMensaje(mensaje, "variable",
									logger);
							log_trace(logger,
									"El proceso consulto la variable <%s>",
									nombre);
							respuesta = com_nuevoMensaje(
									KC_VARIABLE_LECTURA_RESPUESTA, logger);
							var = (t_var_comp*) list_find(
									listas->variablesComp->lista,
									(void*) esVarComp);
							if (var != NULL) {
								com_agregarIntAMensaje(respuesta,
										"resultado_respuesta", 1, logger);
								com_agregarIntAMensaje(respuesta, "valor",
										var->valor, logger);
								log_trace(logger, "El mensajeeeeeee: %d",
										var->valor);
							} else {
								com_agregarIntAMensaje(respuesta,
										"resultado_respuesta", 0, logger);
								com_agregarIntAMensaje(respuesta, "valor", 0,
										logger);
							}

							com_enviarMensaje(respuesta, cpu->sock, logger);
							com_destruirMensaje(respuesta, logger);
							actPCB = false;
							pcbCPU->cantInstPriv++;
							break;
						case CK_VARIABLE_ESCRITURA /* ESCRIBIR VARIABLE COMPARTIDA */:
							nombre = com_leerStringAMensaje(mensaje,
									"nombre_variable", logger);
							valor = com_leerIntAMensaje(mensaje, "valor",
									logger);
							log_trace(logger,
									"El proceso consulto la variable <%s>",
									nombre);
							respuesta = com_nuevoMensaje(
									KC_VARIABLE_ESCRITURA_RESPUESTA, logger);
							var = (t_var_comp*) list_find(
									listas->variablesComp->lista,
									(void*) esVarComp);
							if (var != NULL) {
								var->valor = valor;
								com_agregarIntAMensaje(respuesta,
										"resultado_respuesta", 1, logger);
							} else {
								com_agregarIntAMensaje(respuesta,
										"resultado_respuesta", 0, logger);
							}
							com_enviarMensaje(respuesta, cpu->sock, logger);
							com_destruirMensaje(respuesta, logger);
							actPCB = false;
							pcbCPU->cantInstPriv++;
							break;
						default:
							break;
						}
						if (actPCB) {
							int pid = com_leerIntAMensaje(mensaje, "pid",
									logger);
							int p;
							t_pcb *pcb;
							for (p = 0; p < list_size(listas->programas->lista);
									p++) {
								pcb = list_get(listas->programas->lista, p);
								if (pcb->pid == pid) {
									break;
								}
							}

							actualizarPCB(pcb, mensaje);
							cpu->activo = com_leerIntAMensaje(mensaje,
									"continua_activo", logger);
							cpu->libre = 1;
						}
					} else {
						respuesta = com_nuevoMensaje(KC_FINALIZO_PROGRAMA,
								logger);
						com_agregarIntAMensaje(respuesta, "resultado_respuesta",
								0, logger);
						com_enviarMensaje(respuesta, cpu->sock, logger);
						com_destruirMensaje(respuesta, logger);
					}
				} else {
					FD_CLR(cpu->sock, &master);
					log_trace(logger, "Se desconecto un CPU del sock: %d",
							cpu->sock);
					list_remove(listas->cpus->lista, i);
				}
			}
		}

		log_trace(logger, "Antes de entrar a actualizar");
		actualizarEstadoSistema();
	}

	pthread_mutex_destroy(&mutexSistemaPausado);
	pthread_join(hiloConsolaKernel, NULL);

	finalizarListar(listas);

	log_info(logger, ">>>>>>>>Fin del proceso KERNEL<<<<<<<<");
	log_destroy(logger);

	return 0;
}

void actualizarEstadoSistema() {

	log_trace(logger, "Voy a actualizar estado del sistema");
	log_trace(logger, "Atiendo procesos en Blocked <%d>",
			list_size(listas->blocked->lista));
	int i;
	for (i = 0; i < list_size(listas->blocked->lista); ++i) {
		t_pcb *proceso = list_get(listas->blocked->lista, i);
		if (proceso->exitCode == TERMINO_ESPERA) {
			proceso->exitCode = EXIT_CODE_CORRECTO;
		} else if (proceso->exitCode != ESPERANDO_SEMAFORO) {
			atenderES(listas, proceso, sockFS);

			if (proceso->exitCode == EXIT_CODE_CORRECTO) {
				list_remove(listas->blocked->lista, i);
				list_add(listas->ready->lista, proceso);
			} else if (proceso->exitCode != ESPERANDO_SEMAFORO) {
				// TODO OCURRIO ALGUN ERROR DE E/S
				finalizarProgramaConError(listas->blocked->lista,
						listas->exit->lista, proceso);
				log_trace(logger,
						"Finalizar el proceso con pid %d por el error %d",
						proceso->pid, proceso->exitCode);
			}
		}

	}
	log_trace(logger, "Atiendo procesos en New - Grado Multiprogramacion: <%d>",
			gradoMultiprogramacion);
	log_trace(logger, "Procesos en sistema: <%d>", procesosEnSistema);
	for (i = 0; i < list_size(listas->new->lista); ++i) {
		if (gradoMultiprogramacion > procesosEnSistema) {
			t_pcb *proceso = list_get(listas->new->lista, i);
			reservarMemoriaParaProceso(proceso, sockMemoria);
			list_remove(listas->new->lista, i);
			list_add(listas->ready->lista, proceso);
			procesosEnSistema++;
		}
	}
	log_trace(logger, "Veo si puedo enviar procesos a ejecucion: %d",
			list_size(listas->cpus->lista));
	for (i = 0; i < list_size(listas->cpus->lista); i++) {
		t_cpu *cpu = list_get(listas->cpus->lista, i);
		log_trace(logger, "CPU ACTIVO: [%d]\nCPU LIBRE: [%d]\nCANT READY: [%d]",
				cpu->activo, cpu->libre, list_size(listas->ready->lista));
		if (cpu->activo && cpu->libre && list_size(listas->ready->lista) > 0) {
			t_pcb *proceso = list_get(listas->ready->lista, 0);
			list_add(listas->exec->lista, proceso);
			enviarProcesoACPU(proceso, cpu);
			log_trace(logger, "Envie el proceso al CPU: %d", cpu->sock);
			list_remove(listas->ready->lista, 0);
			cpu->libre = 0;
		}
	}
	log_trace(logger, "Termine actualizar sistema");
}

void moverProgramaDeExecAExit(t_listas *listas, int32_t pid, int sockMemoria) {
	log_trace(logger,
			"El proceso <%d> finalizo y voy a liberar memoria y enviarlo a exit.",
			pid);
	int i;
	for (i = 0; i < list_size(listas->exec->lista); i++) {
		t_pcb *proceso = list_get(listas->exec->lista, i);
		if (proceso->pid == pid) {
			list_remove(listas->exec->lista, i);
			mensajeAMemoriaFinalizado(pid, 10, sockMemoria);
			mensajeAConsolaFinalizo(proceso->exitCode, proceso->sock);
			list_add(listas->exit->lista, proceso);
			procesosEnSistema--;
			return;
		}
	}
}

void moverProgramaDeExecAReady(t_listas *listas, int32_t pid) {
	log_trace(logger, "Entre a la funcion");
	int i;

	for (i = 0; i < list_size(listas->exec->lista); i++) {

		t_pcb *proceso = list_get(listas->exec->lista, i);
		log_trace(logger, "PID: %d", proceso->pid);
		if (proceso->pid == pid) {
			list_remove(listas->exec->lista, i);
			log_trace(logger, "REMOVE");
			list_add(listas->ready->lista, proceso);
			log_trace(logger, "ADD");
			return;
		}
	}
}

void moverProgramaDeExecABlocked(t_listas *listas, int32_t pid) {
	log_trace(logger, "Envio el proceso con pid >%d< a bloqued", pid);
	int i;
	for (i = 0; i < list_size(listas->exec->lista); i++) {
		t_pcb *proceso = list_get(listas->exec->lista, i);
		if (proceso->pid == pid) {
			list_remove(listas->exec->lista, i);
			list_add(listas->blocked->lista, proceso);
			return;
		}
	}
}

void pedidoDeAbrirArchivo(t_listas *listas, int32_t pid, char *pathArchivo) {
	// buscar el pcb
	// al pcb le guardas el nombre de archivo que se esta pidiendo
	moverProgramaDeExecABlocked(listas, pid);
}

void pedidoDeLeerArchivo(t_listas *listas, int32_t pid, int32_t descriptor,
		int32_t posInicio, int32_t largo) {

	moverProgramaDeExecABlocked(listas, pid);
}

void pedidoEscribirArchivo(t_listas *listas, int32_t pid, int32_t descriptor,
		int32_t posInicio, char *texto) {

	moverProgramaDeExecABlocked(listas, pid);
}

void pedidoCerrarArchivo(t_listas *listas, int32_t pid, int32_t descriptor) {

	moverProgramaDeExecABlocked(listas, pid);
}

void pedidoBorrarArchivo(t_listas *listas, int32_t pid, int32_t descriptor) {

	moverProgramaDeExecABlocked(listas, pid);
}

void finalizarProgramaConError(t_list *origen, t_list *exit, t_pcb *pcb) {
	log_trace(logger,
			"El proceso %d finalizo incorrectamente con el codigo de error: %d",
			pcb->pid, pcb->exitCode);
	int i;
	for (i = 0; i < list_size(origen); i++) {
		t_pcb *proceso = list_get(origen, i);
		if (proceso->pid == pcb->pid) {
			list_remove(origen, i);
			//proceso->exitCode = codigoError;
			list_add(exit, proceso);
			mensajeAMemoriaFinalizado(pcb->pid, 10, sockMemoria);
			mensajeAConsolaFinalizo(pcb->exitCode, pcb->sock);
			procesosEnSistema--;
			return;
		}
	}
}

void mensajeAMemoriaFinalizado(int pid, int page, int sockMemoria) {
	log_trace(logger, "AVISO A MEMORIA QUE FINALIZE pid: %d", pid);
	t_mensaje *mensajeFinalizo = com_nuevoMensaje(KM_FIN_PROCESO, logger);
	com_agregarIntAMensaje(mensajeFinalizo, "pid", pid, logger);
	com_agregarIntAMensaje(mensajeFinalizo, "numPag", page, logger);
	com_enviarMensaje(mensajeFinalizo, sockMemoria, logger);
	com_destruirMensaje(mensajeFinalizo, logger);

	log_trace(logger, "Voy a esperar la respuesta");
	t_mensaje *respuesta = com_recibirMensaje(sockMemoria, logger);
	log_trace(logger, "Me llego");
	if (respuesta->header->tipo != MK_FIN_PROCESO_RESPUESTA) {
		log_error(logger, "Error al liberar la memoria del proceso <%d>", pid);
	}
	com_destruirMensaje(respuesta, logger);

}

void mensajeAConsolaFinalizo(int errorCode, int sockConsola) {
	log_trace(logger, "AVISO A CONSOLA QUE FINALIZE errorCode: %d", errorCode);
	t_mensaje *mensajeFinalizo = com_nuevoMensaje(PK_FINALIZO_PROGRAMA, logger);
	com_agregarIntAMensaje(mensajeFinalizo, "codigo_retorno", errorCode,
			logger);
	com_enviarMensaje(mensajeFinalizo, sockConsola, logger);
	com_destruirMensaje(mensajeFinalizo, logger);
}

void finalizarPrograma(t_listas *listas, int sock, int32_t pid, int codigoError) {
	log_trace(logger, "FINALIZO EL PROGRAMA pid: %d sock: %d error:%d", pid,
			sock,codigoError);
	if (sock == -1) {
		mensajeAConsolaFinalizo(codigoError, sock);
	}

	bool esPidBuscadoConsola(int* pidBuscado) {
		return (*pidBuscado) == pid;
	}
	t_pcb *pcb = list_find(listas->new->lista, esPidBuscadoConsola);
	if (pcb != NULL) {
		log_trace(logger, "El proceso estaba en new y lo voy mandar a EXIT");
		pcb->exitCode = codigoError;
		finalizarProgramaConError(listas->new->lista, listas->exit->lista, pcb);
		return;
	}
	pcb = list_find(listas->ready->lista, esPidBuscadoConsola);
	if (pcb != NULL) {
		log_trace(logger, "El proceso estaba en READY y lo voy mandar a EXIT");
		pcb->exitCode = codigoError;
		finalizarProgramaConError(listas->ready->lista, listas->exit->lista,
				pcb);
		return;
	}
	pcb = list_find(listas->exec->lista, esPidBuscadoConsola);
	if (pcb != NULL) {
		log_trace(logger, "El proceso estaba en EXEC y lo voy mandar a EXIT");
		pcb->exitCode = codigoError;
		finalizarProgramaConError(listas->exec->lista, listas->exit->lista,
				pcb);
		return;
	}
	pcb = list_find(listas->blocked->lista, esPidBuscadoConsola);
	if (pcb != NULL) {
		log_trace(logger,
				"El proceso estaba en BLOCKED y lo voy mandar a EXIT");
		pcb->exitCode = codigoError;
		finalizarProgramaConError(listas->blocked->lista, listas->exit->lista,
				pcb);
		return;
	}
}

void enviarProcesoACPU(t_pcb *pcb, t_cpu *cpu) {
	/*t_mensaje *mensaje = com_nuevoMensaje(CK_ENVIAR_CONTEXTO, logger);
	 com_agregarIntAMensaje(mensaje, "pid", pcb->pid, logger);
	 com_agregarIntAMensaje(mensaje, "progCounter", pcb->programCounter, logger);
	 com_agregarIntAMensaje(mensaje, "quantum", pcb->quantum, logger);
	 com_agregarIntAMensaje(mensaje, "quantumSleep", pcb->quantumSleep, logger);
	 // posicion del stack -> stack pointer que usa guille para saber cuales son las posiciones de las varibles
	 com_agregarIntAMensaje(mensaje, "stackPagina", pcb->stackPagina, logger);
	 com_agregarIntAMensaje(mensaje, "stackOffset", pcb->stackOffset, logger);
	 com_agregarIntAMensaje(mensaje, "stackSize", pcb->stackSize, logger);
	 com_agregarIntAMensaje(mensaje, "stackLavel", pcb->stackSize, logger);
	 com_agregarIntAMensaje(mensaje, "contStack", 0, logger);
	 // Indice de codigo -> es una matriz con coordenadas
	 com_agregarIntAMensaje(mensaje, "matrizIndices", 0, logger);

	 ////////////////////////
	 com_agregarIntAMensaje(mensaje, "cantInst", list_size(pcb->instrucciones), logger);
	 log_trace(logger, "Cantidad instrucciones: %d", list_size(pcb->instrucciones));
	 int i;
	 for(i = 0; i < list_size(pcb->instrucciones); i++) {
	 t_map_instruccion *inst = list_get(pcb->instrucciones, i);
	 char *clavePagina = malloc(100);
	 sprintf(clavePagina, "pagina%d", i);
	 com_agregarIntAMensaje(mensaje, clavePagina, inst->pagina, logger);
	 char *claveOffset = malloc(100);
	 sprintf(claveOffset, "offset%d", i);
	 com_agregarIntAMensaje(mensaje, claveOffset, inst->offset, logger);
	 char *claveSize = malloc(100);
	 sprintf(claveSize, "size%d", i);
	 com_agregarIntAMensaje(mensaje, claveSize, inst->size, logger);
	 log_trace(logger, "Agregue instruccion: %d --> <%s:%d><%s:%d><%s:%d>", i, clavePagina, inst->pagina, claveOffset, inst->offset, claveSize, inst->size);
	 }



	 ///////////////////////

	 // Indice de etiquetas - Variables compartidas ??
	 com_agregarIntAMensaje(mensaje, "etiquetas", 0, logger);
	 // indice de stack -> tipo lista
	 com_agregarIntAMensaje(mensaje, "stack", 0, logger);
	 log_trace(logger, "Voy a enviar el proceso a CPU: %s", com_imprimirMensaje(mensaje));
	 com_enviarMensaje(mensaje, cpu->sock, logger);
	 com_destruirMensaje(mensaje, logger);*/
	enviarPCBAlCPU(pcb, cpu->sock);
}

void reservarMemoriaParaProceso(t_pcb *pcb, int sockMemoria) {
	log_trace(logger, "Script : >%s<", pcb->script);
	char *scriptMinificado = getCadenaContatenada(pcb->script);
	log_trace(logger, "Script minificado: >%s<", scriptMinificado);
	int offset = 0;
	int lenScript = strlen(scriptMinificado) + 1;
	int size;
	char *buffer;

	while (lenScript - offset > 0) {
		size = pedirPaginaParaProceso(pcb, sockMemoria);
		pcb->tamPagina = size;
		tamanioPagina = size;

		buffer = malloc(size + 1);
		memset(buffer, '\0', size + 1);
		int largo = (lenScript - offset) < size ? lenScript - offset : size;
		memcpy(buffer, scriptMinificado + offset, largo);
		offset += size;

		guardarPaginaProceso(pcb, sockMemoria, buffer, size);
		free(buffer);
		pcb->paginasPedidas++;
	}
	size = pedirPaginaParaProceso(pcb, sockMemoria);

	buffer = malloc(size + 1);
	memset(buffer, '\0', size + 1);

	guardarPaginaProceso(pcb, sockMemoria, buffer, size);
	free(buffer);
	pcb->stackPagina = pcb->paginasPedidas;
	pcb->stackLevel = 0;
	pcb->stackOffset = 0;
	pcb->stackSize = 0;

	pcb->paginasPedidas++;
	pcb->primerPaginaHeap = pcb->paginasPedidas;
}

int pedirPaginaParaProceso(t_pcb *pcb, int sockMemoria) {
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
		log_error(logger, "error en la reserva de pagina \n");
		return 0;
	}
	log_trace(logger, "pagina %d reservada para %d", pcb->paginasPedidas,
			pcb->pid);
	com_destruirMensaje(respuestaReserva, logger);
	return size;
}

void guardarPaginaProceso(t_pcb *pcb, int sockMemoria, char *buffer, int size) {
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

void actualizarPCB(t_pcb *pcb, t_mensaje* mensaje) {
	log_trace(logger, "Voy a actualizar el PCB");

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

	log_trace(logger, "PID: [%d]", pcb->pid);
	log_trace(logger, "Mensaje que me llego: \n%s",
			com_imprimirMensaje(mensaje));

	//pcb->pid = com_leerIntAMensaje(mensaje, "pid", logger);
	pcb->programCounter = com_leerIntAMensaje(mensaje, "progCounter", logger);
	pcb->quantum = com_leerIntAMensaje(mensaje, "quantum", logger);
	pcb->stackPagina = com_leerIntAMensaje(mensaje, "stackPagina", logger);
	pcb->stackOffset = com_leerIntAMensaje(mensaje, "stackOffset", logger);
	pcb->stackSize = com_leerIntAMensaje(mensaje, "stackSize", logger);
	pcb->stackLevel = com_leerIntAMensaje(mensaje, "stackLevel", logger);
	pcb->cantidadRafagas = com_leerIntAMensaje(mensaje, "cantidad_rafagas",
			logger);

	pcb->cantidadRafagas = com_leerIntAMensaje(mensaje, "cantidad_rafagas",
			logger);

	pcb->descriptor = com_leerIntAMensaje(mensaje, "descriptor", logger);
	pcb->direccion = com_leerStringAMensaje(mensaje, "direccion", logger);
	pcb->creacion = com_leerIntAMensaje(mensaje, "creacion", logger);
	pcb->escritura = com_leerIntAMensaje(mensaje, "escritura", logger);
	pcb->lectura = com_leerIntAMensaje(mensaje, "lectura", logger);
	pcb->offsetArch = com_leerIntAMensaje(mensaje, "offsetArch", logger);
	pcb->tamAEscribir = com_leerIntAMensaje(mensaje, "tamAEscribir", logger);
	pcb->informacion = (char*) com_leerStringAMensaje(mensaje, "infoAEscribir",
			logger);

	pcb->stack = list_create();

	contador = com_leerIntAMensaje(mensaje, "contEtiquetas", logger);

	pcb->indiceEtiquetas = list_create();

	for (i = 0; i < contador; i++) {
		t_indiceDeEtiquetas* nuevo = malloc(sizeof(t_indiceDeEtiquetas));

		sprintf(claveEtiqueta, "claveEtiqueta%d", i);
		nuevo->etiquetas = string_new();
		strcpy(nuevo->etiquetas,
				(char*) com_leerStringAMensaje(mensaje, claveEtiqueta, logger));

		sprintf(claveUbicacion, "claveUbicacion%d", i);
		nuevo->ubicacion = com_leerIntAMensaje(mensaje, claveUbicacion, logger);

		list_add(pcb->indiceEtiquetas, nuevo);
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
			t_variables_stack *var = malloc(sizeof(t_variables_stack));

			sprintf(claveVarID, "varID%d.%d", s, subVar);
			var->id = (char*) com_leerStringAMensaje(mensaje, claveVarID,
					logger);

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

		list_add(pcb->stack, level);
	}

//	contador = com_leerIntAMensaje(mensaje, "cantInst", logger);

	/*pcb->indiceCodigo = list_create();

	 for(i = 0; i < contador; i++){
	 t_codigo* nuevo = malloc(sizeof(t_codigo));

	 sprintf(clavePagina, "pagina%d", i);
	 nuevo->pagina = com_leerIntAMensaje(mensaje, clavePagina, logger);

	 sprintf(claveOffset, "offset%d", i);
	 nuevo->offset = com_leerIntAMensaje(mensaje, claveOffset, logger);

	 sprintf(claveSize, "size%d", i);
	 nuevo->size = com_leerIntAMensaje(mensaje, claveSize, logger);



	 list_add(pcbActivo->indiceCodigo, nuevo);
	 }*/
}

void enviarPCBAlCPU(t_pcb *pcb, int sockCPU) {
	log_trace(logger, "Envio PCB al CPU");
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

	t_mensaje *mensaje = com_nuevoMensaje(CK_ENVIAR_CONTEXTO, logger);
	com_agregarIntAMensaje(mensaje, "pid", pcb->pid, logger);
	com_agregarIntAMensaje(mensaje, "progCounter", pcb->programCounter, logger);
	com_agregarIntAMensaje(mensaje, "quantum", pcb->quantum, logger);
	com_agregarStringAMensaje(mensaje, "algoritmo", algoritmo, logger);
	com_agregarIntAMensaje(mensaje, "quantumSleep", pcb->quantumSleep, logger);
	com_agregarIntAMensaje(mensaje, "stackPagina", pcb->stackPagina, logger);
	com_agregarIntAMensaje(mensaje, "stackOffset", pcb->stackOffset, logger);
	com_agregarIntAMensaje(mensaje, "stackSize", pcb->stackSize, logger);
	com_agregarIntAMensaje(mensaje, "stackLevel", pcb->stackLevel, logger);
	com_agregarIntAMensaje(mensaje, "tamPagina", pcb->tamPagina, logger);
	//com_agregarIntAMensaje(mensaje, "continua_activo", pcb->estadoEnCPU, logger);
	com_agregarIntAMensaje(mensaje, "cantidad_rafagas", pcb->cantidadRafagas,
			logger);
	com_agregarIntAMensaje(mensaje, "volviEntradaSalida",
			pcb->fuiAEntradaSalida, logger);
	com_agregarIntAMensaje(mensaje, "stackMaxSize", stackMaxSize, logger);
	com_agregarIntAMensaje(mensaje, "primerPaginaHeap", pcb->primerPaginaHeap,
			logger);

	if (pcb->informacion == NULL) {
		log_trace(logger, "Me llego NULL de la lectura");
		com_agregarStringAMensaje(mensaje, "infoLectura", "NULL", logger);
	} else {
		com_agregarStringAMensaje(mensaje, "infoLectura", pcb->informacion,
				logger);
	}
//
	pcb->fuiAEntradaSalida = 0;

	com_agregarIntAMensaje(mensaje, "cantidad_rafagas", pcb->cantidadRafagas,
			logger);

	pcb->direccion = malloc(1000);

//	log_error(logger, "DESCRIPTO: %d INFOOOO: %s, contStrak: %d", pcb->descriptor, pcb->informacion, list_size(pcb->stack));
	com_agregarIntAMensaje(mensaje, "descriptor", pcb->descriptor, logger);
////	com_agregarStringAMensaje(mensaje, "direccion", pcb->direccion, logger);
//	com_agregarIntAMensaje(mensaje, "creacion",pcb->creacion, logger);
//	com_agregarIntAMensaje(mensaje, "escritura", pcb->escritura, logger);
//	com_agregarIntAMensaje(mensaje, "lectura", pcb->lectura, logger);
//	com_agregarIntAMensaje(mensaje, "offsetArch", pcb->offsetArch, logger);
//	com_agregarIntAMensaje(mensaje, "tamAEscribir", pcb->tamAEscribir, logger);
//	com_agregarStringAMensaje(mensaje, "infoAEscribir", pcb->informacion, logger);
//	com_agregarStringAMensaje(mensaje, "infoALeer", pcb->informacion, logger);
	log_trace(logger, "4");
	com_agregarIntAMensaje(mensaje, "contEtiquetas",
			list_size(pcb->indiceEtiquetas), logger);
	log_trace(logger, "4.1");
	for (i = 0; i < list_size(pcb->indiceEtiquetas); i++) {
		log_trace(logger, "4.2");
		t_indiceDeEtiquetas* nuevo = list_get(pcb->indiceEtiquetas, i);
		log_trace(logger, "4.3");
		sprintf(claveEtiqueta, "claveEtiqueta%d", i);
		com_agregarStringAMensaje(mensaje, claveEtiqueta, nuevo->etiquetas,
				logger);
		log_trace(logger, "4.4");
		sprintf(claveUbicacion, "claveUbicacion%d", i);
		com_agregarIntAMensaje(mensaje, claveUbicacion, nuevo->ubicacion,
				logger);
		log_trace(logger, "4.5");
	}
	log_trace(logger, "5");
	com_agregarIntAMensaje(mensaje, "contStack", list_size(pcb->stack), logger);

	for (i = 0; i < list_size(pcb->stack); i++) {
		t_stack* level = (t_stack*) list_get(pcb->stack, i);

		sprintf(claveID, "claveID%d", i);
		com_agregarIntAMensaje(mensaje, claveID, level->id, logger);

		sprintf(claveRetPos, "retPos%d", i);
		com_agregarIntAMensaje(mensaje, claveRetPos, level->retPos, logger);

		sprintf(claveRetVarPag, "retVarPag%d", i);
		com_agregarIntAMensaje(mensaje, claveRetVarPag, level->retVar.pagina,
				logger);

		sprintf(claveRetVarOff, "retVarOff%d", i);
		com_agregarIntAMensaje(mensaje, claveRetVarOff, level->retVar.offset,
				logger);

		sprintf(claveRetVarSiz, "retVarSiz%d", i);
		com_agregarIntAMensaje(mensaje, claveRetVarSiz, level->retVar.size,
				logger);

		sprintf(claveArgumento, "contArgumento%d", i);
		com_agregarIntAMensaje(mensaje, claveArgumento,
				list_size(level->argumento), logger);

		for (a = 0; a < list_size(level->argumento); a++) {
			t_posicion* arg = (t_posicion*) list_get(level->argumento, a);

			sprintf(claveArgPag, "argPag%d.%d", i, a);
			com_agregarIntAMensaje(mensaje, claveArgPag, arg->pagina, logger);

			sprintf(claveArgOff, "argOff%d.%d", i, a);
			com_agregarIntAMensaje(mensaje, claveArgOff, arg->offset, logger);

			sprintf(claveArgSize, "argSize%d.%d", i, a);
			com_agregarIntAMensaje(mensaje, claveArgSize, arg->size, logger);

		}
		log_trace(logger, "6");
		sprintf(claveVariable, "contVariable%d", i);
		com_agregarIntAMensaje(mensaje, claveVariable,
				list_size(level->variable), logger);

		for (v = 0; v < list_size(level->variable); v++) {
			t_variables_stack* var = (t_variables_stack*) list_get(
					level->variable, v);

			sprintf(claveVarID, "varID%d.%d", i, v);
			com_agregarStringAMensaje(mensaje, claveVarID, var->id, logger);

			sprintf(claveVarPag, "varPag%d.%d", i, v);
			com_agregarIntAMensaje(mensaje, claveVarPag, var->posicion.pagina,
					logger);

			sprintf(claveVarOff, "varOff%d.%d", i, v);
			com_agregarIntAMensaje(mensaje, claveVarOff, var->posicion.offset,
					logger);

			sprintf(claveVarSize, "varSize%d.%d", i, v);
			com_agregarIntAMensaje(mensaje, claveVarSize, var->posicion.size,
					logger);

		}
	}

	com_agregarIntAMensaje(mensaje, "cantInst", list_size(pcb->instrucciones),
			logger);

	for (c = 0; c < list_size(pcb->instrucciones); c++) {
		t_map_instruccion* elem = (t_map_instruccion*) list_get(
				pcb->instrucciones, c);

		sprintf(clavePagina, "pagina%d", c);
		com_agregarIntAMensaje(mensaje, clavePagina, elem->pagina, logger);

		sprintf(claveOffset, "offset%d", c);
		com_agregarIntAMensaje(mensaje, claveOffset, elem->offset, logger);

		sprintf(claveSize, "size%d", c);
		com_agregarIntAMensaje(mensaje, claveSize, elem->size, logger);

	}
	log_trace(logger, "Mensaje que voy a enviar>>>> \n%s",
			com_imprimirMensaje(mensaje));

	com_enviarMensaje(mensaje, sockCPU, logger);

	log_trace(logger, "Voy a destruir el mensaje");
	com_destruirMensaje(mensaje, logger);
	pcb->fuiAEntradaSalida = 0;
}

void *hiloCreate(void *params) {
	t_hilo_params *p = (t_hilo_params *) params;
	t_listas *listas = (t_listas *) p->listas;
	int sockMemoria = *(int *) p->extra;
	while (1) {
		//sem_wait(&gradoMultiprogramacion);

		//sem_wait(&new);
		t_pcb *pcb = consumir(listas->new);
		cargarProgramaEnMemoria(pcb, sockMemoria);
		producir(listas->ready, pcb);

		//sem_post(&ready);
	}
}

void *hiloDispatcher(void *params) {
	t_hilo_params *p = (t_hilo_params *) params;
	t_listas *listas = (t_listas *) p->listas;
	int sockMemoria = *(int *) p->extra;
	while (1) {
		//sem_wait(&cpus);
		//sem_wait(&ready);
		t_pcb *pcb = consumir(listas->ready);
		producir(listas->exec, pcb);
		//sem_post(&exec);
	}
}

void *hiloCPU(void *params) {
	t_hilo_params *p = (t_hilo_params *) params;
	t_listas *listas = (t_listas *) p->listas;
	t_cpu *cpu = (t_cpu *) p->extra;

	while (1) {
		//sem_wait(cpu->semaforo);
		//sem_wait(&exec);
		t_pcb *pcb = consumir(listas->exec);
		pcb->sockCPU = cpu->sock;
		//pcb->semaforoCPU = cpu->semaforo;
		//sem_post(pcb->semEjecucion);
	}
	//sem_destroy(cpu->semaforo);
	close(cpu->sock);
}

void *hiloProceso(void *params) {
	t_hilo_params *p = (t_hilo_params *) params;
	t_listas *listas = (t_listas *) p->listas;
	t_pcb *pcb = (t_pcb *) p->extra;
	bool finalizar = false;
	while (!finalizar) {
		//sem_wait(pcb->semEjecucion);
		int32_t quantumRestante = pcb->quantum > 0 ? pcb->quantum : -1;
		enviarImagenProcesoACPU(pcb);
		bool pidioIO = false;
		while (!finalizar && (quantumRestante > 0 || quantumRestante == -1)
				&& !pidioIO) {
			// TODO QUIEN CONTROLA EL QUANTUM???? El CPU o el proceso, si es el CPU le envio el quantum en el mensaje si es el proceso le debo ir enviando las rafagas
			// TODO enviarRafaga() ??????????
			t_mensaje *mensaje = com_recibirMensaje(pcb->sockCPU, logger);
			switch (mensaje->header->tipo) {
			case 1 /*IO*/:
				// Cargar en pcb que es lo que esta pidiendo segun el mensaje
				pidioIO = true;
				break;
			case 2 /*MEMORIA HEAP*/:
				reservarMemoria(pcb, mensaje);
				break;
			case 3/*FINALIZAR*/:
				finalizar = true;
				break;
			default:
				break;
			}
			if (!finalizar && pcb->quantum != 0) {
				usleep(pcb->quantumSleep);
			}
		}
		if (pidioIO) {
			producir(listas->blocked, pcb);
			//	sem_post(&blocked);
		} else {
			producir(listas->ready, pcb);
			//	sem_post(&ready);
		}
		//sem_post(pcb->semaforoCPU);
		//sem_post(&cpus);
	}
	producir(listas->exit, pcb);
	//sem_post(&gradoMultiprogramacion);
}

void *hiloBlocked(void *params) {
	t_hilo_params *p = (t_hilo_params *) params;
	t_listas *listas = (t_listas *) p->listas;
	t_config *config = (t_config *) p->extra;
	int sockFileSystem = com_conectarseA(
			config_get_string_value(config, "IP_FS"),
			config_get_int_value(config, "PUERTO_FS"));
	if (sockFileSystem <= 0) {
		log_error(logger,
				"Error al conectarse al file system IP: %s PUERTO: %d",
				config_get_string_value(config, "IP_FS"),
				config_get_int_value(config, "PUERTO_FS"));
	}
	log_info(logger, "Me conecte al file sytem en el sock: %d", sockFileSystem);

	if (!com_kerfs_handshake_kernel(sockFileSystem, logger)) {
		log_error(logger, "Error en el handshake con el File System");
	}
	while (1) {
		puts("antes semaforo");
		//sem_wait(&blocked);
		puts("despues semaforo");
		t_pcb *pcb = consumir(listas->blocked);
		// TODO Manejar pedido de I/O con file system
		//sem_post(&ready);
	}
}

void cargarProgramaEnMemoria(t_pcb *pcb, int sockMemoria) {
	// TODO cargar el proceso en memoria
}

int nuevoPrograma(int listenerProgramas, t_listas *listas, int numeroPid,
		uint32_t quantum, uint32_t quantumSleep) {
	int sockNuevoPrograma = com_aceptarConexion(listenerProgramas);
	if (!com_prgker_handshake_kernel(sockNuevoPrograma, logger)) {
		log_error(logger, "Error en el handshake con el programa");
		return -1;
	}
	log_info(logger, "Se conecto un nuevo programa en el sock: %d",
			sockNuevoPrograma);

	t_mensaje *mensaje = com_recibirMensaje(sockNuevoPrograma, logger);
	log_trace(logger, "RECIBI EL PROGRAMA: %s", com_imprimirMensaje(mensaje));
	if (mensaje->header->tipo != PK_ENVIAR_PROGRAMA) {
		log_error(logger,
				"Error, se esperaba el mensaje inicial de ejecucion de un programa");
		return -1;
	}

	t_pcb *pcb = malloc(sizeof(t_pcb));
	pcb->sock = sockNuevoPrograma;
	pcb->pid = numeroPid;
	pcb->programCounter = 0;
	pcb->stackPointer = 0;
	pcb->sockCPU = -1;
	pcb->tablaArchivos = list_create();
	pcb->tablaHeap = list_create();
	pcb->quantum = quantum;
	pcb->quantumSleep = quantumSleep;
	pcb->paginasPedidas = 0;
	pcb->exitCode = EXIT_CODE_CORRECTO;

	pcb->stack = list_create();
	pcb->indiceEtiquetas = list_create();

	t_stack* level = malloc(sizeof(t_stack));
	level->id = 0;
	level->argumento = list_create();
	level->variable = list_create();
	level->retPos = 0;
	level->retVar.pagina = 0;
	level->retVar.offset = 0;
	level->retVar.size = 0;

	list_add(pcb->stack, level);

	pcb->primerPaginaHeap = 0;
	pcb->cantPaginasHeap = 0;
	pcb->descriptorNext = 3;
	pcb->fuiAEntradaSalida = 0;
	pcb->cantidadRafagas = 0;
	pcb->descriptor = 0;
	pcb->stackLevel = 0;
	pcb->informacion = malloc(1000);
	pcb->cantAccionAlocar = 0;
	pcb->cantAccionLiberar = 0;
	pcb->cantInstPriv = 0;
	pcb->cantSyscalls = 0;
	memset(pcb->informacion, '\0', 1000);
	pcb->informacion[0] = ' ';
	//sem_t semaforo;
	///sem_init(&semaforo, 0, 0);
	//pcb->semEjecucion = &semaforo;

	char *script = com_leerStringAMensaje(mensaje, "script", logger);
	pcb->script = (char *) malloc(strlen(script) + 1);

	memcpy(pcb->script, script, strlen(script) + 1);
	//log_trace(logger, "Voy a crear el hilo");
	//crearHilo(listas, pcb, hiloProceso);

	pcb->instrucciones = scriptToBloques(script);

	producir(listas->new, pcb);
	producir(listas->programas, pcb);
	//sem_post(&new);
	log_info(logger, "Se creo un nuevo proceso con el pid: %d", numeroPid);

	t_mensaje *respuesta = com_nuevoMensaje(PK_RESPUESTA_PROGRAMA_OK, logger);
	com_agregarIntAMensaje(respuesta, "pid", pcb->pid, logger);
	com_enviarMensaje(respuesta, pcb->sock, logger);
	com_destruirMensaje(respuesta, logger);

	return sockNuevoPrograma;
}

int nuevoCPU(int listenerCPUs, t_listas *listas) {
	log_info(logger, "voy a aceptar conexion en: %d", listenerCPUs);
	int sockCPU = com_aceptarConexion(listenerCPUs);
	if (!com_cpuker_handshake_kernel(sockCPU, logger)) {
		log_error(logger, "Error en el handshake con el cpu");
		return -1;
	}
	log_info(logger, "Se conecto un nuevo CPU en el sock: %d", sockCPU);

	t_cpu *cpu = (t_cpu *) malloc(sizeof(t_cpu));
	//sem_t semaforo;
	//sem_init(&semaforo, 0, 1);
	cpu->sock = sockCPU;
	cpu->activo = 1;
	cpu->libre = 1;
	//cpu->semaforo = &semaforo;

	//producir(listas->cpus->lista, cpu);

	//pthread_mutex_lock(listas->cpus->mutex);
	list_add(listas->cpus->lista, cpu);
	//pthread_mutex_unlock(listas->cpus->mutex);

	// crearHilo(listas, cpu, hiloCPU);

	//sem_post(&cpus);

	log_info(logger, "Se conecto un nuevo CPU");
	return sockCPU;
}

void reservarMemoria(t_pcb *pcb, t_mensaje *mensaje) {
	// TODO hacer reserva de memoria Haap segun lo que pide en el mensaje
}

void enviarImagenProcesoACPU(t_pcb *pcb) {
	t_mensaje *mensaje = com_nuevoMensaje(1/*IMAGEN_PROCESO*/, logger);
	// TODO cargar imagen del proceso en mensjae
	com_enviarMensaje(mensaje, pcb->sockCPU, logger);
}

void crearHilo(t_listas *listas, void *extraParams, void *(*funcion)(void *)) {
	log_trace(logger, "Voy a crear un thread");
	t_hilo_params *params = malloc(sizeof(t_hilo_params));
	params->listas = listas;
	params->extra = extraParams;
	pthread_t newDispatcher;
	int hilo = pthread_create(&newDispatcher, NULL, funcion, params);
//	pthread_mutex_lock(listas->hilos->mutex);
	list_add(listas->hilos->lista, &hilo);
//	pthread_mutex_unlock(listas->hilos->mutex);
	log_trace(logger, "Se creo un nuevo thread");
}

void producir(t_lista_sinc *lista, t_pcb *pcb) {
	log_trace(logger, "Productor INIT");
	//pthread_mutex_lock(lista->mutex);
	log_trace(logger, "Productor MEDIUM");

	list_add(lista->lista, pcb);
	//pthread_mutex_unlock(lista->mutex);
	log_trace(logger, "Productor END");

}

t_pcb *consumir(t_lista_sinc *lista) {
	//pthread_mutex_lock(lista->mutex);
	t_pcb *pcb = (t_pcb *) list_get(lista->lista, 0);
	list_remove(lista->lista, 0);
	//pthread_mutex_unlock(lista->mutex);
	return pcb;
}

void inicializarSemaforos(int gradoMultiprogracion) {
	/*	sem_init(&mutexNew, 0, 1);
	 sem_init(&mutexReady, 0, 1);
	 sem_init(&mutexExec, 0, 1);
	 sem_init(&mutexBlocked, 0, 1);
	 sem_init(&mutexExit, 0, 1);
	 sem_init(&mutexCPUs, 0, 1);
	 sem_init(&mutexHilos, 0, 1);
	 sem_init(&gradoMultiprogramacion, 0, gradoMultiprogracion);
	 sem_init(&new, 0, 0);
	 sem_init(&ready, 0, 0);
	 sem_init(&exec, 0, 0);
	 sem_init(&blocked, 0, 0);
	 sem_init(&cpus, 0, 0);*/
}

void finalizarSemaforos() {
	/*	sem_destroy(&mutexNew);
	 sem_destroy(&mutexReady);
	 sem_destroy(&mutexExec);
	 sem_destroy(&mutexBlocked);
	 sem_destroy(&mutexCPUs);
	 sem_destroy(&mutexHilos);
	 sem_destroy(&gradoMultiprogramacion);
	 sem_destroy(&new);
	 sem_destroy(&ready);
	 sem_destroy(&exec);
	 sem_destroy(&blocked);
	 sem_destroy(&cpus);*/
}

t_listas *inicializarListas() {
	t_listas *listas = malloc(sizeof(t_listas));

	listas->new = malloc(sizeof(t_lista_sinc));
	listas->new->lista = list_create();
	//listas->new->mutex = &mutexNew;
	listas->ready = malloc(sizeof(t_lista_sinc));
	listas->ready->lista = list_create();
	//listas->ready->mutex = &mutexReady;
	listas->exec = malloc(sizeof(t_lista_sinc));
	listas->exec->lista = list_create();
	//listas->exec->mutex = &mutexExec;
	listas->blocked = malloc(sizeof(t_lista_sinc));
	listas->blocked->lista = list_create();
	//listas->blocked->mutex = &mutexBlocked;
	listas->exit = malloc(sizeof(t_lista_sinc));
	listas->exit->lista = list_create();
	//listas->exit->mutex = &mutexExit;
	listas->cpus = malloc(sizeof(t_lista_sinc));
	listas->cpus->lista = list_create();
	//listas->cpus->mutex = &mutexCPUs;
	listas->hilos = malloc(sizeof(t_lista_sinc));
	listas->hilos->lista = list_create();
	//listas->hilos->mutex = &mutexHilos;
	listas->programas = malloc(sizeof(t_lista_sinc));
	listas->programas->lista = list_create();
	listas->archivosGlobal = malloc(sizeof(t_lista_sinc));
	listas->archivosGlobal->lista = list_create();
	listas->variablesComp = malloc(sizeof(t_lista_sinc));
	listas->variablesComp->lista = list_create();
	listas->semaforosComp = malloc(sizeof(t_lista_sinc));
	listas->semaforosComp->lista = list_create();
	return listas;
}

void finalizarListas(t_listas *listas) {
	list_destroy_and_destroy_elements(listas->new->lista, destroyPCB);
	list_destroy_and_destroy_elements(listas->ready->lista, destroyPCB);
	list_destroy_and_destroy_elements(listas->exec->lista, destroyPCB);
	list_destroy_and_destroy_elements(listas->blocked->lista, destroyPCB);
	list_destroy_and_destroy_elements(listas->exit->lista, destroyPCB);
	finalizarCPUs(listas->cpus->lista);
	list_destroy(listas->cpus->lista);
	finalizarHilos(listas->hilos->lista);
	list_destroy(listas->hilos->lista);
}

void finalizarCPUs(t_list *cpus) {
	int i;
	for (i = list_size(cpus) - 1; i >= 0; i--) {
		t_cpu *cpu = (t_cpu *) list_get(cpus, i);
		close(cpu->sock);
		//sem_destroy(cpu->semaforo);
		free(cpu);
		list_remove(cpus, i);
	}
}

void finalizarHilos(t_list *hilos) {
	int i;
	for (i = list_size(hilos) - 1; i >= 0; i--) {
		int hilo = *(int *) list_get(hilos, i);
		pthread_kill(hilo, SIGKILL);
		list_remove(hilos, i);
	}
}

void destroyPCB(void *pcb) {
	t_pcb *p = (t_pcb *) pcb;
	//sem_destroy(p->semEjecucion);
	list_destroy(p->tablaHeap);
	list_destroy(p->tablaArchivos);
	free(p);
}

void atenderES(t_listas *listas, t_pcb *proceso, int sockFS) {
	proceso->fuiAEntradaSalida = 1;
	bool esArchBuscadoGlobal(t_tabla_global_archivo* arch){ log_trace(logger,"El archivo que busco es: [%s]", arch->file); return  strcmp(arch->file, proceso->direccion) == 0;}
	bool esArchBuscado(t_tabla_archivo* arch){ log_trace(logger,"El descriptor que busco es: [%d]", arch->fileDescriptor); return arch->fileDescriptor == proceso->descriptor;}
	void destroyArch(t_tabla_archivo* arch){ free(arch);}
	void destroyArchGlobal(t_tabla_global_archivo* arch){ free(arch);}
	bool esSemComp(t_var_comp* var) {
		return strcmp(var->nombre, proceso->nombreSemaforo) == 0;
	}
	t_tabla_archivo *archPCB;
	t_var_comp* sem;

	proceso->exitCode = EXIT_CODE_CORRECTO;

	switch (proceso->operacionES) {
		case OPERACION_ABRIR_ARCHIVO:

			if(!validarExisteArchivo(proceso->direccion, sockFS)) {
				if(proceso->creacion){
					crearArchivo(proceso->direccion, sockFS);
				}else{
					proceso->exitCode = EXIT_CODE_ERROR_ARCHIVO_INEXISTENTE;
					break;
				}
			}
			t_tabla_global_archivo* archivoGlobal = list_find(listas->archivosGlobal->lista, esArchBuscadoGlobal);
			t_tabla_global_archivo *global;
			if(archivoGlobal != NULL) {
				archivoGlobal->countOpen++;
				global = archivoGlobal;
			} else {
				t_tabla_global_archivo *nuevoArch = malloc(sizeof(t_tabla_global_archivo));
				nuevoArch->countOpen = 1;
				strcpy(nuevoArch->file, proceso->direccion);
				list_add(listas->archivosGlobal->lista, nuevoArch);
				global = nuevoArch;
			}
			t_tabla_archivo *archPCB = malloc(sizeof(t_tabla_archivo));
			proceso->lectura = 0;
			proceso->escritura = 0;
			archPCB->permisos = string_new();
			if(proceso->lectura) {
				proceso->lectura = 1;
				string_append(&(archPCB->permisos), "r");
			}
			if(proceso->escritura) {
				proceso->escritura = 1;
				string_append(&(archPCB->permisos), "w");
			}
			archPCB->globalFD = global;
			archPCB->fileDescriptor = proceso->descriptorNext;
			proceso->descriptor = archPCB->fileDescriptor;
			proceso->descriptorNext++;
			list_add(proceso->tablaArchivos, archPCB);
			//else {
				// FINALIZAR porque intento abrir un archivo invalido
				// proceso->exitCode = EXIT_CODE_ERROR_ARCHIVO_INEXISTENTE;

			//}
			break;
		case OPERACION_BORRAR_ARCHIVO:
			archPCB = list_find(proceso->tablaArchivos, esArchBuscado);
			if(archPCB != NULL && validarExisteArchivo(archPCB->globalFD->file, sockFS)) {
				if(archPCB->globalFD->countOpen != 1) {
					// No se puede borrar el archivo porque otros programas tienen el archivo abierto
					break;
				}
				if(borrarArchivo(archPCB->globalFD->file, sockFS)) {
					list_remove_and_destroy_by_condition(listas->archivosGlobal->lista, esArchBuscadoGlobal, destroyArchGlobal);
				} else {
					log_error(logger, "Error al borrar archivo %s, sockFS: %d", archPCB->globalFD->file, sockFS);
				}
			} else {
				// FINALIZAR porque intento borrar un archivo invalido
				proceso->exitCode = EXIT_CODE_ERROR_SIN_DEFINICION;
			}
			break;
		case OPERACION_CERRAR_ARCHIVO:
			archPCB = list_find(proceso->tablaArchivos, esArchBuscado);
			if(archPCB != NULL) {
				archPCB->globalFD->countOpen--;
				if(archPCB->globalFD->countOpen == 0) {
					list_remove_and_destroy_by_condition(listas->archivosGlobal->lista, esArchBuscadoGlobal, destroyArchGlobal);
				}
				list_remove_and_destroy_by_condition(proceso->tablaArchivos, esArchBuscado, destroyArch);
			} else {
				// FINALIZAR porque intento cerrar un archivo invalido
				proceso->exitCode = EXIT_CODE_ERROR_SIN_DEFINICION;
			}
			break;
		case OPERACION_ESCRIBIR_ARCHIVO:
			if(proceso->descriptor == 1) {
				t_mensaje *mensajePantalla = com_nuevoMensaje(PK_PRINT_CONSOLA, logger);
				com_agregarStringAMensaje(mensajePantalla, "texto", proceso->informacion, logger);
				com_enviarMensaje(mensajePantalla, proceso->sock, logger);
				com_destruirMensaje(mensajePantalla, logger);
				proceso->exitCode = EXIT_CODE_CORRECTO;
			} else {
				log_trace(logger, "Voy a buscar el archivo [%d]. Tam lista: [%d]",proceso->descriptor, list_size(proceso->tablaArchivos));
				archPCB = list_find(proceso->tablaArchivos, esArchBuscado);
				if(archPCB != NULL && archPCB->escritura) {
					escribirArchivo(proceso, sockFS, archPCB);
				} else {
					// FINALIZAR porque intento escribir un archivo que no podia
					proceso->exitCode = EXIT_CODE_ERROR_ESCRIBIR_SIN_PERMISO;
				}

			}

			break;
		case OPERACION_LEER_ARCHIVO:
			log_trace(logger, "Voy a buscar el archivo [%d]. Tam lista: [%d]",proceso->descriptor, list_size(proceso->tablaArchivos));
			archPCB = list_find(proceso->tablaArchivos, esArchBuscado);
			if(archPCB != NULL && archPCB->lectura) {
				leerArchivo(proceso, sockFS, archPCB);
			} else {
				// FINALIZAR porque intento leer un archivo que no podia
				proceso->exitCode = EXIT_CODE_ERROR_LEER_SIN_PERMISO;
			}
			break;
		case OPERACION_WAIT_SEMAFORO:
			proceso->exitCode = ESPERANDO_SEMAFORO;
			log_trace(logger, "Me enviaron un wait del semaforo <%s>", proceso->nombreSemaforo);
			sem = (t_var_comp*) list_find(listas->semaforosComp->lista,(void*) esSemComp);
			if(sem != NULL)  {
				//log_trace(logger, "Tengo el semaforo %s que esta en valor %d y tiene %d cpus encolados", sem->nombre, sem->valor, queue_size(sem->pilaEspera));
				queue_push(sem->pilaEspera, proceso);
				//log_trace(logger, "Tengo el semaforo %s que esta en valor %d y tiene %d cpus encolados", sem->nombre, sem->valor, queue_size(sem->pilaEspera));
				if(sem->valor > 0) {
					sem->valor --;
					//log_trace(logger, "Tengo el semaforo %s que esta en valor %d y tiene %d cpus encolados", sem->nombre, sem->valor, queue_size(sem->pilaEspera));
					t_pcb *pcbEsperando = queue_pop(sem->pilaEspera);
					//log_trace(logger, "Tengo el semaforo %s que esta en valor %d y tiene %d cpus encolados", sem->nombre, sem->valor, queue_size(sem->pilaEspera));
					pcbEsperando->exitCode = EXIT_CODE_CORRECTO;
				}
			}
			break;
		case OPERACION_ATENDIDA:
			break;
		default:
			break;
	}
	proceso->operacionES = OPERACION_ATENDIDA;
	proceso->fuiAEntradaSalida = 1;
}


int validarExisteArchivo(char *nombreArch, int sockFS) {
	t_mensaje* mensaje = com_nuevoMensaje(KF_ARCH_EXIST, logger);
	com_agregarStringAMensaje(mensaje, "nombre_archivo", nombreArch, logger);
	int mensajeAlFS = com_enviarMensaje(mensaje, sockFS, logger);
	com_destruirMensaje(mensaje, logger);
	t_mensaje* mensajeDelFS = com_recibirMensaje(sockFS, logger);
	if (mensajeDelFS->header->tipo != FK_ARCH_EXIST_RTA) {
		return 0;
	}
	return com_leerIntAMensaje(mensajeDelFS, "resultado", logger);
}

int crearArchivo(char *nombreArch, int sockFS) {
	t_mensaje* mensaje = com_nuevoMensaje(KF_ABRIR_ARCH, logger);
	com_agregarStringAMensaje(mensaje, "nombre_archivo", nombreArch, logger);
	int mensajeAlFS = com_enviarMensaje(mensaje, sockFS, logger);
	com_destruirMensaje(mensaje, logger);
	t_mensaje* mensajeDelFS = com_recibirMensaje(sockFS, logger);
	if (mensajeDelFS->header->tipo != FK_ABRIR_ARCH_RTA) {
		return 0;
	}
	return com_leerIntAMensaje(mensajeDelFS, "resultado", logger);
}

int borrarArchivo(char *nombreArch, int sockFS) {
	t_mensaje* mensaje = com_nuevoMensaje(KF_BORRAR_ARCH, logger);
	com_agregarStringAMensaje(mensaje, "nombre_archivo", nombreArch, logger);
	int mensajeAlFS = com_enviarMensaje(mensaje, sockFS, logger);
	com_destruirMensaje(mensaje, logger);
	t_mensaje* mensajeDelFS = com_recibirMensaje(sockFS, logger);
	if (mensajeDelFS->header->tipo != FK_BORRAR_ARCH_RTA) {
		return 0;
	}
	return com_leerIntAMensaje(mensajeDelFS, "resultado", logger);
}

int escribirArchivo(t_pcb *pcb, int sockFS, t_tabla_archivo* archPCB) {
	t_mensaje* mensaje = com_nuevoMensaje(KF_ESCRIBIR_ARCH, logger);
	com_agregarStringAMensaje(mensaje, "nombre_archivo",
			archPCB->globalFD->file, logger);
	com_agregarStringAMensaje(mensaje, "escritura", pcb->informacion, logger);
	com_agregarIntAMensaje(mensaje, "size", pcb->tamAEscribir, logger);
	com_agregarIntAMensaje(mensaje, "offset", pcb->offsetArch, logger);
	int mensajeAlFS = com_enviarMensaje(mensaje, sockFS, logger);
	com_destruirMensaje(mensaje, logger);
	t_mensaje* mensajeDelFS = com_recibirMensaje(sockFS, logger);
	if (mensajeDelFS->header->tipo != FK_ESCRIBIR_ARCH_RTA) {
		return 0;
	}
	return com_leerIntAMensaje(mensajeDelFS, "resultado", logger);
}

int leerArchivo(t_pcb *pcb, int sockFS, t_tabla_archivo* archPCB) {
	t_mensaje* mensaje = com_nuevoMensaje(KF_LEER_ARCH, logger);
	com_agregarStringAMensaje(mensaje, "nombre_archivo",
			archPCB->globalFD->file, logger);
	com_agregarIntAMensaje(mensaje, "size", pcb->tamAEscribir, logger);
	com_agregarIntAMensaje(mensaje, "offset", pcb->offsetArch, logger);
	int mensajeAlFS = com_enviarMensaje(mensaje, sockFS, logger);
	com_destruirMensaje(mensaje, logger);
	t_mensaje* mensajeDelFS = com_recibirMensaje(sockFS, logger);
	if (mensajeDelFS->header->tipo != FK_LEER_ARCH_RTA) {
		return 0;
	}
	pcb->informacion = com_leerStringAMensaje(mensajeDelFS, "lectura", logger);
	return com_leerIntAMensaje(mensajeDelFS, "resultado", logger);
}

void consolaKernel() {
	while (1) {
		homeKernelConsola();
		leerComandos();
	}
}

void homeKernelConsola() {
	printf("------------------------------------\n");
	printf("Bienvenido a la consola de Kernel \n");
	printf("------------------------------------\n");
	printf("Por favor, ingrese una de las siguientes opciones para operar: \n");
	printf("1 - PROCESOS DEL SISTEMA. \n");
	printf("2 - INFORMACION PROCESO. \n");
	printf("3 - TABLA GLOBAL DE ARCHIVOS. \n");
	printf("4 - MODIFICAR GRADO MULTIPROGRAMACION. \n");
	printf("5 - FINALIZAR PROCESO. \n");
	if (sistemaPausado) {
		printf("6 - REANUDAR PLANIFICACION. \n");
	} else {
		printf("6 - DETENER PLANIFICACION. \n");
	}
	//printf("13 - IMPRIMIR VARIABLES. \n");

	printf("------------------------------------\n");
	printf("Introduzca el numero de opcion y presione ENTER\n\n");
}

void leerComandos() {

	char *comando = detectaIngresoConsola("Ingrese una opcion: ");
	printf("\n");

	if (comando[0] != ' ' && comando[0] != '\0') {

		int opcion = detectarComandoConsola(comando);

		switch (opcion) {
		case LISTA_PROCESOS: {
			imprimirListaProcesos();
			break;
		}
		case INFO_PROCESO: {
			infoProceso();
			break;
		}
		case TABLA_GLOBAL: {
			imprimirTablaGlobal();
			break;
		}
		case GRADO_MULT: {
			modificarGradoMult();
			break;
		}
		case FIN_PROCESO: {
			finalizarProceso();
			break;
		}
		case STOP_PLANI: {
			stopPlani();
			break;
		}
		case IMP_VARIABLES: {
			imprimirVariables();
			break;
		}
		default: {
			printf("\n\n");
			printf("Comando invalido. \n\n");
			leerComandos(listas);
		}
		}
	} else {
		leerComandos(listas);
	}

	free(comando);
}

char* detectaIngresoConsola(char* const mensaje) {
	char * entrada = malloc(sizeof(char));

	printf("%s", mensaje);

	char c = fgetc(stdin);
	int i = 0;

	while (c != '\n') {
		entrada[i] = c;
		i++;
		entrada = realloc(entrada, (i + 1) * sizeof(char));
		c = fgetc(stdin);
	}

	entrada[i] = '\0';

	return entrada;
}

int detectarComandoConsola(char* comando) {
	if (string_equals_ignore_case(comando, "1") == true)
		return LISTA_PROCESOS;
	if (string_equals_ignore_case(comando, "2") == true)
		return INFO_PROCESO;
	if (string_equals_ignore_case(comando, "3") == true)
		return TABLA_GLOBAL;
	if (string_equals_ignore_case(comando, "4") == true)
		return GRADO_MULT;
	if (string_equals_ignore_case(comando, "5") == true)
		return FIN_PROCESO;
	if (string_equals_ignore_case(comando, "6") == true)
		return STOP_PLANI;
	if (string_equals_ignore_case(comando, "7") == true)
		return IMP_NEW;
	if (string_equals_ignore_case(comando, "8") == true)
		return IMP_READY;
	if (string_equals_ignore_case(comando, "9") == true)
		return IMP_EXEC;
	if (string_equals_ignore_case(comando, "10") == true)
		return IMP_BLOQUED;
	if (string_equals_ignore_case(comando, "11") == true)
		return IMP_EXIT;
	if (string_equals_ignore_case(comando, "12") == true)
		return IMP_TODAS;
	if (string_equals_ignore_case(comando, "13") == true)
		return IMP_VARIABLES;
	return -1;
}

void imprimirListaProcesos() {

	printf("\n");
	printf("7 - IMPRIMIR NEW. \n");
	printf("8 - IMPRIMIR READY. \n");
	printf("9 - IMPRIMIR EXEC. \n");
	printf("10 - IMPRIMIR BLOQUED. \n");
	printf("11 - IMPRIMIR EXIT. \n");
	printf("12 - IMPRIMIR TODAS. \n");
	printf("\n");

	char *comando = detectaIngresoConsola("Ingrese una opcion: ");
	printf("\n");

	if (comando[0] != ' ' && comando[0] != '\0') {

		int opcion = detectarComandoConsola(comando);
		void imprimirElem(t_pcb* proceso) {
			printf("pid: %d\n", proceso->pid);
		}

		switch (opcion) {
		case IMP_NEW: {
			printf("------- Lista New ------\n");
			list_iterate(listas->new->lista, (void *) imprimirElem);
			break;
		}
		case IMP_READY: {
			printf("------- Lista Ready ------\n");
			list_iterate(listas->ready->lista, (void *) imprimirElem);
			break;
		}
		case IMP_EXEC: {
			printf("------- Lista Exec ------\n");
			list_iterate(listas->exec->lista, (void *) imprimirElem);
			break;
		}
		case IMP_BLOQUED: {
			printf("------- Lista Bloqued ------\n");
			list_iterate(listas->blocked->lista, (void *) imprimirElem);
			break;
		}
		case IMP_EXIT: {
			printf("------- Lista Exit ------\n");
			list_iterate(listas->exit->lista, (void *) imprimirElem);
			break;
		}
		case IMP_TODAS: {
			printf("------- Lista New ------\n");
			list_iterate(listas->new->lista, (void *) imprimirElem);
			printf("\n\n");
			printf("------- Lista Ready ------\n");
			list_iterate(listas->ready->lista, (void *) imprimirElem);
			printf("\n\n");
			printf("------- Lista Exec ------\n");
			list_iterate(listas->exec->lista, (void *) imprimirElem);
			printf("\n\n");
			printf("------- Lista Bloqued ------\n");
			list_iterate(listas->blocked->lista, (void *) imprimirElem);
			printf("\n\n");
			printf("------- Lista Exit ------\n");
			list_iterate(listas->exit->lista, (void *) imprimirElem);
			break;
		}
		default: {
			printf("\n\n");
			printf("Comando invalido. \n\n");
		}
		}
	}
}

void infoProceso() {
	char* comando = detectaIngresoConsola(
			"Ingrese el PID del proceso que desea obtener informacion: ");
	printf("\n");

	if (comando[0] != ' ' && comando[0] != '\0') {
		if (es_entero(comando)) {
			int pid = atoi(comando);
			bool esPidBuscado(int* pidBuscado) {
				return (*pidBuscado) == pid;
			}

			t_pcb *resultado = (t_pcb*) list_find(listas->programas->lista,
					(void *) esPidBuscado);

			void imprimirElem(t_tabla_archivo* arch) {
				printf("FD: %d\n", arch->fileDescriptor);
				printf("Permisos: %s\n\n", arch->permisos);
			}

			printf("----- INFORMACION DEL PID: %d ---------\n", resultado->pid);
			printf("Rafagas ejecutadas: %d\n", resultado->cantidadRafagas);
			printf("Operaciones privilegiadas: %d\n", resultado->cantInstPriv);
			printf("Tabla de archivos: \n");
			list_iterate(resultado->tablaArchivos, (void *) imprimirElem);
			printf("Acciones alocar: %d\n", resultado->cantAccionAlocar);
			printf("Acciones liberar: %d\n", resultado->cantAccionLiberar);
			printf("Syscalls ejecutadas: %d\n\n", resultado->cantSyscalls);

		} else {
			printf("Ingresar un numero entero\n\n");
			infoProceso(listas);
		}
	}

}

void modificarGradoMult() {
	char * grado = detectaIngresoConsola(
			"Ingrese el nuevo grado de multiprogramacion: ");
	printf("\n");

	if (grado[0] != ' ' && grado[0] != '\0') {
		gradoMultiprogramacion = atoi(grado);
		printf("El grado de multiprorgamacion ahora es de: %d\n\n",
				gradoMultiprogramacion);
		actualizarEstadoSistema();
	} else {
		modificarGradoMult();
	}
}

void imprimirTablaGlobal() {

	void imprimirElem(t_tabla_global_archivo* arch) {
		printf("FILE: %s\n", arch->file);
		printf("V abierto: %d\n\n", arch->countOpen);
	}

	list_iterate(listas->archivosGlobal->lista, (void *) imprimirElem);
}

void finalizarProceso() {
	char* comando = detectaIngresoConsola(
			"Ingrese el PID del proceso que desea finalizar: ");
	printf("\n");

	if (comando[0] != ' ' && comando[0] != '\0') {
		if (es_entero(comando)) {
			int pid = atoi(comando);
//			int size;
//			if (size > 0) {
//				printf("El pid: %d ocupa %d marcos\n\n", pid, size);
//			} else {
//				printf("El pid: %d no existe en el sistema.\n\n", pid);
//			}

			finalizarPrograma(listas, -1, pid,EXIT_CODE_ERROR_COMANDO_FINALIZAR);

		} else {
			printf("Ingresar un numero entero\n\n");
			finalizarProceso();
		}
	}
}

void stopPlani() {

	if (sistemaPausado) {
		sistemaPausado = 0;
		pthread_mutex_unlock(&mutexSistemaPausado);
		printf("Planificacion reanudada\n\n");
	} else {
		sistemaPausado = 1;
		printf("Planificacion detenida\n\n");
	}

}

void imprimirVariables() {
	void imprimirVariable(t_var_comp* var) {
		printf("Nombre: %s, valor: %d\n", var->nombre, var->valor);
	}

	void imprimirSemaforo(t_var_comp* var) {
		printf("Nombre: %s, valor: %d, size espera: %d\n", var->nombre,
				var->valor, queue_size(var->pilaEspera));
	}

	printf("------- Variables Compartidas ------\n");
	list_iterate(listas->variablesComp->lista, (void *) imprimirVariable);

	printf("------- Semaforos ------\n");
	list_iterate(listas->semaforosComp->lista, (void *) imprimirSemaforo);
}

int es_entero(char *cadena) {
	int i, valor;

	for (i = 0; i < string_length(cadena); i++) {
		valor = cadena[i] - '0';
		if (valor < 0 || valor > 9) {
			if (i == 0 && valor == -3)
				continue;
			return 0;
		}
	}
	return 1;
}
