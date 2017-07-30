/*
 * structkernel.h
 *
 *  Created on: 10/7/2017
 *      Author: utnso
 */

#ifndef STRUCTKERNEL_H_
#define STRUCTKERNEL_H_

#include <commons/collections/queue.h>



#define PERMISOS_MAX_SIZE 100+1
#define NOMBRE_ARCHIVO_MAX_SIZE 100+1

typedef struct heap_metadata{
	uint32_t size;
	_Bool isFree;
} t_heap_metadata;

typedef struct indiceDeEtiquetas {

	char* etiquetas;
	uint32_t ubicacion;

} t_indiceDeEtiquetas;

typedef struct tabla_global_archivo{
	uint32_t countOpen;
	char file[PERMISOS_MAX_SIZE];
} t_tabla_global_archivo;

typedef struct tabla_archivo{
	uint32_t fileDescriptor;
	char *permisos;
	t_tabla_global_archivo *globalFD;
	uint32_t lectura;
	uint32_t escritura;
} t_tabla_archivo;

typedef struct tabla_heap{
	uint32_t numPagina;
} t_tabla_heap;



typedef struct pcb{
	uint32_t pid;
	uint32_t programCounter;
	uint32_t sock;
	t_list *tablaArchivos;
	t_list *tablaHeap;
	uint32_t stackPointer;
	char *script;
	int32_t exitCode;
	//sem_t *semEjecucion;
	uint32_t quantum;
	uint32_t quantumSleep;
	int sockCPU;
	//sem_t *semaforoCPU;
	uint32_t paginasPedidas;
	t_list *instrucciones;
	uint32_t stackPagina;
	uint32_t stackOffset;
	uint32_t stackSize;
	uint32_t stackLevel;
	t_list *stack;
	uint32_t tamPagina;
	uint32_t cantidadRafagas;
	uint32_t cantInstPriv;
	uint32_t cantAccionAlocar; //suma memoria alocada
 	uint32_t cantAccionLiberar; //suma memoria liberada
 	uint32_t cantSyscalls;
 	uint32_t forzarFinalizacion;
 	uint32_t descriptor;
 	char* direccion;
 	uint32_t creacion;
 	uint32_t escritura;
 	uint32_t lectura;
 	uint32_t offsetArch;
 	uint32_t tamAEscribir;
 	t_list * indiceEtiquetas;
 	char *informacion;
 	uint32_t operacionES;
 	uint32_t descriptorNext;
 	uint32_t fuiAEntradaSalida;
 	char* nombreSemaforo;
 	uint32_t fuiAMutex;
 	uint32_t primerPaginaHeap;
	uint32_t cantPaginasHeap;
} t_pcb;

typedef struct listas_sinc{
	t_list *lista;
	//pthread_mutex_t *mutex;
} t_lista_sinc;

typedef struct listas{
	t_lista_sinc *new;
	t_lista_sinc *ready;
	t_lista_sinc *exec;
	t_lista_sinc *blocked;
	t_lista_sinc *exit;
	t_lista_sinc *cpus;
	t_lista_sinc *hilos;
	t_lista_sinc *programas;
	t_lista_sinc *archivosGlobal;
	t_lista_sinc *variablesComp;
	t_lista_sinc *semaforosComp;
} t_listas;

typedef struct hilo_params{
	t_listas *listas;
	void *extra;
} t_hilo_params;

typedef struct cpu{
	int sock;
	//sem_t *semaforo;
	int32_t activo;
	int32_t libre;
} t_cpu;

typedef struct posicion {
	uint32_t pagina;
	uint32_t offset;
	uint32_t size;
} t_posicion;

typedef struct variablesStack {
	char* id;
	t_posicion posicion;
} t_variables_stack;

typedef struct stack {
	uint32_t id;
	t_list* argumento;
	t_list* variable;
	uint32_t retPos;
	t_posicion retVar;
} t_stack;

typedef struct var_comp {
	char* nombre;
	int32_t valor;
	t_queue *pilaEspera;
} t_var_comp;


#endif /* STRUCTKERNEL_H_ */
