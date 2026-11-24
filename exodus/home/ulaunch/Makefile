CXXFLAGS ?= -O2
WARN := -std=c++17 -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti

check: test_catalog$(EXEEXT)
	./test_catalog$(EXEEXT)

test_catalog$(EXEEXT): catalog.cpp catalog.hpp test_catalog.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARN) -o $@ catalog.cpp test_catalog.cpp

.PHONY: check
