CC=gcc
CFLAGS=-g
TARGET:test.exe
LIBS=-lpthread -L ./CommandParser -lcli 
OBJS=glueThread/glthread.o	\
		graph.o				\
		topologies.o		\
		testapp.o			\
		net.o				\
		nwcli.o				\
		comm.o				\
		utils.o				\
		Layer2/layer2.o		\
		Layer2/l2switch.o	\
		Layer3/layer3.o		\
		Layer3/ip.o		    \
		Layer5/layer5.o		\
		Layer5/ping.o			

test.exe:${OBJS} CommandParser/libcli.a
	${CC} ${CFLAGS} ${OBJS} -o test.exe ${LIBS}

testapp.o:testapp.c
	${CC} ${CFLAGS} -c testapp.c -I . -o testapp.o

glueThread/glthread.o:glueThread/glthread.c
	${CC} ${CFLAGS} -c glueThread/glthread.c -I glueThread -o glueThread/glthread.o 

graph.o:graph.c
	${CC} ${CFLAGS} -c graph.c -I . -o graph.o

topologies.o:topologies.c
	${CC} ${CFLAGS} -c topologies.c -I . -o topologies.o

net.o:net.c
	${CC} ${CFLAGS} -c net.c -I . -o net.o

nwcli.o:nwcli.c
	${CC} ${CFLAGS} -c nwcli.c -I . -o nwcli.o

comm.o:comm.c
	${CC} ${CFLAGS} -c comm.c -I . -o comm.o

utils.o:utils.c
	${CC} ${CFLAGS} -c utils.c -I . -o utils.o

Layer2/layer2.o:Layer2/layer2.c
	${CC} ${CFLAGS} -c Layer2/layer2.c -I . -o Layer2/layer2.o

Layer2/l2switch.o:Layer2/l2switch.c
	${CC} ${CFLAGS} -c Layer2/l2switch.c -I . -o Layer2/l2switch.o	

Layer3/layer3.o:Layer3/layer3.c
	${CC} ${CFLAGS} -c Layer3/layer3.c -I . -o Layer3/layer3.o

Layer3/ip.o:Layer3/ip.c
	${CC} ${CFLAGS} -c Layer3/ip.c -I . -o Layer3/ip.o

Layer5/ping.o:Layer5/ping.c
	${CC} ${CFLAGS} -c Layer5/ping.c -I . -o Layer5/ping.o	

Layer5/layer5.o:Layer5/layer5.c
	${CC} ${CFLAGS} -c Layer5/layer5.c -I . -o Layer5/layer5.o	

CommandParser/libcli.a:
	(cd CommandParser; make)

clean:
	rm *.o
	rm glueThread/glthread.o
	rm Layer2/*.o
	rm Layer3/*.o
	rm Layer5/*.o
	rm *.exe
	(cd CommandParser; make clean)

all:
	make
	(cd CommandParser; make)
