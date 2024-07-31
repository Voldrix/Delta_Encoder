CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = 

.SILENT: encode decode clean
.PHONY: clean all

all: encode decode

encode: delta_encode.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o delta_encode
	echo "delta_encode"

decode: delta_decode.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o delta_decode
	echo "delta_decode"

clean:
	rm -f delta_encode delta_decode

