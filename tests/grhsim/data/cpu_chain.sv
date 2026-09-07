module cpu_chain (
    input logic clock, clock_b, reset, enable,
    input logic [7:0] data,
    output logic [7:0] q1, q2, qg, qn, latched, merged, cancelled
);
    wire gated = clock & enable;
    initial begin
        q1 = 0; q2 = 0; qg = 0; qn = 0; latched = 0; merged = 0; cancelled = 0;
    end
    always @(posedge clock or posedge reset) begin
        q1 <= reset ? 0 : data;
        q2 <= reset ? 0 : q1;
    end
    always @(posedge gated or posedge reset) qg <= reset ? 0 : q2;
    always @(negedge clock_b or posedge reset) qn <= reset ? 0 : data;
    always_latch if (enable) latched = data;
    always @(posedge clock or posedge reset) merged[3:0] <= reset ? 0 : data[3:0];
    always @(posedge clock or posedge reset) merged[7:4] <= reset ? 0 : ~data[7:4];
    always @(posedge clock or posedge reset) begin
        cancelled <= reset ? 0 : data;
        cancelled <= 0;
    end
endmodule
