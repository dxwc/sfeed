# sfeed version
VERSION = 0.8

# customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# includes and libs
INCS =
LIBEXPAT = -lexpat
LIBS = -lc

# flags
#CFLAGS = -Os -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=500 -DVERSION=\"${VERSION}\"
#LDFLAGS = -s ${LIBS}

# debug
CFLAGS = -g -O0 -pedantic -Wall -Wextra -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=700 -DVERSION=\"${VERSION}\"
LDFLAGS = ${LIBS}

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
