/*
 * heapmemo.h
 *
 *  Created on: 10/7/2017
 *      Author: utnso
 */

#ifndef HEAPMEMO_H_
#define HEAPMEMO_H_
#include <stdio.h>
#include <stdlib.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include "../comunicacion/comunicacion.h"
#include "structkernel.h"

typedef struct bloque_heap {
	_Bool isUsed;
	uint32_t size;
	uint32_t puntero;
} t_bloque_heap;



void* liberarMemoriaHeap(t_cpu* cpu, t_pcb* pcbCPU, t_mensaje *mensajeDeCPU,
		int sockMemoria, t_log* logger);
int pedirTamanioAMemoria(int pid, int numPag, int sockMemoria);

int enviarMensajeAMemoria(int pid,int numPage,int sockMemoria,t_log *logger);
void _enviarMensajeACPU(int resultado,int posicion,int sockCPU);
int reservarMemoriaHeap(t_cpu* cpu, t_pcb* pcbCPU, t_mensaje *mensajeDeCPU, int sockMemoria,
		t_log* logger, uint32_t tamanioPagina);

#endif /* HEAPMEMO_H_ */
