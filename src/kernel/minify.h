/*
 * minify.h
 *
 *  Created on: 11/7/2017
 *      Author: utnso
 */

#ifndef MINIFY_H_
#define MINIFY_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/collections/list.h>
#include "../comunicacion/comunicacion.h"

typedef struct map_instruccion{
	uint32_t index;
	uint32_t pagina;
	uint32_t offset;
	uint32_t size;
} t_map_instruccion;

char* getCadenaContatenada(char* script);


#endif /* MINIFY_H_ */
