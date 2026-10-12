# --------------------------------------------------------------------
#  badapple_player - terminal player: original vs decoded frames side by side
#
#  Consumes the shared asset shipped in ../assets/ (badapple_h4b.c/.h,
#  produced by tools/build/h4b_encoder - see ../README.md).
# --------------------------------------------------------------------
CC     ?= gcc

# NOTE: no -DUSE_ENCODE -> exercises the MCU decode-only path
# -I../assets pulls in badapple.c / badapple_h4b.* (shared resources)
CFLAGS = -Wall -Wextra -O2 -I../../src -I../assets

TARGET = badapple_player
SRC    = $(TARGET).c
CORE   = ../../src/huffman4bit.c
GEN_C  = ../assets/badapple_h4b.c

EXE :=
ifeq ($(OS),Windows_NT)
    EXE := .exe
endif

RM := rm -f
ifeq ($(OS),Windows_NT)
  ifeq (,$(findstring sh,$(SHELL)))
    RM := del /q /f
  endif
endif

.PHONY: all run clean

all: $(TARGET)$(EXE)

$(TARGET)$(EXE): $(SRC) $(CORE) $(GEN_C)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(CORE) -lm

run: $(TARGET)$(EXE)
	./$(TARGET)$(EXE)

clean:
	-$(RM) $(TARGET)$(EXE)
