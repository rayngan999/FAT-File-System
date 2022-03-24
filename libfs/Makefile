# Target library
lib := libfs.a
objs    := disk.o fs.o
CC      := gcc

CFLAGS  := -Wall -Wextra -Werror -MMD

all: $(lib)

ifneq ($(V),1)
Q=@
endif

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)

$(lib): $(objs)
	@echo "CC $@"
	$(Q) ar rcs $(lib) $(objs)


%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "clean"
	$(Q)rm -f $(lib) $(objs) $(deps)