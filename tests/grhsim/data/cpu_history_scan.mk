CPU_HISTORY_SCAN_DATA := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include Makefile
CPU_HISTORY_SCAN_ASAN_OPTIONS ?= detect_leaks=0
CPU_HISTORY_SCAN_ARGS ?=

.PHONY: check
check: cpu_history_scan_test
	ASAN_OPTIONS=$(CPU_HISTORY_SCAN_ASAN_OPTIONS) prlimit --stack=8388608:8388608 -- ./cpu_history_scan_test $(CPU_HISTORY_SCAN_ARGS)

cpu_history_scan_test: $(CPU_HISTORY_SCAN_DATA)cpu_history_scan_main.cpp libgrhsim_cpu_history_scan.a
	$(CXX) $(CXXFLAGS) -I. $< libgrhsim_cpu_history_scan.a -o $@
