# customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/man

# compiler and linker
CC = cc
AR = ar
RANLIB = ranlib

# use system flags.
SFEED_CFLAGS = ${CFLAGS}
SFEED_LDFLAGS = ${LDFLAGS}
SFEED_CPPFLAGS = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE

# debug
#SFEED_CFLAGS = -fstack-protector-all -O0 -g -std=c99 -Wall -Wextra -pedantic \
#               -Wno-unused-parameter
#SFEED_LDFLAGS =

# optimized
#SFEED_CFLAGS = -O2 -std=c99
#SFEED_LDFLAGS = -s

# optimized static
#SFEED_CFLAGS = -static -O2 -std=c99
#SFEED_LDFLAGS = -static -s
