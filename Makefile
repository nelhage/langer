udis86=/home/nelhage/sw/udis86

LIBS=-lbfd -L$(udis86)/lib -ludis86
CFLAGS=-I$(udis86)/include

all: replace-ret

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LIBS) -o $@

