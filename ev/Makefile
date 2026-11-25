CC      ?= gcc
CFLAGS  ?= -std=c11 -O1 -Wall -Wextra -Iinclude
FW      := $(wildcard src/*.c)
SIM     := port/hal_sim.c
VARIANT ?= final
DEFS    ?=
all: out/suite_$(VARIANT)
out/suite_$(VARIANT): $(FW) $(SIM) tests/main.c tests/*.inc tests/tf.h include/*.h port/hal_sim.h
	$(CC) $(CFLAGS) $(DEFS) -DTAG=\"$(VARIANT)\" -o $@ $(FW) $(SIM) tests/main.c -lm
run: out/suite_$(VARIANT)
	./out/suite_$(VARIANT)
