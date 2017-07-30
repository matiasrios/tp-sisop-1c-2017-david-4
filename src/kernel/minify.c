/*
 * minify.c
 *
 *  Created on: 21/6/2017
 *      Author: utnso
 */
#include "minify.h"

char* limpiarBlancosIniciales(char *cadena);
char* limpiarBlancos(char *cadena);
t_list* scriptToList(char *script);
t_list* scriptToBloques(char* script);
t_map_instruccion* getCadenaToBloque(int inicio, char* cadena);

char* rearmarScript(char* cadena,t_list* listBloq);
char* substring(const char* str,size_t begin, size_t len);
// MINIFY
/**
 * Convierte un script en una lista de sentencias, descarta las lineas de comentarios, las lineas vacias, y espacios a la izquierda de la
 * sentencia.
 */
t_list* scriptToList(char* script) {
	t_list* lista = list_create();
	char* separador = "\r\n\t";
	char* comando = strtok(script, separador);
	comando = limpiarBlancosIniciales(comando);
	if (comando[0] != '#' && strlen(comando) > 0)
		list_add(lista, comando);
	while ((comando = strtok(NULL, separador)) != NULL) {
		comando = limpiarBlancosIniciales(comando);
		if (comando[0] != '#' && strlen(comando) > 0)
			list_add(lista, comando);
	}
	return lista;
}

t_list* scriptToBloques(char* script) {
	int i;
	t_map_instruccion* bloque;
	t_list* lista = list_create();
	t_list* lineasComando = scriptToList(script);
	//imprimirLista(lineasComando);
	int inicio = 0;

	for (i = 0; i < lineasComando->elements_count; i++) {
		char* cadena = (char*) list_get(lineasComando, i);
		bloque = getCadenaToBloque(inicio, cadena);
		bloque->index = i;
		inicio+=bloque->size;
		bloque->pagina = 0; // TODO esto no esta del todo bien pero con el offeset de la instruccion se deberia posicionar en la pagina correcta, hablar con el pola porque puede traer problemas
		list_add(lista, bloque);
	}
	return lista;
}

char* getCadenaContatenada(char* script){
	int i;
	char* concatenada=malloc(sizeof(char*));
	strcpy(concatenada, "");
	t_list* lineasComando = scriptToList(script);
	for (i = 0; i < lineasComando->elements_count; i++) {
		char* cadena = (char*) list_get(lineasComando, i);
		concatenada=realloc(concatenada,(strlen(cadena)+strlen(concatenada)+1));
		/*concatenada=*/strcat(concatenada,cadena);
	}
	return concatenada;
}

t_map_instruccion* getCadenaToBloque(int inicio, char* cadena) {
	t_map_instruccion* nBloque = (t_map_instruccion*) malloc(sizeof(t_map_instruccion));
	nBloque->offset = inicio;
	nBloque->size = strlen(cadena);
	return nBloque;
}

char* limpiarBlancos(char *cadena) {
	int i, j;
	for (i = j = 0; cadena[i] != '\0'; i++) {
		if (cadena[i] != ' ' && cadena[i] != '\t' && cadena[i] != '\n') {
			cadena[j++] = cadena[i];
		}
	}
	cadena[j] = '\0';
	return cadena;
}

char* limpiarBlancosIniciales(char *cadena) {
	int fin = 0;
	int i, j;
	for (i = j = 0; cadena[i] != '\0'; i++) {
		if ((cadena[i] != ' ' && cadena[i] != '\t' )|| fin == 1) {
			cadena[j++] = cadena[i];
			fin = 1;
		} else {
			fin = 0;
		}
	}
	cadena[j] = '\0';

	return cadena;
}

char* rearmarScript(char* cadena,t_list* listBloq){
	int i;
	t_map_instruccion* bloque;
	int tam;
	char* subCadena;
	char* concatenada=malloc(sizeof(char*));

	for (i = 0; i < listBloq->elements_count; i++) {
		bloque = (t_map_instruccion*) list_get(listBloq, i);
		tam=(sizeof(char*) * bloque->size)+(sizeof(char*) * strlen(concatenada))+1;
		concatenada=(char*)realloc(concatenada,tam);
		subCadena=substring(cadena,bloque->offset,bloque->size);
		concatenada=strcat(concatenada,subCadena);
		concatenada=strcat(concatenada,"\n");
	}
	return concatenada;
}

char* substring(const char* str,size_t begin, size_t len){
	if(str==0 || strlen(str)==0 || strlen(str)<begin || strlen(str)<(begin+len))
		return 0;
	return strndup(str+begin,len);
}

// FIN MINIFY
