
TARGET = piserver

-include exclude.mk

CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -L./lib -lm -pthread
OBJ_DIR = build/
$(shell mkdir -p $(OBJ_DIR))

SRCS = $(wildcard *c)
SUBDIR = $(filter-out $(OBJ_DIR), $(wildcard */))
SRCS_RAW= $(foreach dir, $(SUBDIR), $(wildcard $(dir)*.c))
SRCS += $(filter-out $(EXCLUDE_CFILES), $(SRCS_RAW))
VPATH = $(dir $(SRCS))

HDRS = $(wildcard *h)
HDRS_RAW= $(foreach dir, $(SUBDIR), $(wildcard $(dir)*.h))
HDRS += $(filter-out $(EXCLUDE_HFILES), $(HDRS_RAW))
INCPATH = $(addprefix -I, $(dir $(HDRS))) 


OBJS_RAW = $(patsubst %.c, %.o, $(SRCS))
OBJS_FILES = $(notdir $(OBJS_RAW))
OBJS = $(addprefix $(OBJ_DIR), $(OBJS_FILES))



.PHONY: all, install, clean, show, uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "linking $@"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)
	
$(OBJ_DIR)%.o: %.c $(HDRS)
	@echo "compiling $@ from $<"
	@$(CC) -c $< -o $@ $(CFLAGS) -Icommon -Ithpool_lib -Iapp -Itsap
	
show:
	@echo "SRCS: $(SRCS)"
	@echo "HDRS: $(HDRS)"
	@echo "OBJS: $(OBJS)"
	@echo "VPATH: $(VPATH)"
	@echo "INCPATH: $(INCPATH)"

clean:
	@rm -f $(OBJS) $(TARGET)
	
install:
	@sudo cp $(TARGET) /usr/local/bin/
	
uninstall:
	@sudo rm -f /usr/local/bin/$(TARGET)
