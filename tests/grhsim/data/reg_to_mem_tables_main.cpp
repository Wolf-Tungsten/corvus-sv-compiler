#include "grhsim_reg_to_mem_tables.hpp"
#include "Vreg_to_mem_tables.h"

#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>

int main() {
    GrhSIM_reg_to_mem_tables actual;
    Vreg_to_mem_tables reference;
    actual.init();
    actual.clock = reference.clock = 0;
    actual.reset = reference.reset = 0;
    actual.eval();
    reference.eval();
    std::mt19937_64 random(260907);
    for (unsigned sample = 0; sample < 8192; ++sample) {
        actual.clock = reference.clock = sample & 1;
        actual.reset = reference.reset = sample < 4 || random() % 41 == 0;
        actual.clear_useful = reference.clear_useful = random() % 17 == 0;
        actual.enable0 = reference.enable0 = random() & 1;
        actual.enable1 = reference.enable1 = random() & 1;
        actual.address0 = reference.address0 = random() & 15;
        actual.address1 = reference.address1 = sample % 3 ? random() & 15 : actual.address0;
        actual.read_address = reference.read_address = random() & 15;
        actual.window_start = reference.window_start = random() & 31;
        actual.data0 = {random(), random() & 1};
        actual.data1 = {random(), random() & 1};
        for (unsigned i = 0; i < 3; ++i) {
            reference.data0[i] = actual.data0[i / 2] >> (32 * (i % 2));
            reference.data1[i] = actual.data1[i / 2] >> (32 * (i % 2));
        }
        actual.eval();
        reference.eval();
        if (actual.phr_window != reference.phr_window || actual.useful_read0 != reference.useful_read0 ||
            actual.useful_read1 != reference.useful_read1 || actual.rename_rows != reference.rename_rows)
            throw std::runtime_error("table mismatch sample=" + std::to_string(sample));
        for (unsigned bit = 0; bit < 65; ++bit)
            if (((actual.ftq_read[bit / 64] >> (bit % 64)) & 1) !=
                ((reference.ftq_read[bit / 32] >> (bit % 32)) & 1))
                throw std::runtime_error("wide table mismatch sample=" + std::to_string(sample));
    }
    std::cout << "scalar table RTL/GrhSIM PASS samples=8192\n";
}
