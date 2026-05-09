/**
 * @file rtc_display_74hc595.v
 * @brief Time Display Module with 74HC595
 * 
 * Safely converts UNIX timestamp to Beijing time using Iterative Subtraction.
 * Displays HH.MM.SS (带有小数点分隔) on 6-digit 7-segment display.
 */

`timescale 1ns/1ns

module rtc_display_74hc595 (
    input  wire        sys_clk,        
    input  wire        sys_rst_n,      
    input  wire        cmd_valid,      
    input  wire [31:0] timestamp,      
    
    output reg         ds,             
    output reg         shcp,           
    output reg         stcp,           
    output wire        oe              
);

localparam BEIJING_OFFSET = 32'd28800; // UTC+8
localparam SCAN_PERIOD = 50_000;       // 1kHz refresh
localparam SHIFT_PERIOD = 16'd25;      // 2MHz shift clock

//====================================================================//
// Iterative Subtraction Hardware Divider (Replaces '/' and '%')
//====================================================================//
reg [4:0]  calc_state;
reg [31:0] temp_val;
reg [5:0]  hours, minutes, seconds;
reg [5:0]  d0, d1, d2, d3, d4, d5; // Temp digits
reg [5:0]  digit [0:5];            // Final display digits

always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        calc_state <= 0; temp_val <= 0;
        hours <= 0; minutes <= 0; seconds <= 0;
        d0<=0; d1<=0; d2<=0; d3<=0; d4<=0; d5<=0;
        digit[0]<=0; digit[1]<=0; digit[2]<=0; digit[3]<=0; digit[4]<=0; digit[5]<=0;
    end else begin
        case (calc_state)
            0: if (cmd_valid) begin temp_val <= timestamp + BEIJING_OFFSET; calc_state <= 1; end
            
            // 1. Subtract Days (86400)
            1: begin
                if (temp_val >= 32'd8640000) temp_val <= temp_val - 32'd8640000;
                else if (temp_val >= 32'd864000) temp_val <= temp_val - 32'd864000;
                else if (temp_val >= 32'd86400) temp_val <= temp_val - 32'd86400;
                else begin hours <= 0; calc_state <= 2; end
            end
            
            // 2. Subtract Hours (3600)
            2: begin
                if (temp_val >= 32'd3600) begin temp_val <= temp_val - 32'd3600; hours <= hours + 1; end
                else begin minutes <= 0; calc_state <= 3; end
            end
            
            // 3. Subtract Minutes (60)
            3: begin
                if (temp_val >= 32'd60) begin temp_val <= temp_val - 32'd60; minutes <= minutes + 1; end
                else begin seconds <= temp_val[5:0]; calc_state <= 4; d0<=0; d1<=0; end
            end
            
            // 4. Split Hours Tens/Units
            4: begin
                if (hours >= 6'd10) begin hours <= hours - 10; d0 <= d0 + 1; end
                else begin d1 <= hours; calc_state <= 5; d2<=0; d3<=0; end
            end
            
            // 5. Split Minutes Tens/Units
            5: begin
                if (minutes >= 6'd10) begin minutes <= minutes - 10; d2 <= d2 + 1; end
                else begin d3 <= minutes; calc_state <= 6; d4<=0; d5<=0; end
            end
            
            // 6. Split Seconds Tens/Units & Latch to Display
            6: begin
                if (seconds >= 6'd10) begin seconds <= seconds - 10; d4 <= d4 + 1; end
                else begin
                    d5 <= seconds;
                    digit[0] <= d0; digit[1] <= d1;
                    digit[2] <= d2; digit[3] <= d3;
                    digit[4] <= d4; digit[5] <= d5;
                    calc_state <= 0; // Done!
                end
            end
            default: calc_state <= 0;
        endcase
    end
end

//====================================================================//
// 74HC595 Driver Logic
//====================================================================//
reg [2:0]  state;
localparam IDLE=0, LOAD=1, SHIFT=2, LATCH=3, WAIT=4;

reg [3:0]  digit_idx;
reg [4:0]  shift_cnt;
reg [15:0] scan_cnt;
reg [15:0] shift_cnt_timing;

reg [7:0] seg_code, dig_sel;
reg [15:0] display_data;

// 获取基础数字段码 (共阳极: 0表示亮，最高位为 DP 小数点，默认 1 表示灭)
always @(*) begin
    case (digit[digit_idx])
        4'd0: seg_code = 8'b11000000; 
        4'd1: seg_code = 8'b11111001; 
        4'd2: seg_code = 8'b10100100; 
        4'd3: seg_code = 8'b10110000; 
        4'd4: seg_code = 8'b10011001; 
        4'd5: seg_code = 8'b10010010; 
        4'd6: seg_code = 8'b10000010; 
        4'd7: seg_code = 8'b11111000; 
        4'd8: seg_code = 8'b10000000; 
        4'd9: seg_code = 8'b10010000; 
        default: seg_code = 8'b11111111;
    endcase
    
    // 【点亮小数点的黑魔法】：
    // 在第 2 位 (小时个位) 和 第 4 位 (分钟个位) 上，强行把最高位拉低 (0亮)
    if (digit_idx == 4'd1 || digit_idx == 4'd3) begin
        seg_code[7] = 1'b0; 
    end
end

always @(*) begin
    // 为6位数码管动态扫描分配位选 
    case (digit_idx)
        3'd0: dig_sel = 8'b00000001; 
        3'd1: dig_sel = 8'b00000010; 
        3'd2: dig_sel = 8'b00000100; 
        3'd3: dig_sel = 8'b00001000; 
        3'd4: dig_sel = 8'b00010000; 
        3'd5: dig_sel = 8'b00100000; 
        default: dig_sel = 8'b00000000;
    endcase
end

always @(posedge sys_clk) display_data <= {dig_sel, seg_code};

always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        state <= IDLE;
        scan_cnt <= 0; shift_cnt <= 0; shift_cnt_timing <= 0; digit_idx <= 0;
        ds <= 0; shcp <= 0; stcp <= 0;
    end else begin
        case (state)
            IDLE: begin
                ds <= 0; shcp <= 0; stcp <= 0;
                if (scan_cnt >= SCAN_PERIOD - 1) begin
                    state <= LOAD; scan_cnt <= 0; shift_cnt <= 15;
                end else scan_cnt <= scan_cnt + 1;
            end
            LOAD: begin
                ds <= display_data[shift_cnt];
                state <= SHIFT; shift_cnt_timing <= 0;
            end
            SHIFT: begin
                if (shift_cnt_timing >= SHIFT_PERIOD - 1) begin
                    shcp <= ~shcp; shift_cnt_timing <= 0;
                    if (shcp == 1'b0) begin
                        if (shift_cnt == 0) state <= LATCH;
                        else begin shift_cnt <= shift_cnt - 1; state <= LOAD; end
                    end
                end else shift_cnt_timing <= shift_cnt_timing + 1;
            end
            LATCH: begin
                stcp <= 1'b1; state <= WAIT;
                if (digit_idx >= 5) digit_idx <= 0; // 6位数码管，0~5
                else digit_idx <= digit_idx + 1;
            end
            WAIT: begin stcp <= 1'b0; state <= IDLE; end
            default: state <= IDLE;
        endcase
    end
end

assign oe = 1'b0;

endmodule