module cpu_dual_ram (
    input logic clock_a, clock_b, reset, en_a, en_b,
    input logic [2:0] addr_a, addr_b, probe,
    input logic [15:0] data_a, data_b, mask_a, mask_b,
    output logic [15:0] q_a, q_b, observed
);
    logic [15:0] memory [0:7];
    initial begin
        q_a = 0; q_b = 0;
        for (int i = 0; i < 8; i++) memory[i] = 0;
    end
    always @(posedge clock_a) if (!reset && en_a)
        memory[addr_a] <= (memory[addr_a] & ~mask_a) | (data_a & mask_a);
    always @(posedge clock_b) if (!reset && en_b)
        memory[addr_b] <= (memory[addr_b] & ~mask_b) | (data_b & mask_b);
    always @(posedge clock_a or posedge reset) q_a <= reset ? 0 : memory[addr_a];
    always @(posedge clock_b or posedge reset) q_b <= reset ? 0 : memory[addr_b];
    assign observed = memory[probe];
endmodule
