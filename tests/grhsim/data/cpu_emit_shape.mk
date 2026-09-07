CPU_SHAPE_DATA := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include Makefile

.PHONY: check
check: cpu_emit_shape_test
	ASAN_OPTIONS=detect_leaks=0 ./cpu_emit_shape_test

cpu_emit_shape_test: $(CPU_SHAPE_DATA)cpu_emit_shape_main.cpp libgrhsim_cpu_emit_shape.a
	$(CXX) $(CXXFLAGS) -I. $< libgrhsim_cpu_emit_shape.a -o $@
