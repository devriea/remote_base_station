# compiler you want to use
CC ?= gcc

# names of directories for compiled objects
BIN_DIR = bin
OBJ_DIR = obj
DEP_DIR = dep

# name of the output image
TARGET := $(BIN_DIR)/test.out

# locations of directories containing source files.
# these locations should be specified relative to the makefile location.
SRC_DIRS = \
	src \
	src/driver

# locations of directories containing header files.
# these locations should be specified relative to the makefile location.
INC_DIRS = \
	src/driver/include

# compilation flags you want to pass to the compiler
CFLAGS := 			\
	-g				\
	-Wall

# DBGFLAGS := -g

# creates the list of include flags to pass to the compiler
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
# adds the source directories to Make's search path
VPATH = $(SRC_DIRS)



# creates the list of .c source files by looking for every .c file in the source directories
CSRCS := $(foreach x, $(SRC_DIRS), $(wildcard $(addprefix $(x)/*,.c)))
# eventually we will also be looking for .S and .s assembly files in the source directories

# creates a list of all .c, .S, and .s source files in one place
SRCS := $(CSRCS)

# creates a list of object files by taking every source file, removing the file extension
# and replacing it with an "o"
OBJS := $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(notdir $(basename $(SRCS)))))
# creates a list of dependecy files by taking every source file, removing the file extension
# and replacing it with a "d"
DEPS := $(addprefix $(DEP_DIR)/, $(addsuffix .d, $(notdir $(basename $(SRCS)))))

# rule for the overall image being created. This is the default rule that is called
# if you type "make" with no arguments. The prerequisites are the object files and the existence
# of the binary directory. 
# listing pdebug as a prerequisite means it gets called everytime this rule is ran.
$(TARGET): $(OBJS) pdebug | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# rule to make object files. The prerequisites listed here do not include the header files needed
# to compile each object file. Those prerequisites are added by the dependency (.d) files.
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC_FLAGS) -c $< -o $@

# rule to make the dependency files. There is one dependency file per .c file (or is it per source file?).
# the dependency file lists all the prerequisite headers for each object (.o) file.
$(DEP_DIR)/%.d: %.c | $(DEP_DIR)
	@echo DEPEND: $(CC) -MM $(CFLAGS) $<
#  The purpose of the sed command is to translate (for example):
# main.o : main.c defs.h
# into:
# main.o main.d : main.c defs.h
	@set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $(INC_FLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJ_DIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

# rule to make the build directories if they do not already exist
$(BIN_DIR) $(OBJ_DIR) $(DEP_DIR):
	mkdir -p $@

# .PHONY targets will be run every time they are called.
# any special recipes you want to run by name should be a phony target.
.PHONY: clean pdebug

# recipe to print some debug information
pdebug:
	@echo "Source Directories = " $(SRC_DIRS)
	@echo "Include Directories = " $(INC_DIRS)

# recipe to remove the build directories and clean up the workspace
clean:
	rm -r $(BIN_DIR) $(OBJ_DIR) $(DEP_DIR)

# if we are not cleaning the workspace, include the dependency files.
# the rules in included files are combined with pre-existing rules to
# fully define the prerequisites for each target output.
ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif