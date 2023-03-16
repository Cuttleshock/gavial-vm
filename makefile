SOURCE_DIR := src
BUILD_DIR := build
OBJ_DIR = $(BUILD_DIR)/objects
EXE = $(BUILD_DIR)/gavial

LINK_FLAGS := -Wall `pkg-config --static --libs deps/glfw3.pc`
COMPILE_FLAGS := -Wall -c `pkg-config --cflags deps/glfw3.pc`

SOURCES := $(wildcard $(SOURCE_DIR)/*.c)
HEADERS := $(wildcard $(SOURCE_DIR)/*.h)
OBJECTS = $(addprefix $(OBJ_DIR)/, $(notdir $(SOURCES:.c=.o)))

ifeq ($(DEBUG), true)
	LINK_FLAGS += -O0 -DDEBUG -g
	COMPILE_FLAGS += -O0 -DDEBUG -g
	BUILD_DIR := $(BUILD_DIR)/debug
else
	LINK_FLAGS += -O3
	COMPILE_FLAGS += -O3
endif

$(EXE) : $(OBJECTS)
	@gcc $^ -o $@ $(LINK_FLAGS)

$(OBJECTS) : $(OBJ_DIR)/%.o : $(SOURCE_DIR)/%.c $(HEADERS)
	@mkdir -p $(@D)
	@gcc $< -o $@ $(COMPILE_FLAGS)

clean :
	@rm -rf $(BUILD_DIR)

.PHONY : clean
