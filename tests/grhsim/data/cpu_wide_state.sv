module cpu_wide_state (
    input clock, en_a, en_b, fill,
    input [1:0] address, address_b,
    input [128:0] data_a, data_b, mask,
    output reg [128:0] q, latched,
    output [128:0] memory_out, sequence_out, fill_out
);
    reg [128:0] memory [0:3];
    reg [128:0] seq_mem [0:3];
    reg [128:0] fill_mem [0:3];
    initial begin
        q = 0;
        latched = 0;
        for (integer i = 0; i < 4; i++) begin
            memory[i] = 0;
            seq_mem[i] = 0;
            fill_mem[i] = 0;
        end
    end
    always @(posedge clock) begin
        for (integer i = 0; i < 129; i++) begin
            if (en_a && mask[i]) begin
                q[i] <= data_a[i];
                memory[address][i] <= data_a[i];
            end
            if (en_b && !mask[i]) begin
                q[i] <= data_b[i];
                memory[address][i] <= data_b[i];
            end
        end
        if (en_a) seq_mem[address] <= data_a;
        if (en_b) seq_mem[address_b] <= data_b;
        if (fill) for (integer i = 0; i < 4; i++) fill_mem[i] <= data_b;
    end
    always @* begin
        for (integer i = 0; i < 129; i++)
            if (en_a && mask[i]) latched[i] = data_a[i];
    end
    assign memory_out = memory[address];
    assign sequence_out = seq_mem[address];
    assign fill_out = fill_mem[address];
endmodule
