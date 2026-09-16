# --------------------------------------------------------------------
#  All build products (objects + binary) go to ./build/, keeping the
#  source folder clean.
#
#  Platform detection:
#  - EXE suffix follows the OS
#  - command syntax (rmdir, mkdir variants) follows the shell make
#    actually spawns: sh.exe when make is hosted by MSYS/Git Bash,
#    cmd.exe otherwise
# --------------------------------------------------------------------
CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -I../src -DUSE_ENCODE=1
LDFLAGS ?= -lm
CORE    = ../src/huffman4bit.c
TARGET  = h4b_encoder
BUILD   ?= build

OBJS    = main.o parser.o codegen.o
HDRS    = parser.h codegen.h
BINOBJS = $(addprefix $(BUILD)/,$(OBJS))

EXE :=
ifeq ($(OS),Windows_NT)
    EXE = .exe
endif

ifeq ($(OS),Windows_NT)
  ifeq (,$(findstring sh,$(SHELL)))
    # make spawned by cmd.exe -> Windows built-ins
    RMDIR = rmdir /s /q
    MKDIR = mkdir
  else
    # make spawned by sh.exe (MSYS/Git Bash hosting a Windows make)
    RMDIR = rm -rf
    MKDIR = mkdir -p
  endif
else
  RMDIR = rm -rf
  MKDIR = mkdir -p
endif

all: $(BUILD)/$(TARGET)$(EXE)

$(BUILD)/$(TARGET)$(EXE): $(BINOBJS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(BINOBJS) $(CORE) $(LDFLAGS)

$(BUILD)/%.o: %.c $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	-$(MKDIR) $(BUILD)

clean:
	-$(RMDIR) $(BUILD)

.PHONY: all clean
