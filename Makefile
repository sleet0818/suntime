CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra
BIN = suntime
SRC = suntime.c
DAYS ?= 0

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

clean:
	rm $(BIN)

msk: $(BIN)
	./$(BIN) $(DAYS) +554521+0373704

all: $(BIN)
	cat /usr/share/zoneinfo/zone.tab | awk '!/^#/ { print $$2 }' | xargs ./$(BIN) $(DAYS)

.PHONY: clean all msk
