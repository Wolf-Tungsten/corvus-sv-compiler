module cpu_cdc (
    input logic clock_a, clock_b, reset, inc_a, inc_b,
    output logic [7:0] count_a, count_b, b_sync1, b_sync2, a_sync1, a_sync2
);
    initial begin
        count_a = 0; count_b = 0;
        b_sync1 = 0; b_sync2 = 0; a_sync1 = 0; a_sync2 = 0;
    end
    always @(posedge clock_a or posedge reset) begin
        count_a <= reset ? 0 : count_a + inc_a;
        b_sync1 <= reset ? 0 : count_b;
        b_sync2 <= reset ? 0 : b_sync1;
    end
    always @(negedge clock_b or posedge reset) begin
        count_b <= reset ? 0 : count_b + inc_b;
        a_sync1 <= reset ? 0 : count_a;
        a_sync2 <= reset ? 0 : a_sync1;
    end
endmodule
