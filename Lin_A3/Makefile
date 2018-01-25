CFLAGS = -c -g -Wall -std=gnu99 `pkg-config fuse --cflags --libs`

LDFLAGS = `pkg-config fuse --cflags --libs`

#SOURCES= disk_emu.c sfs_api.c sfs_test.c sfs_api.h bitmap.h bitmap.c
#SOURCES= disk_emu.c sfs_api.c sfs_test2.c sfs_api.h bitmap.c bitmap.h
SOURCES= disk_emu.c sfs_api.c fuse_wrappers.c sfs_api.h bitmap.c bitmap.h

OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE= 260561361_LIN_RINGGOLD

all: $(SOURCES) $(HEADERS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	gcc $(OBJECTS) $(LDFLAGS) -o $@

.c.o:
	gcc $(CFLAGS) $< -o $@

clean:
	rm -rf *.o *~ $(EXECUTABLE)
