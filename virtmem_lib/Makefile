lib_NAME := virtmem
lib_C_SRCS := $(lib_NAME).c
lib_C_OBJS := ${lib_C_SRCS:.c=.o}

lib_LIBRARIES := riffa pthread
CPPFLAGS += -g -Wall -std=gnu99

lib_INCLUDE_DIRS :=
lib_LIBRARY_DIRS :=

CPPFLAGS += $(foreach includedir,$(lib_INCLUDE_DIRS),-I$(includedir))
LDFLAGS += $(foreach librarydir,$(lib_LIBRARY_DIRS),-L$(librarydir))
LDFLAGS += $(foreach library,$(lib_LIBRARIES),-l$(library))

.PHONY: all clean distclean

all: $(lib_C_OBJS)

clean:
	@- $(RM) $(program_OBJS)

distclean: clean
