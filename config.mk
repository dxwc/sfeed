# customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# includes and libs
LIBS = -lc

# debug
CFLAGS = -fstack-protector-all -O0 -g -std=c99 -Wall -Wextra -pedantic \
	-DVERSION=\"${VERSION}\" -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE
LDFLAGS = ${LIBS}

# optimized
#CFLAGS = -O2 -std=c99 \
#	-DVERSION=\"${VERSION}\" -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE
#LDFLAGS = -s ${LIBS}

# optimized static
#CFLAGS = -static -O2 -std=c99 \
#	-DVERSION=\"${VERSION}\" -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE
#LDFLAGS = -static -s ${LIBS}

# uncomment for compat
#CFLAGS += -DCOMPAT
# uncomment if your libc doesn't support strlcpy
#EXTRAOBJ = strlcpy.o

# compiler and linker
#CC = cc
