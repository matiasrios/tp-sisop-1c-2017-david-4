/*
 * file_system.c
 *
 *  Created on: 1/4/2017
 *      Author: utnso
 */
#include "file_system.h"

int puerto;
char* puntoMontaje;
int cantBloques;
int tamBloque;
int magicNumber;
t_log* logger;
t_bitarray* bitmap;
FILE* bloqueBitmap;

int main(int argc, char **argv)
{
    logger = log_create("../logs/filesystem.log", "FILESYSTEM",true, LOG_LEVEL_INFO);
    log_info(logger, ">>>>Inicio del proceso FILESYSTEM<<<<");
    t_config* config;
    if(argc != 2)
    {
    	log_error(logger, "Error en la cantidad de parametros: %d, (filesystem arch.cfg)", argc);
    }
    config = config_create(argv[1]);
    if(config == NULL)
    {
        log_error(logger, "Error al abrir el archivo de configuracion: %s, (filesystem arch.cfg)", argv[1]);
    }
	int listenerKernel = com_escucharEn(config_get_int_value(config, "PUERTO"));
	if(listenerKernel <= 0)
	{
		log_error(logger, "Error al crear listener para el kernel en PUERTO: %d", config_get_int_value(config, "PUERTO"));
	}
	puntoMontaje = config_get_string_value(config,"PUNTO_MONTAJE");
	log_trace(logger, "Punto montaje <%s>", puntoMontaje);
	int sockKernel =com_aceptarConexion(listenerKernel);
	if(!com_kerfs_handshake_filesystem(sockKernel,logger))
	{
		log_error(logger, "Error en el handshake con el kernel");
		return -1;
	}
	log_info(logger, "Handshake con kernel OK");
	t_config* archMetadata;
	char *tempArchMetadata = malloc(100);
	sprintf(tempArchMetadata, "%sMetadata/Metadata.bin", puntoMontaje);
	archMetadata = config_create(tempArchMetadata);
	if (archMetadata != NULL)
	{
		log_info(logger, "Archivo Metadata abierto");
		tamBloque = config_get_int_value(archMetadata, "TAMANIO_BLOQUES");
		cantBloques = config_get_int_value(archMetadata, "CANTIDAD_BLOQUES");
		magicNumber = config_get_string_value(archMetadata, "MAGIC_NUMBER");
		log_trace(logger, "tamanio Bloques %d cantidadBloques %d magicNumber %s", tamBloque,cantBloques, magicNumber);
	}
	else
	{
		log_error(logger, "Error al abrir el archivo de configuracion: %s, (file system metadata.bin)", ponerPath("Metadata","/Metadata",".bin"));
	}
	//FUNCIONES
	FILE* arch_bitmap;
	char *tempArchBitmap = malloc(100);
	sprintf(tempArchBitmap, "%sMetadata/Bitmap.bin", puntoMontaje);

	log_trace(logger, "queiro abrir el archivo %s", ponerPath("Metadata","/Bitmap",".bin"));
	arch_bitmap = fopen(tempArchBitmap, "w+b");
	log_trace(logger, "punteroArchivoAbierto %d", arch_bitmap);
	int cantBytes = redondearArribaDivision(cantBloques,8);
	char* bitarray= malloc(cantBytes);
	fread(bitarray,1,cantBytes,arch_bitmap);
	bitmap = bitarray_create_with_mode(bitarray, cantBytes, 0);
	int bloqueBitmap;
	char* nombreArchivo=malloc(100);
/*	bloquesBitmap = malloc(sizeof(FILE*)*(cantBloques-1));
	for(bloqueBitmap=0;bloqueBitmap<cantBloques;bloqueBitmap++)
	{
		sprintf(nombreArchivo,"%d.bin",bloqueBitmap);
		//char *punteroArchivo = fopen(ponerPath("Bloques",nombreArchivo,""),"a+");
		bloquesBitmap[bloqueBitmap] = fopen(ponerPath("Bloques",nombreArchivo,""),"a+");
		log_trace(logger,"****nombreBitmap:%s, y su puntero es : %p",ponerPath("Bloques",nombreArchivo,""), bloquesBitmap[bloqueBitmap]);
	}*/
	while(1)
	{
		/* TODO recibir y manejar mensajes del kernel */
		t_mensaje *mensajeDelKernel = com_recibirMensaje(sockKernel, logger);
		uint32_t archivoAbierto;
		char* pathArchivo;
		int respuestaInt;
		char* respuestaString;
		char *escritura;
		char* clave= "resultado";
		uint32_t size;
		uint32_t offset;
		t_mensaje* respuestaAlKernel;
		log_trace(logger, "mensaje: %s",com_imprimirMensaje(mensajeDelKernel));
		char *claveObtenerArchivo="nombre_archivo";
		switch(mensajeDelKernel->header->tipo)
		{
			case KF_ARCH_EXIST:
				respuestaAlKernel = com_nuevoMensaje(FK_ARCH_EXIST_RTA, logger);
				log_trace(logger,"existeArchivo?");
				pathArchivo = (char*)com_leerStringAMensaje(mensajeDelKernel,"nombre_archivo" , logger);
				log_trace(logger,"mostrame el pathArchivo: %s",pathArchivo);
				respuestaInt = existeArchivo(pathArchivo);
				log_trace(logger,"existeArchivo: %d",respuestaInt);
				(uint32_t) com_agregarIntAMensaje(respuestaAlKernel,clave, respuestaInt, logger);
				log_trace(logger,"mensaje: %s",com_imprimirMensaje(respuestaAlKernel));
				com_enviarMensaje(respuestaAlKernel, sockKernel, logger);
				break;
			case KF_ABRIR_ARCH:
				pathArchivo = (char*)com_leerStringAMensaje(mensajeDelKernel, "nombre_archivo", logger);
				uint32_t respuesta = abrirArchivo(pathArchivo);
				log_trace(logger, "decime que llegaste aca la puta que te pario y dame un 1 pq el archivo esta abierto: %d", respuesta);
				respuestaAlKernel = com_nuevoMensaje(FK_ABRIR_ARCH_RTA, logger);
				com_agregarIntAMensaje(respuestaAlKernel, "resultado", respuesta, logger);
				log_trace(logger, "mostrame el mensajito porfa : %s", com_imprimirMensaje(respuestaAlKernel));
				com_enviarMensaje(respuestaAlKernel, sockKernel, logger);
				break;
			case KF_LEER_ARCH:
				pathArchivo = (char*)com_leerStringAMensaje(mensajeDelKernel, "nombre_archivo", logger);
				offset = com_leerIntAMensaje(mensajeDelKernel,"offset", logger);
				size = com_leerIntAMensaje(mensajeDelKernel, "size", logger);
				respuestaString = leerArchivo(pathArchivo, offset, size);
				log_trace(logger, "esto es lo que responde ================>%s<==========",respuestaString);
				respuestaAlKernel = com_nuevoMensaje(FK_LEER_ARCH_RTA, logger);
				com_agregarIntAMensaje(respuestaAlKernel,"size",size, logger);
				com_agregarIntAMensaje(respuestaAlKernel,"resultado",1, logger);
				if(respuestaString == NULL) {respuestaString = "-";}
				com_agregarStringAMensaje(respuestaAlKernel, "lectura", respuestaString, logger);
				com_enviarMensaje(respuestaAlKernel, sockKernel, logger);
				break;
			case KF_ESCRIBIR_ARCH:
				pathArchivo = (char*)com_leerStringAMensaje(mensajeDelKernel, claveObtenerArchivo, logger);
				offset = com_leerIntAMensaje(mensajeDelKernel,"offset", logger);
				size = com_leerIntAMensaje(mensajeDelKernel,"size", logger);
				escritura = (char*)com_leerStringAMensaje(mensajeDelKernel, "escritura", logger);
				log_trace(logger,"size: %d, offset: %d, buffer: %s",size,offset,escritura);
				respuestaInt = escribirArchivo(pathArchivo,offset, size, escritura);
				respuestaAlKernel = com_nuevoMensaje(FK_ESCRIBIR_ARCH_RTA, logger);
				com_agregarIntAMensaje(respuestaAlKernel, "resultado", respuestaInt, logger);
				com_enviarMensaje(respuestaAlKernel, sockKernel, logger);
				log_trace(logger,"mevoy");
				break;
			case KF_BORRAR_ARCH:
				log_trace(logger, "---------------1");
				pathArchivo = (char*)com_leerStringAMensaje(mensajeDelKernel, claveObtenerArchivo , logger);
				log_trace(logger, "--------------2");
				uint32_t pudoCerrar = borrarArchivo(pathArchivo);
				respuestaAlKernel = com_nuevoMensaje(FK_BORRAR_ARCH_RTA, logger);
				com_agregarIntAMensaje(respuestaAlKernel,"resultado", pudoCerrar, logger);
				com_enviarMensaje(respuestaAlKernel, sockKernel, logger);
				com_destruirMensaje(respuestaAlKernel,logger);
				break;
		}
	}
    log_info(logger, ">>>>>>>>Fin del proceso FILESYSTEM<<<<<<<<");
    log_destroy(logger);
}

bool estaLibre(off_t bitmap_index)
{
	return !bitarray_test_bit(bitmap, bitmap_index);
}

void ocuparPosicion(off_t index_bitmap)
{
	log_trace(logger,"Voy a ocupar en el BITMAP el bloque: [%d]", index_bitmap);
	bitarray_set_bit(bitmap, index_bitmap);
	actualizarBitarray();
}
void liberarPosicion(off_t index_bitmap)
{
	bitarray_clean_bit(bitmap, index_bitmap);
	actualizarBitarray();
}

char* leerArchivo(char* pathArchivo,int offset,int size)
{
	log_trace(logger, "estamos aca? llegamos %s,%d,%d", ponerPath("Archivos",pathArchivo,""),offset,size);
	t_config* archivo = config_create(ponerPath("Archivos",pathArchivo,""));

	uint32_t tamArchivo = config_get_int_value(archivo,"TAMANIO");

	size = size > tamArchivo? tamArchivo : size;
	char **bloques;
	bloques = config_get_array_value(archivo,"BLOQUES");
	log_trace(logger, "estamos aca=====|daleeee, primerbloque: %s", bloques[1]);
	int cantBloques = redondearArribaDivision(tamArchivo,tamBloque);
	log_trace(logger, "estamos aca=====|daleeee, cantBloques: %d", cantBloques);
	int bloqueALeer = offset/tamBloque;
	log_trace(logger, "estamos aca=====|daleeee, BloqueAleer: %d", bloqueALeer);
	int nuevoOffset = offset - (tamBloque * bloqueALeer);
	log_trace(logger,"nuevoOffset: %d",nuevoOffset);
	char *lecturaArchivo = malloc(size+1);
	log_trace(logger,"pntero: %p",lecturaArchivo);
	memset(lecturaArchivo, '\0', size+1);
	log_trace(logger,"adentro: %s",lecturaArchivo);
	char* contenido= malloc(tamBloque);
	memset(contenido, '\0', tamBloque);
	log_trace(logger,"pntero: %p",lecturaArchivo);
	log_trace(logger,"while: %d >= %d",size+nuevoOffset,tamBloque);
	while(size+nuevoOffset>=tamBloque)
	{
		log_trace(logger, "entre");
		int tamanioALeer = tamBloque - nuevoOffset;
		log_trace(logger, "bloque : %s", bloques[bloqueALeer]);
		bloqueBitmap = fopen(ponerPath("Bloques/",bloques[bloqueALeer], ".bin"), "a+");
		log_trace(logger, "puntero : %p", bloqueBitmap);
		fseek(bloqueBitmap, nuevoOffset, SEEK_SET);
		//fread(contenido, tamanioALeer, 1, bloqueBitmap);
		fgets(contenido,tamanioALeer+1,bloqueBitmap);
		log_trace(logger,"1erContenido: ",contenido);
		strcat(lecturaArchivo, contenido);
		bloqueALeer++;
		size = size - tamanioALeer;
		nuevoOffset = 0;
	}
	log_trace(logger, "chapa C, el bloque es: %s", bloques[bloqueALeer]);
	bloqueBitmap = fopen(ponerPath("Bloques/",bloques[bloqueALeer], ".bin"), "a+");
	log_trace(logger,"%p",bloqueBitmap);
	fseek(bloqueBitmap, nuevoOffset, SEEK_SET);
	log_trace(logger, "ya pase el seek papu, a ver si encontras el error");
	//fread(contenido, size, 1,bloqueBitmap);
	log_trace(logger,"size: %d",size);

	fgets(contenido,size+1,bloqueBitmap);
	//sprintf(contenido,)
	log_trace(logger,"1erContenido: ",contenido);
	strcat(lecturaArchivo, contenido);
	log_trace(logger, "contenido FInal: %s",lecturaArchivo);
	return lecturaArchivo;
}

int escribirArchivo(char* pathArchivo,int offset ,int size , char* buffer)
{
		t_config* archivo = config_create(ponerPath("Archivos",pathArchivo,"")); //ruta completa
		uint32_t tamArchivo = config_get_int_value(archivo,"TAMANIO");
		char **bloques = config_get_array_value(archivo,"BLOQUES"); // puntero que apunta a una lista de punteros a bloques
		int cantBloques = redondearArribaDivision(tamArchivo, tamBloque);
		int bloqueAEscribir = offset/tamBloque; //Bloque donde arranco
		int nuevoOffset = offset - (tamBloque * bloqueAEscribir); // corrimiento dentro del bloque donde arranco
		int bloquesDisponibles = cantBloquesLibres();
		int bloquesNecesarios = redondearArribaDivision(size, tamBloque);
		if(bloquesDisponibles<bloquesNecesarios){return 0;}
		FILE* bloqueSeleccionado;// = malloc(tamBloque +1);
		int resultado;
		if(nuevoOffset + size <= tamBloque) { // este caso seria el de escribir un solo bloque
			int resultado = escribirEnBloque(bloqueAEscribir, bloques, nuevoOffset, buffer, 0 , size);
			return resultado;
		} else {
			int cantAEscribir = tamBloque - nuevoOffset;
			int remanente = size;
			int loQueEscribi = 0;
			while (remanente > tamBloque) {
				resultado = escribirEnBloque(bloqueAEscribir, bloques, nuevoOffset, buffer, loQueEscribi ,tamBloque);
				remanente = remanente - cantAEscribir;
				nuevoOffset = 0;
				bloqueAEscribir++; //TODO llamar a siguiente bloque libre
				if(((bloqueAEscribir) == cantBloques) && (remanente > 0)){
					agregarBloqueAListaYActualizarTamanio(pathArchivo , bloques, bloqueAEscribir, (tamBloque+tamArchivo));
				}
				cantAEscribir = tamBloque;
				loQueEscribi = size - remanente;
			}
			if (remanente > 0) {
				resultado = escribirEnBloque(bloqueAEscribir, bloques, nuevoOffset, buffer, loQueEscribi, size);
			}
		}

		return resultado;
}

int agregarBloqueAListaYActualizarTamanio(char* pathArchivo, char** bloques, int bloqueAEscribir,int nuevoTamanio)
{
	off_t nuevoBloque = buscarLibreEnBitmap();
	char* stringNuevoBloque = malloc(100);
	sprintf(stringNuevoBloque,"%d",(int)nuevoBloque);
	if(bloqueAEscribir==0)
	{
		bloques=malloc(sizeof(char*));
	}
	else
	{
		bloques = realloc(bloques,((bloqueAEscribir + 1)*sizeof(char *))); // +1 por el corrimiento que la lista arranca en 0 y +1 por el nuevo bloque
	}
	bloques[bloqueAEscribir]=malloc(100);
	memset(bloques[bloqueAEscribir], '\0', 100);
	strcpy(bloques[bloqueAEscribir],stringNuevoBloque);
	char* concatStringBloque = (char*)string_new();
	string_append(&concatStringBloque, "[");
	char *temp = malloc(1000);
	int j;
	for(j = 0; j <bloqueAEscribir; j++)
	{
		string_append(&concatStringBloque, bloques[j]);
		string_append(&concatStringBloque, ",");
	}
	string_append(&concatStringBloque, bloques[bloqueAEscribir]);
	string_append(&concatStringBloque, "]");
	FILE* archivo = fopen(ponerPath("Archivos",pathArchivo,""),"w");
	char* temporal=malloc(1000);
	sprintf(temporal,"TAMANIO=%d\nBLOQUES=%s\n",nuevoTamanio,concatStringBloque);
	fputs(temporal,archivo);
	free(temporal);
	fclose(archivo);
	ocuparPosicion(nuevoBloque);
	return 1;
}

int borrarArchivo(char* pathArchivo)
{
	t_config* archivo = config_create(ponerPath("Archivos",pathArchivo,""));
	int tamArchivo = config_get_int_value(archivo,"TAMANIO");
	char **bloquesABorrar;
	bloquesABorrar = config_get_array_value(archivo,"BLOQUES");
	int cantidadBloques = redondearArribaDivision(tamArchivo,tamBloque);
	while(cantidadBloques > 0)
	{
		int bloqueBorrado = atoi(bloquesABorrar[cantidadBloques-1]);
		liberarPosicion(bloqueBorrado);
		cantidadBloques--;
	}
	return remove(ponerPath("Archivos",pathArchivo,""));
}

int existeArchivo(char* pathArchivo)
{
	log_trace(logger, "tirame el path, a ver si esta bien salame: %s" , ponerPath("Archivos",pathArchivo,""));
	FILE* archivo = fopen(ponerPath("Archivos",pathArchivo,""),"r");
	log_trace(logger,"entreAlaFucnion, existe? Dame el puntero: %d", archivo);
	if((int)archivo == 0){
		return 0;
	}
	int valor = 1;
	fclose(archivo);
	return valor;
}

int abrirArchivo(char* pathArchivo)
{
	if(existeArchivo(pathArchivo)==0){
		char** bloquesNuevos=NULL;
 		agregarBloqueAListaYActualizarTamanio(pathArchivo,bloquesNuevos, 0 , tamBloque);
 		return 1;
	}

	return 0;
}

off_t buscarLibreEnBitmap()
{
	off_t index_bitmap=0;
	while(!estaLibre(index_bitmap))
	{
		index_bitmap++;
	}
	return index_bitmap;
}

int redondearArribaDivision(int dividendo, int divisor)
{
	int resultadoDivision = dividendo / divisor;
	if(dividendo % divisor != 0)
	{
		resultadoDivision++;
	}
	return resultadoDivision;
}

char* ponerPath(char*directorio, char* path,char* extension)
{
	char* nuevoPath = malloc(100);
	sprintf(nuevoPath,"%s%s%s%s",puntoMontaje,directorio,path,extension);
	return nuevoPath;
}

int cantBloquesLibres(){
	off_t i = 0;
	int bloquesLibres = 0;
	while(i<(off_t)cantBloques){
		if(!bitarray_test_bit(bitmap, i)){ bloquesLibres++;}
		i++;
	}
	return bloquesLibres;
}
int escribirEnBloque(int bloqueAEscribir, char** bloques, int nuevoOffset, char* buffer,int loQueEscribi, int size)
{
	FILE* bloqueSeleccionado = fopen(ponerPath("Bloques/",bloques[bloqueAEscribir],".bin"), "r");
	char* aux = malloc(1000);
	memset(aux, '\0', 1000);
	fgets(aux,tamBloque,bloqueSeleccionado);
	memcpy(aux+nuevoOffset, buffer+loQueEscribi, size);
	fclose(bloqueSeleccionado);
	bloqueSeleccionado = fopen(ponerPath("Bloques/",bloques[bloqueAEscribir],".bin"), "w");
	int resultado=fputs(aux ,bloqueSeleccionado);
	free(aux);
	fclose(bloqueSeleccionado);
	return resultado;
}
int actualizarBitarray()
{
	FILE* actualizacion = fopen(ponerPath("Metadata","/Bitmap","bin"),"w");
	log_trace(logger,"estoy actualizando este bitmappp %p" , actualizacion);
	fwrite(bitmap->bitarray,1,bitmap->size, actualizacion);
	log_trace(logger,"estoy actualizando y lo metiii");
	fclose(actualizacion);
	log_trace(logger, "locerre");
	return 1;
}
