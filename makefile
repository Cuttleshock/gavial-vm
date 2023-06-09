SOURCE_DIR := src
DEP_DIR := deps
BUILD_DIR := build
OBJ_DIR = $(BUILD_DIR)/objects
EXE = $(BUILD_DIR)/gavial

LINK_FLAGS := -Wall `pkg-config --static --libs $(DEP_DIR)/glfw3.pc`
COMPILE_FLAGS := -Wall -c `pkg-config --cflags $(DEP_DIR)/glfw3.pc` -I.

SOURCES := $(wildcard $(SOURCE_DIR)/*.c) $(wildcard $(SOURCE_DIR)/**/*.c) $(wildcard $(DEP_DIR)/*.c)
HEADERS := $(wildcard $(SOURCE_DIR)/*.h) $(wildcard $(SOURCE_DIR)/**/*.h) $(wildcard $(DEP_DIR)/*.h)
OBJECTS = $(addprefix $(OBJ_DIR)/, $(SOURCES:.c=.o))
COPY_FILES = $(addprefix $(BUILD_DIR)/, $(wildcard shaders/*) LICENSE predefs.ccm)

ifeq ($(DEBUG), true)
	LINK_FLAGS += -O0 -DDEBUG -DCCM_DEBUG_PRINT_TOKENS -DCCM_DEBUG_PRINT_DEFINITION -g3
	COMPILE_FLAGS += -O0 -DDEBUG -DCCM_DEBUG_PRINT_TOKENS -DCCM_DEBUG_PRINT_DEFINITION -g3
	BUILD_DIR := $(BUILD_DIR)/debug
else
	LINK_FLAGS += -O3
	COMPILE_FLAGS += -O3
endif

$(EXE) : $(OBJECTS) | $(COPY_FILES)
	@gcc $^ -o $@ $(LINK_FLAGS)

$(OBJECTS) : $(OBJ_DIR)/%.o : %.c $(HEADERS)
	@mkdir -p $(@D)
	@gcc $< -o $@ $(COMPILE_FLAGS)

$(COPY_FILES) : $(BUILD_DIR)/% : %
	@mkdir -p $(@D)
	@cp $< $@

clean :
	@rm -rf $(BUILD_DIR)

.PHONY : clean
