# customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/man

# compiler and linker
CC = cc
AR = ar
RANLIB = ranlib

# debug
#CFLAGS = -fstack-protector-all -O0 -g -std=c99 -Wall -Wextra -pedantic
#LDFLAGS =

# optimized
CFLAGS = -O2 -std=c99
LDFLAGS = -s

# optimized static
#CFLAGS = -static -O2 -std=c99
#LDFLAGS = -static -s

CPPFLAGS = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE

# OpenBSD 5.9+: use pledge(2)
#CPPFLAGS += -DUSE_PLEDGE
