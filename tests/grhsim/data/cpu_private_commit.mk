CPU_PRIVATE_DATA := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include Makefile

.PHONY: check
check: cpu_private_commit_test
	ASAN_OPTIONS=detect_leaks=0 prlimit --stack=8388608:8388608 -- ./cpu_private_commit_test

cpu_private_commit_test: $(CPU_PRIVATE_DATA)cpu_private_commit_main.cpp libgrhsim_cpu_private_commit.a
	$(CXX) $(CXXFLAGS) -I. $< libgrhsim_cpu_private_commit.a -o $@
