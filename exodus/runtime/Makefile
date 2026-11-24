CFLAGS ?= -O2
WARN = -std=c99 -Wall -Wextra -Wpedantic
PYTHON ?= python3
EXEEXT ?=

test_autorun_probe$(EXEEXT): autorun_probe.c autorun_probe.h test_autorun_probe.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ autorun_probe.c test_autorun_probe.c $(LDLIBS)

check: test_autorun_probe$(EXEEXT)
	$(PYTHON) test_autorun_probe.py ./test_autorun_probe$(EXEEXT)

clean:
	$(RM) test_autorun_probe$(EXEEXT)

.PHONY: check clean
