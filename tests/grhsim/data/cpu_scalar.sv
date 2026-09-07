module cpu_scalar (
    input logic [63:0] a, b,
    input logic signed [63:0] sa, sb,
    input logic [7:0] sm,
    output logic [63:0] out_add, out_sub, out_mul, out_div, out_mod,
    out_and, out_or, out_xor, out_xnor, out_shl, out_lshr, out_not, out_assign, out_mux,
    out_sdiv, out_smod, out_sashr,
    out_eq, out_ne, out_caseEq, out_caseNe,
    out_lt, out_le, out_gt, out_ge, out_logicAnd, out_logicOr,
    out_slt, out_sle, out_sgt, out_sge,
    out_reduceAnd, out_reduceOr, out_reduceXor, out_reduceNand, out_reduceNor, out_reduceXnor, out_logicNot,
    out_concat, out_replicate,
    out_static, out_dynamic, out_array
);
    assign out_add = a+b;
    assign out_sub = a-b;
    assign out_mul = a*b;
    assign out_div = a/b;
    assign out_mod = a%b;
    assign out_and = a&b;
    assign out_or = a|b;
    assign out_xor = a^b;
    assign out_xnor = a~^b;
    assign out_shl = a<<b;
    assign out_lshr = a>>b;
    assign out_not = ~a;
    assign out_assign = sm;
    assign out_mux = sm ? a : b;
    assign out_eq = a==b;
    assign out_ne = a!=b;
    assign out_caseEq = a===b;
    assign out_caseNe = a!==b;
    assign out_lt = a<b;
    assign out_le = a<=b;
    assign out_gt = a>b;
    assign out_ge = a>=b;
    assign out_logicAnd = a&&b;
    assign out_logicOr = a||b;
    assign out_sdiv = sa/sb;
    assign out_smod = sa%sb;
    assign out_sashr = sa>>>sb;
    assign out_slt = sa<sb;
    assign out_sle = sa<=sb;
    assign out_sgt = sa>sb;
    assign out_sge = sa>=sb;
    assign out_reduceAnd = &a;
    assign out_reduceOr = |a;
    assign out_reduceXor = ^a;
    assign out_reduceNand = ~&a;
    assign out_reduceNor = ~|a;
    assign out_reduceXnor = ~^a;
    assign out_logicNot = !a;
    assign out_concat = {sm,sm};
    assign out_replicate = {2{sm}};
    assign out_static = a[38:31];
    assign out_dynamic = b < 64 ? (a >> b) : 0;
    assign out_array = b < 8 ? (a >> (b*8)) : 0;
endmodule
