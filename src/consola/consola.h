/*
 * consola.h
 *
 *  Created on: 6/4/2017
 *      Author: utnso
 */

#ifndef CONSOLA_H_
#define CONSOLA_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "../comunicacion/comunicacion.h"
#include <commons/log.h>
#include <commons/config.h>

#define INICIAR_PROGRAMA 1
#define FINALIZAR_PROGRAMA 2
#define DESCONECTAR_CONSOLA 3
#define LIMPIAR_MENSAJES 4
#define MANUAL 5
#define LISTA 6

#define EXIT_CODE_CORRECTO 0
#define EXIT_CODE_ERROR_RESERVA_RECURSOS -1
#define EXIT_CODE_ERROR_ARCHIVO_INEXISTENTE -2
#define EXIT_CODE_ERROR_LEER_SIN_PERMISO -3
#define EXIT_CODE_ERROR_ESCRIBIR_SIN_PERMISO -4
#define EXIT_CODE_ERROR_EXCEPCION_MEMORIA -5
#define EXIT_CODE_ERROR_DESCONEXION_CONSOLA -6
#define EXIT_CODE_ERROR_COMANDO_FINALIZAR -7
#define EXIT_CODE_ERROR_LIMITE_PAGINA -8
#define EXIT_CODE_ERROR_PAGINAS_DISPONIBLES -9
#define EXIT_CODE_ERROR_SIN_DEFINICION -20
#define EXIT_CODE_ERROR_STACKOVERFLOW -10


typedef struct hilo_params{
	void *extra;
	void *programa;
	t_list* listas;
} t_hilo_params_consol;


typedef struct programas{
	pthread_mutex_t semaforo;
	int pid;
	int sock;
	int state;
} t_programas;

t_list *listaPrograma;

void iniciarFork(t_config* config);

int leerComando(char *lineaComando);
void errorComando();
void finalizarPrograma(char *lineaComando);
void desconectarConsola();
void limpiarConsola();
void tratarCtrlC();
int conectarProgramaConKernel(t_config *config);
char* cargarPrograma(char* parametro);
int tamanioArchivo(FILE* fp);
void manuales();
int enviarMensaje(char *programa,int socket);
void listarProgramas();
void *hiloPrograma(void *params);
void crearHilo(void *extraParams, void *programa,
		void *(*funcion)(void *));
void iniciarHiloPrograma(char *lineaComando,
		t_config *config);
int avisar_al_kernel_que_ya_corte_con(t_programas *programa);
int esperoMensajesdeKernel(int socket,t_log* LOGGER,t_list * lista);
#endif /* CONSOLA_H_ */
