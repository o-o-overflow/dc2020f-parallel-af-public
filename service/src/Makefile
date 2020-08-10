# Source, Executable, Includes, Library Defines
INCL   = $(wildcard *.h)
SRC    = $(wildcard *.c)
OBJ    = $(SRC:%.c=%.o)

PLATFORM = $(shell uname)
LIBS   = -pthread

BUILDDIR = build

FORCE = $(wildcard *.force)
PROGS = $(FORCE:%.force=$(BUILDDIR)/%)
TRAP_PROGS = $(FORCE:%.force=$(BUILDDIR)/trap/%)

# If the platform is not darwin (Mac), the use the -lrt flag (needed on linux)
ifneq (,$(findstring Darwin,$(PLATFORM)))
else
	LIBS := $(LIBS) -lrt
endif

EXE    = $(BUILDDIR)/manchester
TRAP_EXE = $(BUILDDIR)/trap/manchester

# Compiler, Linker Defines
CC      = gcc
CFLAGS  = -std=gnu11 -Wall -O2
LIBPATH = -L.
LDFLAGS = -o $(EXE) $(LIBPATH) $(LIBS)
CFDEBUG = -std=gnu11 -Wall -g -DDEBUG $(LDFLAGS)
RM      = /bin/rm -f

.PHONY: clean deploy debug

# Compile and Assemble C Source Files into Object Files
%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $@

# Link all Object Files with external Libraries into Binaries
$(EXE): $(OBJ) $(PROGS) $(TRAP_EXE)
	$(CC) $(OBJ) $(LDFLAGS)

$(TRAP_EXE): $(OBJ) $(PROGS) $(BUILDDIR)/os
	mkdir $(BUILDDIR)/trap || true
	$(CC) $(SRC) -o $(TRAP_EXE) $(CFLAGS) $(LIBPATH) $(LIBS) -DENABLE_TRAP_MODE
	cp $(PROGS) $(BUILDDIR)/trap
	cat os_trap os.tass > os_trap.tass
	python assembler.py --file os_trap.tass --output $(BUILDDIR)/trap/os

# Objects depend on these Libraries
$(OBJ): $(INCL)

$(PROGS): $(FORCE) compiler.py assembler.py
	python compiler.py --file $(notdir $@).force --assembly $(notdir $@).tass --output $@

# Create a gdb/dbx Capable Executable with DEBUG flags turned on
debug: $(PROGS)
	$(CC) $(SRC) $(CFDEBUG)

# Clean Up Objects, Exectuables, Dumps out of source directory
clean:
	$(RM) $(OBJ) $(EXE) core a.out $(PROGS) $(TRAP_EXE)

deploy: $(EXE) $(PROGS) $(TRAP_EXE)
	strip $(BUILDDIR)/manchester
	cp $(BUILDDIR)/manchester ../manchester
	cp $(BUILDDIR)/manchester ../../local-tester/manchester

	python strip.py --file $(PROGS) $(TRAP_PROGS)

	cp $(PROGS) ../
	cp $(PROGS) ../../local-tester/
	cp -R build/programs ../../local-tester/
	rm -f ../parallel-af.tar.gz || true
	cp build/os ../../remote-interaction/
	cd ../ && tar -zcvf parallel-af.tar.gz manchester os && cp parallel-af.tar.gz ../patches/original.tar.gz.accepted && cd -

	strip $(TRAP_EXE)
	cd build/trap && tar -zcvf ../../../../patches/original-trap-mode.tar.gz.accepted manchester os && cd -
