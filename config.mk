# customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/man

# includes and libs
LIBS = -lc

# debug
#CFLAGS = -fstack-protector-all -O0 -g -std=c99 -Wall -Wextra -pedantic \
#	-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE
#LDFLAGS = ${LIBS}

# optimized
CFLAGS = -O2 -std=c99 \
	-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE
LDFLAGS = -s ${LIBS}

# optimized static
#CFLAGS = -static -O2 -std=c99 \
#	-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE
#LDFLAGS = -static -s ${LIBS}

# compiler and linker
#CC = cc
