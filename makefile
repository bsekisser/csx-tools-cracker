TARGET = cracker

#

BUILD_DIR = build-$(shell $(CC) -dumpmachine)
TOP_DIR = $(PWD)

#

INCLUDE += -Iinclude
INCLUDE += -I../../../include
INCLUDE += -I../../csx-master/include

#

CFLAGS = -g -Wall -Wextra
CFLAGS += $(INCLUDE)
CFLAGS += -MD -MP

LDFLAGS += -lcapstone

#

SRC_DIR = source
OBJ_DIR = $(TOP_DIR)/$(BUILD_DIR)

all: $(OBJS) $(TARGET)

#

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
DEPS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.d, $(SRCS))

#

$(BUILD_DIR):
	-mkdir -p $(TOP_DIR)/$(BUILD_DIR)

$(OBJ_DIR): $(BUILD_DIR)
	-mkdir -p $(OBJ_DIR)

#

$(TARGET) : $(OBJ_DIR) $(OBJ_DIR)/$(TARGET)
	ln -s -r $(OBJ_DIR)/$(TARGET) $(TOP_DIR)/$(TARGET)

$(OBJ_DIR)/$(TARGET) : $(OBJS)
	$(CC) $(LFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJS) : $(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

#

clean:
	-rm $(OBJ_DIR)/*.d
	-rm $(OBJ_DIR)/*.o
	-rm $(OBJ_DIR)/$(TARGET)

#
	
-include $(OBJ_DIR)/*.d
