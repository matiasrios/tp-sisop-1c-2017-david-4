#ifndef COM_COMUNICACION_H_
#define COM_COMUNICACION_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <commons/log.h>
#include <commons/collections/list.h>

#define STDIN 0

#define CARACTER_SEPARADOR "|"

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 38383

#define CK_CPU_HANDSHAKE "CK_CPU_SALUDO"
#define CK_KERNEL_HANDSHAKE "CK_KERNEL_OK"
// Mensajes entre CPU y Kernel --> valores entre 100 y 199
#define CK_ENVIAR_CONTEXTO 100
#define CK_FINALIZO_EJECUCION 101
#define CK_FINALIZO_QUANTUM 102
#define CK_PIDIO_ABRIR_ARCHIVO 103
#define CK_PIDIO_MOVER_CURSOR 104
#define CK_PIDIO_LEER_ARCHIVO 105
#define CK_PIDIO_ESCRIBIR_ARCHIVO 106
#define CK_PIDIO_CERRAR_ARCHIVO 107
#define CK_PIDIO_BORRAR_ARCHIVO 108
#define CK_FINALIZO_CON_ERROR 109
#define CK_FINALIZO_ENTRADA_SALIDA 110
#define CK_ALOCAR 111
#define KC_ALOCAR_RESPUESTA 112
#define CK_LIBERAR 113
#define KC_LIBERAR_RESPUESTA 114
#define CK_WAIT 115
#define KC_WAIT_RESPUESTA 116
#define CK_SIGNAL 117
#define  KC_SIGNAL_RESPUESTA 118
#define CK_VARIABLE_LECTURA 119
#define KC_VARIABLE_LECTURA_RESPUESTA 120
#define CK_VARIABLE_ESCRITURA 121
#define KC_VARIABLE_ESCRITURA_RESPUESTA 122
#define CK_FINALIZO_STACKOVERFLOW 123
#define KC_FINALIZO_PROGRAMA 124


#define CM_CPU_HANDSHAKE "CM_CPU_SALUDO"
#define CM_MEMORIA_HANDSHAKE "CM_MEMORIA_OK"
// Mensajes entre CPU y Memoria --> valores entre 200 y 299
#define CM_PEDIR_DATO_MEMORIA 200
#define MC_DATO_RESPUESTA 201
#define CM_GUARDAR_EN_MEMORIA 202
#define MC_GUARDAR_RESPUESTA 203

#define KF_KERNEL_HANDSHAKE "KF_KERNEL_SALUDO"
#define KF_FS_HANDSHAKE "KF_FILESYSTEM_OK"
// Mensajes entre Kernel y Filesystem --> valores entre 300 y 399

#define KF_ARCH_EXIST 300
#define KF_ABRIR_ARCH 301
#define KF_LEER_ARCH 302
#define KF_ESCRIBIR_ARCH 303
#define KF_BORRAR_ARCH 304
#define FK_ARCH_EXIST_RTA 305
#define FK_ABRIR_ARCH_RTA 306
#define FK_LEER_ARCH_RTA 307
#define FK_ESCRIBIR_ARCH_RTA 308
#define FK_BORRAR_ARCH_RTA 309

#define KM_KERNEL_HANDSHAKE "KM_KERNEL_SALUDO"
#define KM_MEMORIA_HANDSHAKE "KM_MEMORIA_OK"
// Mensajes entre Kernel y Memoria --> valores entre 400 y 499
#define KM_RESERVAR_PAGINA 400
#define MK_RESERVA_PAGINA_RESPUESTA 401
#define KM_GUARDAR_PAGINA 402
#define MK_GUARDAR_PAGINA_RESPUESTA 403
#define KM_LA_PAGINA 404
#define MK_LA_PAGINA_RESPUESTA 405
#define KM_LIBERAR_PAGINA 406
#define MK_LIBERAR_PAGINA_RESPUESTA 407
#define KM_FIN_PROCESO 408
#define MK_FIN_PROCESO_RESPUESTA 409
#define KM_CONSULTAR_PAGINA 410
#define MK_CONSULTAR_PAGINA_RESPUESTA 411


#define PK_PROGRAMA_HANDSHAKE "PK_PROGRAMA_SALUDO"
#define PK_KERNEL_HANDSHAKE "PK_KERNEL_OK"
// Mensajes entre Programa y Kernel --> valores entre 500 y 599

#define PK_ENVIAR_PROGRAMA 500
#define PK_RESPUESTA_PROGRAMA_OK 501
#define PK_FINALIZO_PROGRAMA 502
#define PK_FINALIZAR_PROGRAMA 503
#define PK_PRINT_CONSOLA 504

//Mensajes de prueba que usan los procesos de cliente -> servidor
#define PK_TEST_ENVIAR_PROGRAMA 596
#define PK_TEST_FINALIZO_PROGRMA 597
#define PK_TEST_EJECUTANDO 598
#define PK_TEST_EJECUTANDO_OK 599

typedef struct header{
	uint32_t tipo;
	uint32_t len;
} t_header;

typedef struct mensaje{
	t_header *header;
	char *body;
} t_mensaje;

typedef struct matrix_n_x_m{
	uint32_t n;
	uint32_t m;
	char *body;
	uint32_t *matriz[];
} t_matrix_n_x_m;

/* Manejo de Select */
typedef struct manejo_descriptores{
	int sock;
	void (* manejarDescriptor)(void *);
	void *parametros;
} t_manejo_descriptores;

typedef struct manejo_temporal{
	void *(* manejarEvento)(void *);
	void *parametros;
} t_manejo_temporal;

typedef struct tipo_select{
	int cantidadDescriptores;
	int maxDescriptor;
	fd_set maestroDescriptores;
	fd_set lecturaDescriptores;
	t_list *listaDescriptores;
	t_list *listaEventos;
	int tiempoOriginal;
	struct timeval tiempoRestante;
} t_select;
/* Fin manejo de Select */

// Recibe por parametro el puerto donde recibe las conexiones.
// Si retorna un valor mayor 0 es el descriptor del listener.
// Si retorna un valor menor a 0, indica que ocurrio un error.
int com_escucharEn(int port);

// Recibe por parametro la ip y el puerto a donde conectarse.
// Si retorna un valor mayor 0 es el descriptor del socket.
// Si retorna un valor menor a 0, indica que ocurrio un error.
int com_conectarseA(char *ip, int port);

// Acepta una conexion que llega al listener
// Retorna el valor del descriptor o un valor negativo en caso de error
int com_aceptarConexion(int listener);

// Recibe por parametro el descriptor del socket al cual enviar el mensaje,
// un puntero al mensaje y el tamaño del mismo
// Retorna un valor positivo con la cantidad e bytes enviados y
// en caso de error retorna un valor negativo
int com_enviar(int sock, void *mensaje, int size);

// Recibe por parametro el descriptor del socket del cual recibir el mensaje,
// un puntero a un espacio de memoria donde guardarlo, y el tamaño del mismo.
// Se asume que el espacio de memoria para guardar el mensaje ya esta reservado.
// Retorna un valor positivo con la cantidad de bytes recibidos y en
// caso de error retorna un valor negativo
int com_recibir(int sock, void *mensaje, int size);

// Cierra la conexion
void com_cerrarConexion(int sock);

int com_cpuker_handshake_cpu(int sock, t_log* logger);

int com_cpuker_handshake_kernel(int sock, t_log* logger);

int com_cpumem_handshake_cpu(int sock, t_log* logger);

int com_cpumem_handshake_memoria(int sock, t_log* logger);

int com_kerfs_handshake_kernel(int sock, t_log* logger);

int com_kerfs_handshake_filesystem(int sock, t_log* logger);

int com_kermem_handshake_kernel(int sock, t_log* logger);

int com_kermem_handshake_memoria(int sock, t_log* logger);

// Inicializa la esttructura para un nuevo mensaje
t_mensaje *com_nuevoMensaje(uint32_t tipo, t_log* logger);

// Libera la meria reservada para el mensaje
void com_destruirMensaje(t_mensaje *mensaje, t_log* logger);

// Recibe un mensaje desde sock
t_mensaje *com_recibirMensaje(int sock, t_log* logger);

// envia un mensaje a sock
int com_enviarMensaje(t_mensaje *mensaje, int sock, t_log* logger);

// agrega un campo int al mensaje con su clave asociada
uint32_t com_agregarIntAMensaje(t_mensaje *mensaje, char *clave, int32_t valor, t_log* logger);

// agrega un campo string al mensaje con su clave asociada
uint32_t com_agregarStringAMensaje(t_mensaje *mensaje, char *clave, char *valor, t_log* logger);

// agrega una matriz de int de n x m al mensaje con su clave asociada
uint32_t com_agregarMatrizNxMIntAMensaje(t_mensaje *mensaje, char *clave, t_matrix_n_x_m *valor, t_log* logger);

// agrega una pagina, este mensaje no permite que se le agreguen claves
uint32_t com_cargarPaginaAMensaje(t_mensaje *mensaje, char *pagina, uint32_t len, t_log* logger);

// leer un campo int del mensaje usando su clave asociada
int32_t com_leerIntDeMensaje(t_mensaje *mensaje, char *clave, t_log* logger);

// leer un campo string del mensaje usando su clave asociada
char *com_leerStringDeMensaje(t_mensaje *mensaje, char *clave, t_log* logger);

// leer una matriz de int de n x m  del mensaje usando su clave asociada
t_matrix_n_x_m *com_leerMatrizNxMIntDeMensaje(t_mensaje *mensaje, char *clave, t_log* logger);

// lee la pagina del mensaje. El mensaje no tiene claves.
char *com_leerPaginaDeMensaje(t_mensaje *mensaje, t_log* logger);

// retorna un string con el contenido del mensaje, util para debug
char *com_imprimirMensaje(t_mensaje *mensaje);

/* Manejo de Select */
// Crea una estructura para manejar multiples conexiones por medio de un select
// Recibe por parametro el tiempo en segundos cada cuanto realizar las tareas temporales
// Estas tareas se agregan utilizando la funcion agregarEventoTemporalAlSelect()
t_select *com_crearSelect(int tiempo);

// Hace el control del select, detectando eventos en los descriptores y ejecutando
// las acciones temporales.
void com_controlarSelect(t_select *controladorSelect);

// Crea una estructura para manejar multiples conexiones por medio de un select
void com_destruirSelect(t_select *controladorSelect);

// Agregar un descriptor al select con una funcion asociada que se ejecuta cuando
// ocurre un evento en el descriptor
void com_agregarDescriptorAlSelect(t_select *controladorSelect, int sock, void (*manejarDescriptor)(void*), void *parametros);

// Agrega una funcion que se ejecutara cada vez que se cumpla el tiempo establecido
// en el select
void com_agregarEventoTemporalAlSelect(t_select *controladorSelect, void (*manejarEvento)(void*), void *parametros);

// Remueve un descriptor del select identificandolo por el valor del mismo
void com_quitarDescriptorAlSelect(t_select *controladorSelect, int sock);
/* Fin manejo de select */

#endif
