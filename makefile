SRC_DIR=src
BIN_DIR=bin
DEBUG_DIR=debug
LOG_DIR=logs

all: 
	-cd $(SRC_DIR)/comunicacion && $(MAKE) all
	-cd $(SRC_DIR)/consola && $(MAKE) all
	-cd $(SRC_DIR)/cpu && $(MAKE) all
	-cd $(SRC_DIR)/file_system && $(MAKE) all
	-cd $(SRC_DIR)/kernel && $(MAKE) all
	-cd $(SRC_DIR)/memoria && $(MAKE) all

clean:
	-cd $(BIN_DIR) && $(RM) *
	-cd $(DEBUG_DIR) && $(RM) *
	-cd $(LOG_DIR) && $(RM) *

debug:
	-cd $(SRC_DIR)/comunicacion && $(MAKE) debug
	-cd $(SRC_DIR)/consola && $(MAKE) debug
	-cd $(SRC_DIR)/cpu && $(MAKE) debug
	-cd $(SRC_DIR)/file_system && $(MAKE) debug
	-cd $(SRC_DIR)/kernel && $(MAKE) debug
	-cd $(SRC_DIR)/memoria && $(MAKE) debug

consola:
	-cd $(SRC_DIR)/consola && $(MAKE) all
	-cd $(BIN_DIR) && ./consola ../cfg/consola.cfg

kernel:
	-cd $(SRC_DIR)/kernel && $(MAKE) all
	-cd $(BIN_DIR) && ./kernel ../cfg/kernel.cfg
	
cpu:
	-cd $(SRC_DIR)/cpu && $(MAKE) all
	-cd $(BIN_DIR) && ./cpu ../cfg/cpu.cfg
	
file_system:
	-cd $(SRC_DIR)/file_system && $(MAKE) all
	-cd $(BIN_DIR) && ./file_system ../cfg/file_system.cfg

memoria:
	-cd $(SRC_DIR)/memoria && $(MAKE) all
	-cd $(BIN_DIR) && ./memoria ../cfg/memoria.cfg

#cliente:
#	-cd $(SRC_DIR)/comunicacion && $(MAKE) cliente
#	-cd $(BIN_DIR) && ./cliente

#servidor:
#	-cd $(SRC_DIR)/comunicacion && $(MAKE) servidor
#	-cd $(BIN_DIR) && ./servidor


.PHONY: all create-dirs clean install uninstall debug
