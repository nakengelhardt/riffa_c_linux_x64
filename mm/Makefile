program_NAME := mm
program_C_SRCS := $(wildcard *.c)
program_CXX_SRCS := $(wildcard *.cpp)
program_C_OBJS := ${program_C_SRCS:.c=.o}
program_CXX_OBJS := ${program_CXX_SRCS:.cpp=.o}
virtmem_lib := ../virtmem_lib/riffa_virtmem.o
program_OBJS := $(program_C_OBJS) $(program_CXX_OBJS) $(virtmem_lib)
program_INCLUDE_DIRS := ../virtmem_lib/
program_LIBRARY_DIRS :=
program_LIBRARIES := riffa pthread m
CPPFLAGS += -g -Wall -std=gnu99

CPPFLAGS += $(foreach includedir,$(program_INCLUDE_DIRS),-I$(includedir))
LDFLAGS += $(foreach librarydir,$(program_LIBRARY_DIRS),-L$(librarydir))
LDFLAGS += $(foreach library,$(program_LIBRARIES),-l$(library))

.PHONY: all clean distclean

all: $(program_NAME)

$(program_NAME): $(program_OBJS)
	$(CC) $(CPPFLAGS) $(program_OBJS) -o $(program_NAME) $(LDFLAGS)

clean:
	@- $(RM) $(program_NAME)
	@- $(RM) $(program_OBJS)

distclean: clean
