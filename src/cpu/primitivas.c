/*
 * primitivas.c
 *
 *  Created on: 19/4/2017
 *      Author: utnso
 */

AnSISOP_funciones funciones = {
		.AnSISOP_definirVariable = definirVariable,
        .AnSISOP_obtenerPosicionVariable = obtenerPosicionVariable,
        .AnSISOP_dereferenciar = dereferenciar,
        .AnSISOP_asignar = asignar,
        .AnSISOP_irAlLabel = irAlLabel,
};

AnSISOP_funciones kernel = {
        .AnSISOP_reservar = alocar,
        .AnSISOP_liberar = liberar,

        .AnSISOP_abrir = abrir,
        .AnSISOP_borrar = borrar,
        .AnSISOP_cerrar = cerrar,
        .AnSISOP_moverCursor = moverCursor,
        .AnSISOP_escribir = escribir,
        .AnSISOP_leer = leer,
};

definirVariable(){
	printf("Definir Variable\n");
}

obtenerPosicionVariable(){
	printf("obtenerPosicionVariable\n");
}

dereferenciar(){
	printf("dereferenciar\n");
}

asignar(){
	printf("asignar\n");
}

irAlLabel(){
	printf("irAlLabel\n");
}

alocar(){
	printf("alocar\n");
}

liberar(){
	printf("liberar\n");
}

abrir(){
	printf("abrir\n");
}

borrar(){
	printf("borrar\n");
}

cerrar(){
	printf("cerrar\n");
}

moverCursor(){
	printf("moverCursor\n");
}

escribir(){
	printf("escribir");
}

leer(){
	printf("leer");
}
