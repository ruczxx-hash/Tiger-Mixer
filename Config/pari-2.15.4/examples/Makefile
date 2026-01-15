# Generic Makefile for PARI programs -- amd64 running linux (x86-64/GMP kernel) 64-bit version
#
#  This file was created by Configure. Any change made to it will be
#  lost when Configure is run.
#
# make all will create
#  extgcd (linked with libpari)
#  libextgcd.so (to be used by "install" under GP)
#
# Under GP: install("extgcd", "GG&&", "f", "./libextgcd.so") enables
# you to subsequently use f(a,b) to call extgcd(a,b)

# change this TARGET to compile your own programs
TARGET = extgcd
SHELL  = /bin/sh

DBGFLAGS   = -g -Wall
CFLAGS     = -O3 -Wall -ffp-contract=off -fno-strict-aliasing
EXTRACFLAGS=
#CFLAGS    = $(DBGFLAGS)

CC         = /usr/bin/gcc
CPPFLAGS   = -I. -I/usr/local/include
LD         = /usr/bin/gcc
LDFLAGS    = -O3 -Wall -ffp-contract=off -fno-strict-aliasing    -Wl,--export-dynamic 
MODLD      = /usr/bin/gcc
MODLDFLAGS = -shared  $(CFLAGS) $(DLCFLAGS) -Wl,-shared 
EXTRAMODLDFLAGS = -lc -lm -L/usr/local/lib -lpari
EXTRALIBS  =

RUNPTH     = -Wl,-rpath "/usr/local/lib"
DLCFLAGS   = -fPIC
LIBS       = -lm -L/usr/local/lib -lpari

RM = rm -f

OBJS = $(TARGET).o
DYN = lib$(TARGET).so
ALL = $(TARGET) $(DYN)

all: $(ALL)

dynlib: $(DYN)

$(DYN): $(OBJS)
	$(MODLD) -o $@ $(MODLDFLAGS) $(EXTRACFLAGS) $(OBJS) $(EXTRAMODLDFLAGS)

$(TARGET): $(OBJS)
	$(LD) -o $@ $(LDFLAGS) $(EXTRACFLAGS) $< $(EXTRALIBS) $(RUNPTH) $(LIBS)

%.o: %.c
	$(CC) -c $(CFLAGS) $(EXTRACFLAGS) $(CPPFLAGS) $(DLCFLAGS) $<
clean:
	-$(RM) *.o $(ALL)
