module cpu_wide (
    input [27:0] narrow,
    input bit_in,
    input [128:0] wide,
    input [63:0] shift,
    input [128:0] other,
    input signed [128:0] signed_wide,
    input signed [66:0] signed_short,
    input signed [7:0] signed_byte,
    input [511:0] wide_shift,
    output cmp_eq, cmp_ne, cmp_lt, cmp_le, cmp_gt, cmp_ge,
    output scmp_eq, scmp_ne, scmp_lt, scmp_le, scmp_gt, scmp_ge,
    output mixed_lt, scalar_signed_lt,
    output [128:0] wide_shl, wide_lshr, wide_ashr,
    output [27:0] scalar_shl,
    output [1023:0] deep,
    output [184:0] mixed,
    output mixed_parity,
    output [447:0] shifted,
    output [128:0] sum,
    output parity,
    output [136:0] repeated
);
    assign deep = {1024{bit_in}};
    assign mixed = {narrow, wide, narrow};
    assign mixed_parity = ^mixed;
    assign shifted = {420'b0, narrow} << shift;
    assign sum = {101'b0, narrow} + {101'b0, narrow};
    assign parity = ^wide;
    assign repeated = {137{bit_in}};
    assign cmp_eq = wide == other;
    assign cmp_ne = wide != other;
    assign cmp_lt = wide < other;
    assign cmp_le = wide <= other;
    assign cmp_gt = wide > other;
    assign cmp_ge = wide >= other;
    assign scmp_eq = signed_wide == signed_short;
    assign scmp_ne = signed_wide != signed_short;
    assign scmp_lt = signed_wide < signed_short;
    assign scmp_le = signed_wide <= signed_short;
    assign scmp_gt = signed_wide > signed_short;
    assign scmp_ge = signed_wide >= signed_short;
    assign mixed_lt = signed_byte < wide;
    assign scalar_signed_lt = signed_byte < signed_wide;
    assign wide_shl = wide << wide_shift;
    assign wide_lshr = wide >> wide_shift;
    assign wide_ashr = signed_wide >>> wide_shift;
    assign scalar_shl = narrow << wide_shift;
endmodule
