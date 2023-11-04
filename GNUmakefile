.PHONY: clean install

BIN_DIR ?=	/usr/local/bin
MAN_DIR ?=	/usr/share/man/man1

CFLAGS =	-Wall -Wextra -D_GNU_SOURCE
CFLAGS +=	-DEFTYPE=EBADF -D__dead="__attribute__((__noreturn__))"
LDFLAGS =	-lz

PROG =	dict
SRCS =	dict.c index.c database.c compat.c
MAN =	dict.1

$(PROG): $(SRCS)
	$(CC) $(CFLAGS) -o $(PROG) $(SRCS) $(LDFLAGS)

install: $(PROG) $(MAN)
	install -m 555 $(PROG) $(BIN_DIR)
	install -m 444 $(MAN) $(MAN_DIR)

clean:
	rm -f $(PROG)
