CC=gcc
CFLAGS=-g
TARGET:VirtualRouter.exe CommandParser/libcli.a pkt_gen.exe
LIBS=-lpthread -L ./CommandParser -lcli 
OBJS=glueThread/glthread.o	\
		graph.o				\
		topologies.o		\
		main.o			\
		net.o				\
		nwcli.o				\
		comm.o				\
		utils.o				\
		Layer2/layer2.o		\
		Layer2/l2switch.o	\
		Layer3/layer3.o		\
		Layer3/ip.o		    \
		Layer5/layer5.o		\
		Layer5/spf_algo/spf.o		\
		Layer5/ping.o		\
		tcpip_notif.o	 	\
		tcp_ip_trace.o		\
		tcp_ip_stack_init.o	\
		notif.o

VirtualRouter.exe:${OBJS} CommandParser/libcli.a
	${CC} ${CFLAGS} ${OBJS} -o VirtualRouter.exe ${LIBS}

pkt_gen.exe:pkt_gen.o utils.o
	$(CC) $(CFLAGS) -I tcp_public.h pkt_gen.o utils.o Layer3/ip.o -o pkt_gen.exe

pkt_gen.o:pkt_gen.c
	$(CC) $(CFLAGS) -c pkt_gen.c -I . -I Layer3 -o pkt_gen.o

main.o:main.c
	${CC} ${CFLAGS} -c main.c -I . -o main.o

glueThread/glthread.o:glueThread/glthread.c
	${CC} ${CFLAGS} -c glueThread/glthread.c -I glueThread -o glueThread/glthread.o 

graph.o:graph.c
	${CC} ${CFLAGS} -c graph.c -I . -o graph.o

topologies.o:topologies.c
	${CC} ${CFLAGS} -c topologies.c -I . -o topologies.o

net.o:net.c
	${CC} ${CFLAGS} -c net.c -I . -o net.o

nwcli.o:nwcli.c
	${CC} ${CFLAGS} -c nwcli.c -I . -I BitOp/ -o nwcli.o

comm.o:comm.c
	${CC} ${CFLAGS} -c comm.c -I . -o comm.o

utils.o:utils.c
	${CC} ${CFLAGS} -c utils.c -I . -o utils.o

tcpip_notif.o:tcpip_notif.c
	${CC} ${CFLAGS} -c tcpip_notif.c -I . -o tcpip_notif.o

tcp_ip_trace.o:tcp_ip_trace.c
	${CC} ${CFLAGS} -c tcp_ip_trace.c -I . -o tcp_ip_trace.o

tcp_ip_stack_init.o:tcp_ip_stack_init.c
	${CC} ${CFLAGS} -c tcp_ip_stack_init.c -I . -o tcp_ip_stack_init.o

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

Layer5/spf_algo/spf.o:Layer5/spf_algo/spf.c
	${CC} ${CFLAGS} -c Layer5/spf_algo/spf.c -I . -o Layer5/spf_algo/spf.o

notif.o:notif.c
	${CC} ${CFLAGS} -c notif.c -I . -o notif.o	

CommandParser/libcli.a:
	(cd CommandParser; make)

clean:
	rm *.o
	rm glueThread/glthread.o
	rm Layer2/*.o
	rm Layer3/*.o
	rm Layer5/*.o
	rm Layer5/spf_algo/*.o	
	rm *.exe
	(cd CommandParser; make clean)

all:
	make
	(cd CommandParser; make)
