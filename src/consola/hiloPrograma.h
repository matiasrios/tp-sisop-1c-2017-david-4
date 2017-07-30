/*
 * hiloPrograma.h
 *
 *  Created on: 14/4/2017
 *      Author: utnso
 */

#ifndef HILOPROGRAMA_H_
#define HILOPROGRAMA_H_

#include <pthread.h>
//#include <parser/metadata_program.h>

//void crearHiloParaPrograma(t_metadata_program *programa);
void *funcionHilo(void* e);

#endif /* HILOPROGRAMA_H_ */
