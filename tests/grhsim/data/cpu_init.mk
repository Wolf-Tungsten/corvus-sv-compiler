CPU_INIT_DATA := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include Makefile
CPU_INIT_ASAN_OPTIONS ?= detect_leaks=0

.PHONY: check
check: cpu_init_test
	ASAN_OPTIONS=$(CPU_INIT_ASAN_OPTIONS) prlimit --stack=8388608:8388608 -- ./cpu_init_test

cpu_init_test: $(CPU_INIT_DATA)cpu_init_main.cpp libgrhsim_cpu_init.a
	$(CXX) $(CXXFLAGS) -I. $< libgrhsim_cpu_init.a -o $@
