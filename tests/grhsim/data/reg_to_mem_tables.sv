module reg_to_mem_tables (
    input logic clock, reset, clear_useful, enable0, enable1,
    input logic [3:0] address0, address1, read_address,
    input logic [4:0] window_start,
    input logic [64:0] data0, data1,
    output wire [4:0] phr_window,
    output wire [1:0] useful_read0, useful_read1,
    output wire [64:0] ftq_read,
    output wire [7:0] rename_rows
);
    // Explicit scalar rows model the lowered Xiangshan table representation.
    logic phr_0 = 1'd0;
    logic [1:0] useful_0 = 2'd0;
    logic [64:0] ftq_0 = 65'd0;
    logic rename_entry_0 = 0;
    always @(posedge clock or posedge reset) begin
        if (reset) phr_0 <= 0;
        else begin
            if (enable0 && address0 == 4'd0) phr_0 <= data0[0];
            if (enable1 && address1 == 4'd0) phr_0 <= data1[0];
        end
    end
    always @(posedge clock) begin
        if (reset || clear_useful) useful_0 <= 0;
        else if (enable0 && address0 == 4'd0) useful_0 <= data0[1:0];
        if (reset) ftq_0 <= 0;
        else if (enable0 && address0 == 4'd0) ftq_0 <= data0;
        if (reset) rename_entry_0 <= 0;
        else begin
            if (enable0 && address0 == 4'd0) rename_entry_0 <= data0[0];
            if (enable1 && address1 == 4'd0) rename_entry_0 <= data1[0];
        end
    end
    logic phr_1 = 1'd1;
    logic [1:0] useful_1 = 2'd1;
    logic [64:0] ftq_1 = 65'd1;
    logic rename_entry_1 = 0;
    always @(posedge clock or posedge reset) begin
        if (reset) phr_1 <= 0;
        else begin
            if (enable0 && address0 == 4'd1) phr_1 <= data0[0];
            if (enable1 && address1 == 4'd1) phr_1 <= data1[0];
        end
    end
    always @(posedge clock) begin
        if (reset || clear_useful) useful_1 <= 0;
        else if (enable0 && address0 == 4'd1) useful_1 <= data0[1:0];
        if (reset) ftq_1 <= 0;
        else if (enable0 && address0 == 4'd1) ftq_1 <= data0;
        if (reset) rename_entry_1 <= 0;
        else begin
            if (enable0 && address0 == 4'd1) rename_entry_1 <= data0[0];
            if (enable1 && address1 == 4'd1) rename_entry_1 <= data1[0];
        end
    end
    logic phr_2 = 1'd0;
    logic [1:0] useful_2 = 2'd2;
    logic [64:0] ftq_2 = 65'd2;
    logic rename_entry_2 = 0;
    always @(posedge clock or posedge reset) begin
        if (reset) phr_2 <= 0;
        else begin
            if (enable0 && address0 == 4'd2) phr_2 <= data0[0];
            if (enable1 && address1 == 4'd2) phr_2 <= data1[0];
        end
    end
    always @(posedge clock) begin
        if (reset || clear_useful) useful_2 <= 0;
        else if (enable0 && address0 == 4'd2) useful_2 <= data0[1:0];
        if (reset) ftq_2 <= 0;
        else if (enable0 && address0 == 4'd2) ftq_2 <= data0;
        if (reset) rename_entry_2 <= 0;
        else begin
            if (enable0 && address0 == 4'd2) rename_entry_2 <= data0[0];
            if (enable1 && address1 == 4'd2) rename_entry_2 <= data1[0];
        end
    end
    logic phr_3 = 1'd1;
    logic [1:0] useful_3 = 2'd3;
    logic [64:0] ftq_3 = 65'd3;
    logic rename_entry_3 = 0;
    always @(posedge clock or posedge reset) begin
        if (reset) phr_3 <= 0;
        else begin
            if (enable0 && address0 == 4'd3) phr_3 <= data0[0];
            if (enable1 && address1 == 4'd3) phr_3 <= data1[0];
        end
    end
    always @(posedge clock) begin
        if (reset || clear_useful) useful_3 <= 0;
        else if (enable0 && address0 == 4'd3) useful_3 <= data0[1:0];
        if (reset) ftq_3 <= 0;
        else if (enable0 && address0 == 4'd3) ftq_3 <= data0;
        if (reset) rename_entry_3 <= 0;
        else begin
            if (enable0 && address0 == 4'd3) rename_entry_3 <= data0[0];
            if (enable1 && address1 == 4'd3) rename_entry_3 <= data1[0];
        end
    end
    logic phr_4 = 1'd0;
    logic [1:0] useful_4 = 2'd0;
    logic [64:0] ftq_4 = 65'd4;
    logic rename_entry_4 = 0;
    always @(posedge clock or posedge reset) begin
        if (reset) phr_4 <= 0;
        else begin
            if (enable0 && address0 == 4'd4) phr_4 <= data0[0];
            if (enable1 && address1 == 4'd4) phr_4 <= data1[0];
        end
    end
    always @(posedge clock) begin
        if (reset || clear_useful) useful_4 <= 0;
        else if (enable0 && address0 == 4'd4) useful_4 <= data0[1:0];
        if (reset) ftq_4 <= 0;
        else if (enable0 && address0 == 4'd4) ftq_4 <= data0;
        if (reset) rename_entry_4 <= 0;
        else begin
            if (enable0 && address0 == 4'd4) rename_entry_4 <= data0[0];
            if (enable1 && address1 == 4'd4) rename_entry_4 <= data1[0];
        end
    end
    logic phr_5 = 1'd1;
    logic [1:0] useful_5 = 2'd1;
    logic [64:0] ftq_5 = 65'd5;
    logic rename_entry_5 = 0;
    always @(posedge clock or posedge reset) begin
        if (reset) phr_5 <= 0;
        else begin
            if (enable0 && address0 == 4'd5) phr_5 <= data0[0];
            if (enable1 && address1 == 4'd5) phr_5 <= data1[0];
        end
    end
    always @(posedge clock) begin
        if (reset || clear_useful) useful_5 <= 0;
        else if (enable0 && address0 == 4'd5) useful_5 <= data0[1:0];
        if (reset) ftq_5 <= 0;
        else if (enable0 && address0 == 4'd5) ftq_5 <= data0;
        if (reset) rename_entry_5 <= 0;
        else begin
            if (enable0 && address0 == 4'd5) rename_entry_5 <= data0[0];
            if (enable1 && address1 == 4'd5) rename_entry_5 <= data1[0];
        end
    end
    logic phr_6 = 1'd0;
    logic [1:0] useful_6 = 2'd2;
    logic [64:0] ftq_6 = 65'd6;
    logic rename_entry_6 = 0;
    always @(posedge clock or posedge reset) begin
        if (reset) phr_6 <= 0;
        else begin
            if (enable0 && address0 == 4'd6) phr_6 <= data0[0];
            if (enable1 && address1 == 4'd6) phr_6 <= data1[0];
        end
    end
    always @(posedge clock) begin
        if (reset || clear_useful) useful_6 <= 0;
        else if (enable0 && address0 == 4'd6) useful_6 <= data0[1:0];
        if (reset) ftq_6 <= 0;
        else if (enable0 && address0 == 4'd6) ftq_6 <= data0;
        if (reset) rename_entry_6 <= 0;
        else begin
            if (enable0 && address0 == 4'd6) rename_entry_6 <= data0[0];
            if (enable1 && address1 == 4'd6) rename_entry_6 <= data1[0];
        end
    end
    logic phr_7 = 1'd1;
    logic [1:0] useful_7 = 2'd3;
    logic [64:0] ftq_7 = 65'd7;
    logic rename_entry_7 = 0;
    always @(posedge clock or posedge reset) begin
        if (reset) phr_7 <= 0;
        else begin
            if (enable0 && address0 == 4'd7) phr_7 <= data0[0];
            if (enable1 && address1 == 4'd7) phr_7 <= data1[0];
        end
    end
    always @(posedge clock) begin
        if (reset || clear_useful) useful_7 <= 0;
        else if (enable0 && address0 == 4'd7) useful_7 <= data0[1:0];
        if (reset) ftq_7 <= 0;
        else if (enable0 && address0 == 4'd7) ftq_7 <= data0;
        if (reset) rename_entry_7 <= 0;
        else begin
            if (enable0 && address0 == 4'd7) rename_entry_7 <= data0[0];
            if (enable1 && address1 == 4'd7) rename_entry_7 <= data1[0];
        end
    end
    wire [7:0] phr_bits = {phr_7, phr_6, phr_5, phr_4, phr_3, phr_2, phr_1, phr_0};
    wire [15:0] useful_bits = {useful_7, useful_6, useful_5, useful_4, useful_3, useful_2, useful_1, useful_0};
    wire [519:0] ftq_bits = {ftq_7, ftq_6, ftq_5, ftq_4, ftq_3, ftq_2, ftq_1, ftq_0};
    assign rename_rows = {rename_entry_7, rename_entry_6, rename_entry_5, rename_entry_4, rename_entry_3, rename_entry_2, rename_entry_1, rename_entry_0};
    wire [15:0] repeated_phr = {phr_bits, phr_bits};
    wire [15:0] shifted_phr = repeated_phr >> window_start;
    assign phr_window = shifted_phr[4:0];
    assign useful_read0 = read_address < 8 ? useful_bits[read_address*2+:2] : 0;
    assign useful_read1 = address1 < 8 ? useful_bits[address1*2+:2] : 0;
    assign ftq_read = read_address < 8 ? ftq_bits[read_address*65+:65] : 0;
endmodule
