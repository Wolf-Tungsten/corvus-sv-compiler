CPU_STAGE_DATA := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include Makefile

.PHONY: check
check: cpu_scalar_stage_test
	ASAN_OPTIONS=detect_leaks=0 ./cpu_scalar_stage_test

cpu_scalar_stage_test: $(CPU_STAGE_DATA)cpu_scalar_stage_main.cpp scalar_stage_slots.hpp libgrhsim_cpu_scalar_stage.a
	$(CXX) $(CXXFLAGS) -I. $< libgrhsim_cpu_scalar_stage.a -o $@
