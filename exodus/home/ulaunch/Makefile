CXXFLAGS ?= -O2
WARN := -std=gnu++17 -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti

check: test_catalog$(EXEEXT) test_catalog_refresh$(EXEEXT) check-launch-policy
	./test_catalog$(EXEEXT)
	./test_catalog_refresh$(EXEEXT)

test_catalog$(EXEEXT): catalog.cpp catalog.hpp test_catalog.cpp catalog_homebrew_policy.hpp catalog_homebrew_provider.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARN) -o $@ catalog.cpp test_catalog.cpp

test_catalog_refresh$(EXEEXT): catalog.cpp catalog.hpp catalog_refresh.cpp catalog_refresh.hpp test_catalog_refresh.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARN) -pthread -o $@ catalog.cpp catalog_refresh.cpp test_catalog_refresh.cpp

check-launch-policy: test_catalog_gamecard_policy$(EXEEXT)
	./test_catalog_gamecard_policy$(EXEEXT)

test_catalog_gamecard_policy$(EXEEXT): test_catalog_gamecard_policy.cpp catalog_gamecard_policy.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARN) -o $@ test_catalog_gamecard_policy.cpp

.PHONY: check check-launch-policy
