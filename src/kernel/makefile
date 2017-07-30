all: 
	-$(CC) -o ../../bin/kernel heapmemo.c minify.c kernel.c  -lcommons -lpthread -lComunicacion -L"../../bin"
 
debug: 
	-$(CC) -g -o ../../debug/kernel kernel.c -lcommons -lpthread -lComunicacion -L"../../bin"
 
test_minify:
	-$(CC) -o minify minify.c -lcommons
	-./minify
	
test_virtualmemo:
	-$(CC) -o virtualmemo virtualmemo.c -lcommons
	-./virtualmemo