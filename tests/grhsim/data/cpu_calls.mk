CPU_CALLS_DATA := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include Makefile

.PHONY: check
check: cpu_calls_test
	ASAN_OPTIONS=detect_leaks=0 ./cpu_calls_test
	@ASAN_OPTIONS=detect_leaks=0 ./cpu_calls_test --finish > finish.log 2>&1; status=$$?; test $$status -eq 7
	@ASAN_OPTIONS=detect_leaks=0 ./cpu_calls_test --fatal > fatal.log 2>&1; status=$$?; test $$status -eq 11
	@rg -q '\[fatal\] expected fatal' fatal.log

cpu_calls_test: $(CPU_CALLS_DATA)cpu_calls_main.cpp libgrhsim_cpu_calls.a
	$(CXX) $(CXXFLAGS) -I. $< libgrhsim_cpu_calls.a -o $@
