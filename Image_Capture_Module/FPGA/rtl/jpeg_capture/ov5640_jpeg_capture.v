`timescale  1ns/1ns
module ov5640_jpeg_capture (
    input  wire        sys_clk,
    input  wire        sys_rst_n,
    input  wire        capture_req,
    
    // 摄像头引脚
    input  wire        ov5640_pclk,
    input  wire        ov5640_href,
    input  wire        ov5640_vsync,
    input  wire [7:0]  ov5640_data,
    output wire        sccb_scl,
    inout  wire        sccb_sda,
    output wire        cfg_done,

    // SDRAM 引脚
    input  wire        clk_100m,
    input  wire        clk_100m_shift,
    output wire        sdram_clk, sdram_cke, sdram_cs_n, sdram_ras_n, sdram_cas_n, sdram_we_n,
    output wire [1:0]  sdram_ba, sdram_dqm,
    output wire [12:0] sdram_addr,
    inout  wire [15:0] sdram_dq,
    output wire        sdram_init_done,

    // 用户输出
    input  wire        rd_en,
    output wire [15:0] rd_data,
    output wire [9:0]  rd_fifo_num,
    output reg  [31:0] jpeg_size,
    output reg         capture_done
);

wire wr_en_raw;
wire [15:0] wr_data_raw;

// 1. 摄像头配置 (使用 25MHz 时钟保证 I2C 稳定)
ov5640_top u_ov5640_cfg (
    .sys_clk(sys_clk), .sys_rst_n(sys_rst_n), .sys_init_done(sdram_init_done),
    .ov5640_pclk(ov5640_pclk), .ov5640_href(ov5640_href), .ov5640_vsync(ov5640_vsync), .ov5640_data(ov5640_data),
    .cfg_done(cfg_done), .sccb_scl(sccb_scl), .sccb_sda(sccb_sda),
    .ov5640_wr_en(wr_en_raw), .ov5640_data_out(wr_data_raw)
);

// 2. JPEG 捕获控制器 (处理 capture_req，只抓取一帧)
reg capturing;
always @(posedge ov5640_pclk or negedge sys_rst_n) begin
    if(!sys_rst_n) begin 
        capturing <= 0; jpeg_size <= 0; capture_done <= 0;
    end else begin
        capture_done <= 0;
        if (capture_req && !capturing) begin
            capturing <= 1; jpeg_size <= 0;
        end else if (capturing && wr_en_raw) begin
            // 正常自增，记录真实图像大小
            jpeg_size <= jpeg_size + 2; 
        end
        // 遇到下一帧同步信号停止捕获
        if (capturing && ov5640_vsync) begin
            capturing <= 0; capture_done <= 1;
        end
    end
end

wire wr_en = capturing ? wr_en_raw : 1'b0;
wire [15:0] wr_data = wr_data_raw;

// 3. SDRAM 存储 (利用 capture_req 作为复位信号清空 FIFO 和地址)
sdram_top u_sdram (
    .sys_clk(clk_100m), .clk_out(clk_100m_shift), .sys_rst_n(sys_rst_n),
    .wr_fifo_wr_clk(ov5640_pclk), .wr_fifo_wr_req(wr_en), .wr_fifo_wr_data(wr_data),
    .sdram_wr_b_addr(24'd0), .sdram_wr_e_addr(24'd1200000), .wr_burst_len(10'd512),
    .wr_rst(capture_req), 
    
    .rd_fifo_rd_clk(sys_clk), .rd_fifo_rd_req(rd_en), .rd_fifo_rd_data(rd_data),
    .rd_fifo_num(rd_fifo_num), .sdram_rd_b_addr(24'd0), .sdram_rd_e_addr(24'd1200000), .rd_burst_len(10'd512),
    .rd_rst(capture_req),
    
    .read_valid(1'b1), .pingpang_en(1'b0), .init_end(sdram_init_done),
    .sdram_clk(sdram_clk), .sdram_cke(sdram_cke), .sdram_cs_n(sdram_cs_n),
    .sdram_ras_n(sdram_ras_n), .sdram_cas_n(sdram_cas_n), .sdram_we_n(sdram_we_n),
    .sdram_ba(sdram_ba), .sdram_addr(sdram_addr), .sdram_dq(sdram_dq), .sdram_dqm(sdram_dqm)
);

endmodule