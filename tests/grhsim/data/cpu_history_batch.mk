CPU_HISTORY_DATA := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include Makefile
CPU_HISTORY_ASAN_OPTIONS ?= detect_leaks=0

.PHONY: check
check: cpu_history_batch_test
	ASAN_OPTIONS=$(CPU_HISTORY_ASAN_OPTIONS) prlimit --stack=8388608:8388608 -- ./cpu_history_batch_test

cpu_history_batch_test: $(CPU_HISTORY_DATA)cpu_history_batch_main.cpp libgrhsim_cpu_history_batch.a
	$(CXX) $(CXXFLAGS) -I. $< libgrhsim_cpu_history_batch.a -o $@
