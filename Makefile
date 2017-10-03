
CFLAGS= 
COPT= -O2 -fomit-frame-pointer -std=c89
CWARN= -Wall -Wextra -Wredundant-decls
CWARNIGNORE= -Wno-unused-result -Wno-strict-aliasing
CDEF= 

ifdef debug
CFLAGS+= -O0 -g -Wno-format -fno-omit-frame-pointer
CDEF+= -DDEBUG
endif

_OBJECTS= main opt

OBJECTS= $(patsubst %,build/%.o,$(_OBJECTS))

##############################################################################
# Core Linker flags
##############################################################################
LFLAGS= 
LDYNAMIC= -lpfs
LSTATIC= 

##############################################################################
# Util
##############################################################################
Q= @
E= @echo -e
RM= rm -f 

##############################################################################
# Build rules
##############################################################################
.PHONY: default install all clean

default all: pfs

pfs: $(OBJECTS)
	$(E) "Linking $@"
	$(Q)$(CC) -o $@ $^ $(LSTATIC) $(LDYNAMIC) $(LFLAGS)

build/%.o: %.c $($(CC) -M src/%.c)
	$(E) "\e[0;32mCC     $@\e(B\e[m"
	$(Q)$(CC) -c -o $@ $< $(CDEF) $(COPT) $(CWARN) $(CWARNIGNORE) $(CFLAGS)

clean:
	$(Q)$(RM) build/*.o
	$(Q)$(RM) pfs
	$(E) "Cleaned build directory"

install:
	cp pfs /usr/local/bin/
