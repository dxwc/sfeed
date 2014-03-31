# sfeed version
VERSION = 0.9

# customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# includes and libs
INCS =
LIBS = -lc

# debug
CFLAGS = -fstack-protector-all -O0 -g -ansi -Wall -Wextra -pedantic -DVERSION=\"${VERSION}\"
CFLAGS = -O0 -g -ansi -Wall -Wextra -pedantic -DVERSION=\"${VERSION}\"
LDFLAGS = ${LIBS}

# optimized
#CFLAGS = -O2 -ansi -DVERSION=\"${VERSION}\"
#LDFLAGS = -s ${LIBS}

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
