CFILES := $(shell find . -type f -name '*.c')

build:
	zig cc --target=x86_64-freestanding-none -O3 -I../../../lib/libzorro -I../../../lib/libraven -c $(CFILES) ../../../lib/libzorro/libzorro.a ../../../lib/libraven/libraven.a ../../../lib/libzorro/crt0.o -o welcome.o
	zig cc --target=x86_64-freestanding-none -O3 -nostdlib -T../../../lib/link-scripts/x86_64.ld welcome.o -o ../../../root/bin/welcome
	chmod 777 ../../../root/bin/welcome
	rm welcome.o