CC=gcc
EXT=.c
LDFLAGS= -lX11 -lasound -lXi
DEBUG_FLAGS= -DDEBUG=1 -W -Wall -ansi -pedantic -g
FLAGS= -std=c99 -Wall

PREFIX=/usr/local
MANPREFIX=${PREFIX}/share/man
SOURCE_DIR=src

PROG_NAME=dwmstatus
SRCS=\
dwmstatus.c

ifeq (${DEBUG},1)
	FLAGS+=${DEBUG_FLAGS}
endif
#FLAGS+= -I ${INCLUDE_DIR}
OBJS=${SRCS:${EXT}=.o}


${PROG_NAME}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS} ${FLAGS}

%.o: ${SOURCE_DIR}/%${EXT}
	${CC} -o $@ -c $< ${FLAGS}

install: ${PROG_NAME}
	@echo installing executable file to ${PREFIX}/bin
	@mkdir -p ${PREFIX}/bin
	@cp -f ${PROG_NAME} ${PREFIX}/bin/${PROG_NAME}
	@chmod 755 ${PREFIX}/bin/${PROG_NAME}

mrproper:
	rm -rf *.o *~ \#*\# ${PROG_NAME}

clean:
	rm -rf *.o
