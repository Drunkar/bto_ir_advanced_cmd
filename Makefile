PROG    = bto_advanced_USBIR_cmd
LOCAL_BIN_DIR = /usr/local/bin
CC      = cc
OBJS    = ${PROG}.o

${PROG}: $(OBJS)
	$(CC) -Wall -o $@ $(OBJS) -lusb-1.0 

.c.o:
	$(CC) -Wall -c $<

clean:
	rm -f $(OBJS)
	rm -f ${PROG}

install:
	install -s -p -m 4755 ${PROG} ${LOCAL_BIN_DIR}

uninstall:
	rm -rf ${LOCAL_BIN_DIR}/${PROG}
