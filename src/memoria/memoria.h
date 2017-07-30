/*
 * memoria.h
 *
 *  Created on: 6/4/2017
 *      Author: utnso
 */

#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <time.h>
#include "../comunicacion/comunicacion.h"

#define CACHE_LIBRE 0
#define CACHE_USADA 1

typedef struct elem_hiloCpu {
	int socket;
	pthread_t hilo;
} t_elem_hiloCpu;

typedef struct bloque_memoria {
	char contenido[256];
} t_bloque_memoria;

typedef struct elem_pags_invert {
	int pid;
	int pag;
	int frame;
} t_elem_pags_invert;

typedef struct elem_cache {
	int pid;
	int numPag;
	int numBloque;
	int vecesUsado;
} t_elem_cache;

// Variables Globales
t_config *config;
t_log *logger;
int listener;
char *memoria; /* Esta es mi memoria posta */
char *inicioMemoriaProcesos;
int puerto;
int marcos;
int marco_size;
int entradas_cache;
int entradas_x_proc;
int retardoMemoria;
t_elem_pags_invert *tablaPagsInvert;
t_elem_cache *memCache;
int32_t bloquesAdm;
int continuarEjecutando = 1;
int framesOcupados = 0;
t_list *listaProcActivos;

void crearLogger();
void leerArchConfig(char *nombreArchivo);
void escucharConexiones();
void inicializarSemaforos();
void destruirSemaforos();
void inicializarEstructuras();
void iniciarConsola();
void liberarEstructuras();
void crearMemoria();
void crearCache();
int cacheActivada();
void incializarListaCPU();
void crearListaProcActivos();
void crearHiloKernel(int sockKernel);
void funcionHiloKernel(int sockKernel);
int reservarPagina(int pid, int numPag);
void actualizarListaDeProcesos(int pid);
int buscarPaginaLibre();
int guardarPagina(int pid, int numPag, char* contenido);
int funcionHash(int pid, int numPag);
int verificarPosHash(int pid, int numPag, int pos);
void escribirEnBloque(int pos, char* contenido);
int liberarPagina(int pid, int numPag);
char* consultarPagina(int pid, int numPag);
int finalizarProceso(int pid);
void eliminarProcesoDeActivos(int pid);
void eliminarProcesoEnCache(int pid);
void eliminarProcesoEnMemoria(int pid);


void crearHiloCPU(int sockNuevoCPU);
void funcionHiloCPU(int sockCPU);
void sumarVecesUsada(int pos);
int estaLaPagEnCache(int pid, int numPag);
int paginaAReemplazarEnCache();
int paginaAReemplazarEnCacheDelPid(int pid);
char* irABloqueMemoria(int pos);
void aplicarRetardo();
void actualizarCache(int pos, int pid, int numPag, int posBloque);
int cantVecesPidEnCache(int pid);
int leerDatoInt(int pid, int numPag, int offset, int size);
char* leerDatoString(int pid, int numPag, int offset, int size);
int guardarDatoInt(int pid, int numPag, int offset, int size, int valor);
int guardarDatoString(int pid, int numPag, int offset, int size, char* buffer);

//Funciones de Consola Memoria

#define RETARDO 1
#define DUMP_CACHE 2
#define DUMP_EST_MEM 3
#define DUMP_CONT_MEM 4
#define FLUSH 5
#define SIZE_MEM 6
#define SIZE_PID 7

void consolaMemoria();
void homeConsola();
void leerComandos();
char* detectaIngresoConsola(char* const mensaje);
void modificarRetardo();
void sizeMemoria();
void flushCache();
void sizePID();
int calcularSizePID(int pid);
int es_entero(char *cadena);
void dumpCache();
void dumpEstrucMem();
void imprimirListaDeProcActivos(FILE* fp);
void imprimirTablaDePaginas(FILE* fp);
void dumpContMem();
void dumpEstructurasTodos(FILE *fp);
void dumpEstructurasProceso(FILE *fp);


#endif /* MEMORIA_H_ */
