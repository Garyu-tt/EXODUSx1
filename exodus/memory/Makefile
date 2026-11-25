CFLAGS ?= -O2
WARN = -std=c99 -Wall -Wextra -Wpedantic
PYTHON ?= python3
EXEEXT ?=

all: page_probe$(EXEEXT) page_snapshot_probe$(EXEEXT)

# MinGW needs the wide CRT startup for the Windows probe's wmain.
ifneq (,$(findstring mingw,$(shell $(CC) -dumpmachine)))
page_probe$(EXEEXT) page_snapshot_probe$(EXEEXT): override LDFLAGS += -municode
endif

page_probe$(EXEEXT): page_codec.c page_codec.h page_probe_stats.c page_probe_stats.h page_probe.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_probe_stats.c page_probe.c $(LDLIBS)

test_codec$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h test_codec.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c test_codec.c $(LDLIBS)

test_page_store$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h test_page_store.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c test_page_store.c $(LDLIBS)

test_snapshot$(EXEEXT): page_snapshot.c page_snapshot.h page_codec.c page_codec.h test_snapshot.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_snapshot.c page_codec.c test_snapshot.c $(LDLIBS)

page_snapshot_probe$(EXEEXT): page_snapshot_probe.c page_snapshot.c page_snapshot.h page_codec.c page_codec.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_snapshot_probe.c page_snapshot.c page_codec.c $(LDLIBS)

check: test_codec$(EXEEXT) test_page_store$(EXEEXT) test_snapshot$(EXEEXT) page_probe$(EXEEXT) page_snapshot_probe$(EXEEXT)
	./test_codec$(EXEEXT)
	./test_page_store$(EXEEXT)
	./test_snapshot$(EXEEXT)
	$(PYTHON) test_probe.py ./page_probe$(EXEEXT)
	$(PYTHON) test_snapshot_probe.py ./page_snapshot_probe$(EXEEXT)

clean:
	$(RM) test_codec$(EXEEXT) test_page_store$(EXEEXT) test_snapshot$(EXEEXT) page_probe$(EXEEXT) page_snapshot_probe$(EXEEXT)

.PHONY: all check clean

test_compact_index$(EXEEXT): page_compact_index.c page_compact_index.h page_store.c page_store.h page_codec.c page_codec.h test_compact_index.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_compact_index.c page_store.c page_codec.c test_compact_index.c $(LDLIBS)

check: check-compact-index
check-compact-index: test_compact_index$(EXEEXT)
	./test_compact_index$(EXEEXT)

clean: clean-compact-index
clean-compact-index:
	$(RM) test_compact_index$(EXEEXT)

.PHONY: check-compact-index clean-compact-index
