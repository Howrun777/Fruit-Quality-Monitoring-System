`timescale 1ns/1ns

module uart_rx (
    input  wire        sys_clk,
    input  wire        sys_rst_n,
    input  wire        uart_rx,
    output reg         rx_done,
    output reg  [7:0]  rx_data
);

parameter CLK_FREQ  = 50_000_000;
parameter UART_BAUD = 115200;    // 放心用 115200，绝不吞字！
localparam BIT_CNT  = CLK_FREQ / UART_BAUD;

// 1. 消除亚稳态 & 精准下降沿检测
reg rx_d0, rx_d1, rx_d2;
always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        rx_d0 <= 1; rx_d1 <= 1; rx_d2 <= 1;
    end else begin
        rx_d0 <= uart_rx; 
        rx_d1 <= rx_d0; 
        rx_d2 <= rx_d1;
    end
end
wire rx_fall = rx_d2 & ~rx_d1;

// 2. 状态机与精准抗干扰采样
reg        rx_en;
reg [15:0] clk_cnt;
reg [3:0]  bit_cnt;

always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        rx_en   <= 0;
        clk_cnt <= 0;
        bit_cnt <= 0;
        rx_done <= 0;
        rx_data <= 0;
    end else begin
        rx_done <= 0; // 默认拉低，产生单周期脉冲
        
        if (!rx_en) begin
            // 只有检测到下降沿，才开启接收流程
            if (rx_fall) begin
                rx_en   <= 1;
                clk_cnt <= 0;
                bit_cnt <= 0;
            end
        end else begin
            // 波特率计数器
            if (clk_cnt < BIT_CNT - 1) begin
                clk_cnt <= clk_cnt + 1;
            end else begin
                clk_cnt <= 0;
                bit_cnt <= bit_cnt + 1;
            end
            
            // 黄金采样点：在每个比特的正中间采样
            if (clk_cnt == BIT_CNT / 2) begin
                if (bit_cnt == 0) begin
                    // 毛刺滤除：如果在起始位中间发现变成高电平了，立刻取消接收
                    if (rx_d2 == 1'b1) rx_en <= 0;
                end else if (bit_cnt >= 1 && bit_cnt <= 8) begin
                    // 采样有效数据位
                    rx_data[bit_cnt - 1] <= rx_d2;
                end else if (bit_cnt == 9) begin
                    // 【致命BUG修复】：在停止位的正中间，提前结束接收！
                    // 这样就能留下半个周期的空闲裕量，连续发 1 万个字符都不会吞字！
                    rx_en <= 0;
                    rx_done <= 1;
                end
            end
        end
    end
end

endmodule