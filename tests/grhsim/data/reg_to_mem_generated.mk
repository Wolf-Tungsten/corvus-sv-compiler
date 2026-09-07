include Makefile

.PHONY: check
check: reg_to_mem_generated_test
	ASAN_OPTIONS=detect_leaks=0 ./reg_to_mem_generated_test

reg_to_mem_generated_test: main.cpp $(LIB)
	$(CXX) $(CXXFLAGS) -I. $< $(LIB) -o $@
