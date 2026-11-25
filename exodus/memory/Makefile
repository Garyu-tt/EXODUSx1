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

test_page_dedup_store$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h page_dedup_store.c page_dedup_store.h test_page_dedup_store.c bdi_slab.c bdi_slab.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c page_dedup_store.c test_page_dedup_store.c bdi_slab.c $(LDLIBS)

test_store$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h test_store.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c test_store.c $(LDLIBS)

check: test_store$(EXEEXT) test_page_dedup_store$(EXEEXT) test_codec$(EXEEXT) test_page_store$(EXEEXT) test_snapshot$(EXEEXT) page_probe$(EXEEXT) page_snapshot_probe$(EXEEXT)
	./test_codec$(EXEEXT)
	./test_page_store$(EXEEXT)
	./test_page_dedup_store$(EXEEXT)
	./test_store$(EXEEXT)
	./test_snapshot$(EXEEXT)
	$(PYTHON) test_probe.py ./page_probe$(EXEEXT)
	$(PYTHON) test_snapshot_probe.py ./page_snapshot_probe$(EXEEXT)

clean:
	$(RM) test_store$(EXEEXT) test_page_dedup_store$(EXEEXT) test_codec$(EXEEXT) test_page_store$(EXEEXT) test_snapshot$(EXEEXT) page_probe$(EXEEXT) page_snapshot_probe$(EXEEXT)

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

test_page_read$(EXEEXT): page_store.c page_codec.c page_read.c page_read.h page_codec.h page_store.h page_dedup_store.h test_page_read.c bdi_slab.c bdi_slab.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_store.c page_codec.c page_read.c test_page_read.c bdi_slab.c $(LDLIBS)

check: check-direct-read
check-direct-read: test_page_read$(EXEEXT)
	./test_page_read$(EXEEXT)

clean: clean-direct-read
clean-direct-read:
	$(RM) test_page_read$(EXEEXT)

.PHONY: check-direct-read clean-direct-read

test_page_cow$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h page_dedup_store.c page_dedup_store.h page_read.c page_read.h test_page_cow.c bdi_slab.c bdi_slab.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c page_dedup_store.c page_read.c test_page_cow.c bdi_slab.c $(LDLIBS)

check: check-page-cow
check-page-cow: test_page_cow$(EXEEXT)
	./test_page_cow$(EXEEXT)

clean: clean-page-cow
clean-page-cow:
	$(RM) test_page_cow$(EXEEXT)

.PHONY: check-page-cow clean-page-cow

test_page_bdi$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h page_dedup_store.c page_dedup_store.h page_read.c page_read.h page_snapshot.c page_snapshot.h test_page_bdi.c bdi_slab.c bdi_slab.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c page_dedup_store.c page_read.c page_snapshot.c test_page_bdi.c bdi_slab.c $(LDLIBS)

check: check-page-bdi
check-page-bdi: test_page_bdi$(EXEEXT)
	./test_page_bdi$(EXEEXT)

clean: clean-page-bdi
clean-page-bdi:
	$(RM) test_page_bdi$(EXEEXT)

.PHONY: check-page-bdi clean-page-bdi

test_page_pool$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h page_dedup_store.c page_dedup_store.h page_read.c page_read.h page_compact_index.c page_compact_index.h page_integrity_store.c page_integrity_store.h test_page_pool.c bdi_slab.c bdi_slab.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c page_dedup_store.c page_read.c page_compact_index.c page_integrity_store.c test_page_pool.c bdi_slab.c $(LDLIBS)

check: check-page-pool
check-page-pool: test_page_pool$(EXEEXT)
	./test_page_pool$(EXEEXT)

clean: clean-page-pool
clean-page-pool:
	$(RM) test_page_pool$(EXEEXT)

.PHONY: check-page-pool clean-page-pool

test_extent_index$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h test_extent_index.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c test_extent_index.c $(LDLIBS)

test_dedup_index$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h page_dedup_store.c page_dedup_store.h test_dedup_index.c bdi_slab.c bdi_slab.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c page_dedup_store.c test_dedup_index.c bdi_slab.c $(LDLIBS)

check: check-extent-index
check-extent-index: test_extent_index$(EXEEXT) test_dedup_index$(EXEEXT)
	./test_extent_index$(EXEEXT)
	./test_dedup_index$(EXEEXT)

clean: clean-extent-index
clean-extent-index:
	$(RM) test_extent_index$(EXEEXT) test_dedup_index$(EXEEXT)

.PHONY: check-extent-index clean-extent-index

test_page_discard$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h page_dedup_store.c page_dedup_store.h test_page_discard.c bdi_slab.c bdi_slab.h
	$(CC) $(CPPFLAGS) -D_DEFAULT_SOURCE $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c page_dedup_store.c test_page_discard.c bdi_slab.c $(LDLIBS)

check: check-page-discard
check-page-discard: test_page_discard$(EXEEXT)
	./test_page_discard$(EXEEXT)

clean: clean-page-discard
clean-page-discard:
	$(RM) test_page_discard$(EXEEXT)

.PHONY: check-page-discard clean-page-discard

test_pool_compact32$(EXEEXT): page_codec.c page_store.c page_dedup_store.c page_read.c test_pool_compact32.c page_store.h page_dedup_store.h page_read.h page_codec.h bdi_slab.c bdi_slab.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c page_dedup_store.c page_read.c test_pool_compact32.c bdi_slab.c $(LDLIBS)

check: check-pool-compact32
check-pool-compact32: test_pool_compact32$(EXEEXT)
	./test_pool_compact32$(EXEEXT)

clean: clean-pool-compact32
clean-pool-compact32:
	$(RM) test_pool_compact32$(EXEEXT)

.PHONY: check-pool-compact32 clean-pool-compact32

test_local_patch$(EXEEXT): page_codec.c page_codec.h page_store.c page_store.h page_dedup_store.c page_dedup_store.h page_read.c page_read.h test_local_patch.c bdi_slab.c bdi_slab.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c page_dedup_store.c page_read.c test_local_patch.c bdi_slab.c $(LDLIBS)

check: check-local-patch
check-local-patch: test_local_patch$(EXEEXT)
	./test_local_patch$(EXEEXT)

clean: clean-local-patch
clean-local-patch:
	$(RM) test_local_patch$(EXEEXT)

.PHONY: check-local-patch clean-local-patch

test_bdi_slab$(EXEEXT): page_codec.c page_codec.h bdi_slab.c bdi_slab.h test_bdi_slab.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c bdi_slab.c test_bdi_slab.c $(LDLIBS)

check: check-bdi-slab
check-bdi-slab: test_bdi_slab$(EXEEXT)
	./test_bdi_slab$(EXEEXT)

clean: clean-bdi-slab
clean-bdi-slab:
	$(RM) test_bdi_slab$(EXEEXT)

.PHONY: check-bdi-slab clean-bdi-slab

test_hot_store$(EXEEXT): page_codec.c page_store.c page_dedup_store.c page_read.c bdi_slab.c test_hot_store.c page_codec.h page_store.h page_dedup_store.h page_read.h bdi_slab.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ page_codec.c page_store.c page_dedup_store.c page_read.c bdi_slab.c test_hot_store.c $(LDLIBS)

check: check-hot-store
check-hot-store: test_hot_store$(EXEEXT)
	./test_hot_store$(EXEEXT)

clean: clean-hot-store
clean-hot-store:
	$(RM) test_hot_store$(EXEEXT)

.PHONY: check-hot-store clean-hot-store
