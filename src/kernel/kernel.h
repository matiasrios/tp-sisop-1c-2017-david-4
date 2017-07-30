/*
 * kernel.h
 *
 *  Created on: 6/4/2017
 *      Author: utnso
 */

#ifndef KERNEL_H_
#define KERNEL_H_

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
 #include <signal.h>
#include <semaphore.h>
#include <commons/config.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>

#include "../comunicacion/comunicacion.h"
#include "structkernel.h"
#include "minify.h"
#include "heapmemo.h"

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
#define OPERACION_ABRIR_ARCHIVO 103
#define OPERACION_MOVER_CURSOR 104
#define OPERACION_LEER_ARCHIVO 105
#define OPERACION_ESCRIBIR_ARCHIVO 106
#define OPERACION_CERRAR_ARCHIVO 107
#define OPERACION_BORRAR_ARCHIVO 108
#define OPERACION_WAIT_SEMAFORO 109
#define OPERACION_SIGNAL_SEMAFORO 110
#define ESPERANDO_SEMAFORO 111
#define TERMINO_ESPERA 112
#define OPERACION_ATENDIDA 113


void actualizarEstadoSistema();
void atenderES(t_listas *listas, t_pcb *proceso, int sockFS);
int validarExisteArchivo(char *nombreArch, int sockFS);
int crearArchivo(char *nombreArch, int sockFS);
int borrarArchivo(char *nombreArch, int sockFS);
int escribirArchivo(t_pcb *pcb, int sockFS, t_tabla_archivo* archPCB);
int leerArchivo(t_pcb *pcb, int sockFS, t_tabla_archivo* archPCB);

void moverProgramaDeExecAExit(t_listas *listas, int32_t pid, int sockMemoria);
void moverProgramaDeExecAReady(t_listas *listas, int32_t pid);
void moverProgramaDeExecABlocked(t_listas *listas, int32_t pid);
void pedidoDeAbrirArchivo(t_listas *listas, int32_t pid, char *pathArchivo);
void pedidoDeLeerArchivo(t_listas *listas, int32_t pid, int32_t descriptor, int32_t posInicio, int32_t largo);
void pedidoEscribirArchivo(t_listas *listas, int32_t pid, int32_t descriptor, int32_t posInicio, char *texto);
void pedidoCerrarArchivo(t_listas *listas, int32_t pid, int32_t descriptor);
void pedidoBorrarArchivo(t_listas *listas, int32_t pid, int32_t descriptor);
void finalizarProgramaConError(t_list *origen, t_list *exit, t_pcb *pcb);
void finalizarPrograma(t_listas *listas, int sock, int32_t pid, int codigoError);
void enviarProcesoACPU(t_pcb *pcb, t_cpu *cpu);
void reservarMemoriaParaProceso(t_pcb *pcb, int sockMemoria);
/* retorna el tamaño del a pagina */
int pedirPaginaParaProceso(t_pcb *pcb, int sockMemoria);
void guardarPaginaProceso(t_pcb *pcb, int sockMemoria, char *pagina, int size);
void actualizarPCB(t_pcb *pcb, t_mensaje* mensaje);
void enviarPCBAlCPU(t_pcb *pcb, int sockCPU);

void *hiloCreate(void *params);
void *hiloDispatcher(void *params);
void *hiloCPU(void *params);
void *hiloProceso(void *params);
void *hiloBlocked(void *params);
void cargarProgramaEnMemoria(t_pcb *pcb, int sockMemoria);
int nuevoPrograma(int listenerProgramas, t_listas *listas, int numeroPid, uint32_t quantum, uint32_t quantumSleep);
int nuevoCPU(int listenerCPUs, t_listas *listas);
void reservarMemoria(t_pcb *pcb, t_mensaje *mensaje);
void enviarImagenProcesoACPU(t_pcb *pcb);
void crearHilo(t_listas *listas, void *extraParams, void *(*funcion)(void *) );
void producir(t_lista_sinc *lista, t_pcb *pcb);
t_pcb *consumir(t_lista_sinc *new);
void inicializarSemaforos(int gradoMultiprogracion);
void finalizarSemaforos();
t_listas * inicializarListas();
void finalizarListas(t_listas *listas);
void finalizarCPUs(t_list *cpus);
void finalizarHilos(t_list *hilos);
void destroyPCB(void *pcb);

void mensajeAMemoriaFinalizado(int pid,int page,int sockMemoria);
void mensajeAConsolaFinalizo(int errorCode,int sockConsola);
// KERNEL CONSOLA
#define LISTA_PROCESOS 1
#define INFO_PROCESO 2
#define TABLA_GLOBAL 3
#define GRADO_MULT 4
#define FIN_PROCESO 5
#define STOP_PLANI 6
#define IMP_NEW 7
#define IMP_READY 8
#define IMP_EXEC 9
#define IMP_BLOQUED 10
#define IMP_EXIT 11
#define IMP_TODAS 12
#define IMP_VARIABLES 13

void consolaKernel();
void homeKernelConsola();
void leerComandos();
char* detectaIngresoConsola(char* const mensaje);
int detectarComandoConsola(char* comando);
void imprimirListaProcesos();
void infoProceso();
void modificarGradoMult();
void imprimirTablaGlobal();
void finalizarProceso();
void stopPlani();
void imprimirVariables();
int es_entero(char *cadena);

// FIN KERNEL CONSOLA

#endif /* KERNEL_H_ */
