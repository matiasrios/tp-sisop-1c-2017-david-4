/*
 * file_system.h
 *
 *  Created on: 6/4/2017
 *      Author: utnso
 */

#ifndef FILE_SYSTEM_H_
#define FILE_SYSTEM_H_

#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <commons/config.h>
#include <commons/log.h>
#include <commons/bitarray.h>
#include "../comunicacion/comunicacion.h"

bool estaLibre(off_t bitmap_index);
void ocuparPosicion(off_t index_bitmap);
void liberarPosicion(off_t index_bitmap);
char* leerArchivo(char* pathArchivo,int offset,int size);
int escribirArchivo(char* pathArchivo,int offset ,int size , char* buffer);
int agregarBloqueAListaYActualizarTamanio(char* pathArchivo, char** bloques, int bloqueAEscribir,int nuevoTamanio);
int borrarArchivo(char* pathArchivo);
off_t buscarLibreEnBitmap();
int redondearArribaDivision(int dividendo, int divisor);
char* ponerPath(char* directorio, char* path, char* extension);
int existeArchivo(char* pathArchivo);
int abrirArchivo(char* pathArchivo);

#endif /* FILE_SYSTEM_H_ */
