CFLAGS=-g
SO_LDFLAGS=-fPIC -shared

all: preload.so

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LIBS) -o $@

preload.so: preload.c
	$(CC) $(CFLAGS) $(SO_LDFLAGS) $< -o $@
