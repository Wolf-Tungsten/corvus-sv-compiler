CPU_STARTUP_DATA := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include Makefile
CPU_STARTUP_ASAN_OPTIONS ?= detect_leaks=0

.PHONY: check
check: cpu_startup_test
	ASAN_OPTIONS=$(CPU_STARTUP_ASAN_OPTIONS) prlimit --stack=8388608:8388608 -- ./cpu_startup_test

cpu_startup_test: $(CPU_STARTUP_DATA)cpu_startup_main.cpp libgrhsim_cpu_startup.a
	$(CXX) $(CXXFLAGS) -I. $< libgrhsim_cpu_startup.a -o $@
