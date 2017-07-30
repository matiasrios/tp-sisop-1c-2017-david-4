/*
 * cpu.h
 *
 *  Created on: 6/4/2017
 *      Author: utnso
 */

#ifndef CPU_H_
#define CPU_H_

#include <stdio.h>
#include <commons/log.h>
#include <commons/config.h>
#include <string.h>

#include "../comunicacion/comunicacion.h"

#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <parser/parser.h>
#include <parser/metadata_program.h>
#include <commons/collections/queue.h>
#include <stdarg.h>

#include <math.h>

#define EJECUTANDO 1200

typedef struct posicion {
	uint32_t pagina;
	uint32_t offset;
	uint32_t size;
} t_posicion;

typedef struct varsStack {
	char id;
	t_posicion posicion;
} t_vars_stack;

typedef struct argsStack {
	char id;
	t_posicion posicion;
}t_args_stack;

typedef struct leerMemoria {
	int pag;
	int offset;
	int tamanio;
} t_leerMemoria;

typedef struct indiceCod {
	uint32_t pagina;
	uint32_t offset;
	uint32_t size;
} t_codigo;

typedef struct stack {
	uint32_t id;
	t_list* argumento;
	t_list* variable;
	uint32_t retPos;
	t_posicion retVar;
} t_stack;

typedef struct indiceDeEtiquetas {

	char* etiquetas;
	uint32_t ubicacion;

} t_indiceDeEtiquetas;

typedef struct pcbActivo {
	uint32_t pid;
	uint32_t programCounter;
	uint32_t quantum;
	uint32_t estadoEnCPU;
	t_posicion stackPointer;
	uint32_t stackLevel;
	t_list* indiceCodigo;
	t_list* indiceEtiquetas;
	t_list* indiceStack;
	uint32_t tamPagina;
	uint32_t quantumRetardo;

	char* direccion;
	uint32_t lectura;
	uint32_t escritura;
	uint32_t creacion;
	uint32_t descriptor;
	uint32_t offsetArch;

	char* informacionEscribir;
	char* informacionLeer;
	uint32_t tamAEscribir;

	uint32_t cantidadRafagas;

	uint32_t volvioEntradaSalida;

	char* algoritmo;
	uint32_t stackMaxSize;

	char* nombreSemaforo;

	uint32_t primerPaginaHeap;

} t_pcbActivo;

typedef struct primitivaKernel{
	char* id;
	int valor;
} t_primitivaKernel;


int sockMemoria;
t_pcbActivo* pcbActivo;
t_log* logger;
int sockKernel;
int continuarEjecutando = 0;
int abortar = 0;
int sigusr1 = 0;
int voyAEntradaSalida;
int contadorDesreferenciar = 0 ;
construccion = 0;


///////////////////////////////////////// PRIMITIVAS ////////////////////////////////////////////////////////

t_puntero definirVariable(t_nombre_variable);
t_puntero obtenerPosicionVariable(t_nombre_variable);
t_valor_variable dereferenciar(t_puntero);
void asignar(t_puntero, t_valor_variable);
void irAFuncion(t_nombre_etiqueta nombre_etiqueta);
void irAlLabel(t_nombre_etiqueta nombre_etiqueta);
t_puntero alocar(t_valor_variable);
void liberar(t_puntero);
t_descriptor_archivo abrir(t_direccion_archivo, t_banderas);
void borrar(t_descriptor_archivo);
void cerrar(t_descriptor_archivo);
void moverCursor(t_descriptor_archivo, t_valor_variable);
void escribir(t_descriptor_archivo, void*, t_valor_variable);
void leer(t_descriptor_archivo, t_puntero, t_valor_variable);

void cargarPCB(t_mensaje* mensaje);
void almacenarEnMemoriaUnValor(int pid, t_posicion pos, int valor, char* texto, int tipo);
char* pedirAMemoriaString(int pagina, int offset, int size);
int pedirAMemoriaEntero(int pagina, int offset, int size);
void ejecutarRafaga();
void respoderAlKernel(int valor);
void crearLogger();
void saludar();
void imprimirConexionKernel(int sockKernel, int sockMemoria);
void actualizarStackPointer();
void finalizar();
t_valor_variable obtenerValorCompartida(t_nombre_compartida variable);
t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor);
void wait(t_nombre_semaforo identificador_semaforo);
void signalUP(t_nombre_semaforo identificador_semaforo);
void llamarSinRetorno(t_nombre_etiqueta etiqueta);
void llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar);
void retornar(t_valor_variable retorno);
void capturarSigusr1();
void desconectarse();

int	villereada(t_nombre_etiqueta nombre_etiqueta, int flag);

#endif /* CPU_H_ */

