CFLAGS += -Os

LDFLAGS += -lcapstone

LDLIBS += -Lgit/libarm -larm
LDLIBS += -Lgit/libbse -lbse
LDLIBS += -lcapstone

SRC_DIR = source
SRCS = $(wildcard $(SRC_DIR)/*.c)

TARGET = cracker
TARGET_EXE = $(TARGET)



include git/libbse/makefile.setup



all: $(TARGET_EXE)



include git/libbse/makefile.build
