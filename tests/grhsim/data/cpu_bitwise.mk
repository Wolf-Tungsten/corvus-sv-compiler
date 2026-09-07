CPU_BITWISE_DATA := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include Makefile

.PHONY: check
check: cpu_bitwise_test
	ASAN_OPTIONS=detect_leaks=0 prlimit --stack=8388608:8388608 -- ./cpu_bitwise_test

cpu_bitwise_test: $(CPU_BITWISE_DATA)cpu_bitwise_main.cpp libgrhsim_cpu_bitwise.a
	$(CXX) $(CXXFLAGS) -I. $< libgrhsim_cpu_bitwise.a -o $@
