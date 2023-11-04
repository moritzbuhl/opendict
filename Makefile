CFLAGS +=	-Wall
LDADD +=	-lz
DPADD +=	${LIBZ}

PROG =	dict
SRCS =	main.c index.c database.c
MAN =	dict.1

.include <bsd.prog.mk>
