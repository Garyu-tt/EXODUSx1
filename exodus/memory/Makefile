CFLAGS ?= -O2
WARN = -std=c99 -Wall -Wextra -Wpedantic
PYTHON ?= python3
EXEEXT ?=

all: page_probe$(EXEEXT)

page_probe$(EXEEXT): page_codec.c page_codec.h page_probe_stats.c page_probe_stats.h page_probe.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_probe_stats.c page_probe.c $(LDLIBS)

test_codec$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h test_codec.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c test_codec.c $(LDLIBS)

test_store$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h test_store.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c test_store.c $(LDLIBS)

check: test_codec$(EXEEXT) test_store$(EXEEXT) page_probe$(EXEEXT)
	./test_codec$(EXEEXT)
	./test_store$(EXEEXT)
	$(PYTHON) test_probe.py ./page_probe$(EXEEXT)

clean:
	$(RM) test_codec$(EXEEXT) test_store$(EXEEXT) page_probe$(EXEEXT)

.PHONY: all check clean
