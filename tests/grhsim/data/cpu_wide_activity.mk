CPU_WIDE_ACTIVITY_DATA := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include Makefile

.PHONY: check
check: cpu_wide_activity_test
	ASAN_OPTIONS=detect_leaks=0 prlimit --stack=8388608:8388608 -- ./cpu_wide_activity_test

cpu_wide_activity_test: $(CPU_WIDE_ACTIVITY_DATA)cpu_wide_activity_main.cpp libgrhsim_cpu_wide_activity.a
	$(CXX) $(CXXFLAGS) -I. $< libgrhsim_cpu_wide_activity.a -o $@
