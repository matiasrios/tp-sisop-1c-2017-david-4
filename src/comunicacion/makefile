# Completar con sus paths
LIBRARY_COMMONS=commons

COMUNICACION_SO=../../bin/libComunicacion.so
DEBUG_COMUNICACION_SO=../../debug/libComunicacion.so
CFLAGS=-shared -fPIC
INSTALL_DIR=/usr/lib
SRCS = $(wildcard *.c)

all:
# 	-$(CC) $(CFLAGS) $(SRCS) -o $(COMUNICACION_SO) -l$(LIBRARY_COMMONS)
	-$(CC) $(CFLAGS) comunicacion.c -o $(COMUNICACION_SO) -l$(LIBRARY_COMMONS)
	sudo cp -u $(COMUNICACION_SO) $(INSTALL_DIR)
#	-$(CC) -o ../../bin/cliente cliente.c -lcommons -lComunicacion -L"../../bin"
#	-$(CC) -o ../../bin/servidor servidor.c -lcommons -lComunicacion -L"../../bin"
	
cliente:
	-$(CC) -o ../../bin/cliente cliente.c -lcommons -lComunicacion -L"../../bin"

servidor:
	-$(CC) -o ../../bin/servidor servidor.c -lcommons -lComunicacion -L"../../bin"
	
debug:
# 	-$(CC) $(CFLAGS) $(SRCS) -g -o $(COMUNICACION_SO) -l$(LIBRARY_COMMONS)
	-$(CC) $(CFLAGS) comunicacion.c -g -o $(COMUNICACION_SO) -l$(LIBRARY_COMMONS)
	-$(CC) -g -o ../../debug/cliente cliente.c -lcommons -lComunicacion -L"../../bin"
	-$(CC) -g -o ../../debug/servidor servidor.c -lcommons -lComunicacion -L"../../bin"

install:
	sudo cp -u $(COMUNICACION_SO) $(INSTALL_DIR)


# PHONY targets son los que no producen un archivo llamado como el target
# Se los marca para evitar confusion si se crea un archivo llamado asi
.PHONY: compile install uninstall clean

