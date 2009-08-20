include mk/prefix.mk

CFLAGS+= -Wall -I./include
LDFLAGS+= -L./build -lvomid

OUTFILE=build/libvomid.a
