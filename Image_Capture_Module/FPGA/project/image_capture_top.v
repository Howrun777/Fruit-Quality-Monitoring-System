/**
 * @file image_capture_top.v
 * @brief 修复后的整合顶层：串口触发 -> 捕获 -> SDRAM -> SPI发送
 */

`timescale 1ns/1ns

module image_capture_top (
    input  wire        sys_clk,        // 50MHz 晶振
    input  wire        sys_rst_n,      // 复位按键
    
    // 串口与外设
    input  wire        uart_rx,        
    output wire        beep,            
    output wire        ds, shcp, stcp, oe, 
    
    // Debug LED
    output wire        led_1,          // SDRAM 初始化完成
    output wire        led_2,          // 抓图状态
    output wire        led_3,          // SPI 发送状态
    output wire        led_4,          // 摄像头配置完成
    
    // 摄像头接口
    input  wire [7:0]  ov5640_data,
    input  wire        ov5640_vsync,
    input  wire        ov5640_href,
    input  wire        ov5640_pclk,
    output wire        ov5640_xclk,    
    output wire        ov5640_rst_n,
    output wire        ov5640_pwdn,
    output wire        sccb_scl,
    inout  wire        sccb_sda,
    
    // SDRAM 物理接口
    output wire        sdram_clk, sdram_cke, sdram_cs_n, sdram_ras_n, sdram_cas_n, sdram_we_n,
    output wire [1:0]  sdram_ba, sdram_dqm,
    output wire [12:0] sdram_addr,
    inout  wire [15:0] sdram_dq,
    
    // SPI 物理接口
    output wire        spi_sck, spi_mosi, spi_cs_n,
    input  wire        spi_miso
);

//-------------------------------------------------------
// 1. 时钟管理
//-------------------------------------------------------
wire clk_100m, clk_100m_shift, clk_25m, locked;
wire rst_n = sys_rst_n & locked;

clk_gen u_pll (
    .areset (~sys_rst_n),
    .inclk0 (sys_clk),
    .c0     (clk_100m),       
    .c1     (clk_100m_shift), 
    .c2     (clk_25m),        
    .locked (locked)
);
assign ov5640_xclk = clk_25m;

//-------------------------------------------------------
// 2. 串口控制与反馈
//-------------------------------------------------------
wire        cmd_start, cmd_valid;
wire [255:0] device_id;  // 【修改 1】：扩大为 256 bit
wire [255:0] token;      // 【修改 2】：扩大为 256 bit
wire [31:0] timestamp;

uart_cmd_parser u_parser (
    .sys_clk(sys_clk), .sys_rst_n(rst_n), .uart_rx(uart_rx),
    .cmd_start(cmd_start), .cmd_valid(cmd_valid),
    .device_id(device_id), .token(token), .timestamp(timestamp)
);

buzzer_pwm_ctrl u_buzzer (
    .sys_clk(sys_clk), .sys_rst_n(rst_n), .cmd_valid(cmd_start), .beep(beep)
);

rtc_display_74hc595 u_display (
    .sys_clk(sys_clk), .sys_rst_n(rst_n), .cmd_valid(cmd_valid), .timestamp(timestamp),
    .ds(ds), .shcp(shcp), .stcp(stcp), .oe(oe)
);

//-------------------------------------------------------
// 3. 全局业务逻辑状态机
//-------------------------------------------------------
localparam SYS_INIT    = 3'd0;
localparam SYS_IDLE    = 3'd1;
localparam SYS_CAPTURE = 3'd2;
localparam SYS_SPI_TX  = 3'd3;

reg [2:0]   sys_state;
reg         capture_req;
wire        capture_done, spi_done, sdram_init_end, cfg_done;
wire [31:0] current_jpeg_size;
wire [15:0] sdram_data_out;
wire [9:0]  rd_fifo_num;

always @(posedge sys_clk or negedge rst_n) begin
    if (!rst_n) begin
        sys_state   <= SYS_INIT;
        capture_req <= 0;
    end else begin
        capture_req <= 0;
        case (sys_state)
            SYS_INIT:    if (sdram_init_end && cfg_done) sys_state <= SYS_IDLE;
            SYS_IDLE:    if (cmd_valid) begin
                            capture_req <= 1; // 触发捕获
                            sys_state   <= SYS_CAPTURE;
                         end
            SYS_CAPTURE: if (capture_done) sys_state <= SYS_SPI_TX;
            SYS_SPI_TX:  if (spi_done) sys_state <= SYS_IDLE;
            default:     sys_state <= SYS_IDLE;
        endcase
    end
end

//-------------------------------------------------------
// 4. 摄像头与存储子系统
//-------------------------------------------------------
ov5640_jpeg_capture u_capture (
    .sys_clk           (sys_clk),
    .sys_rst_n         (rst_n),
    .capture_req       (capture_req),
    
    // 摄像头
    .ov5640_pclk       (ov5640_pclk),
    .ov5640_href       (ov5640_href),
    .ov5640_vsync      (ov5640_vsync),
    .ov5640_data       (ov5640_data),
    .sccb_scl          (sccb_scl),
    .sccb_sda          (sccb_sda),
    .cfg_done          (cfg_done),

    // SDRAM信号桥接
    .clk_100m          (clk_100m),
    .clk_100m_shift    (clk_100m_shift),
    .sdram_clk         (sdram_clk),
    .sdram_cke         (sdram_cke),
    .sdram_cs_n        (sdram_cs_n),
    .sdram_ras_n       (sdram_ras_n),
    .sdram_cas_n       (sdram_cas_n),
    .sdram_we_n        (sdram_we_n),
    .sdram_ba          (sdram_ba),
    .sdram_dqm         (sdram_dqm),
    .sdram_addr        (sdram_addr),
    .sdram_dq          (sdram_dq),
    .sdram_init_done   (sdram_init_end),

    // 用户数据输出
    .rd_en             (spi_rd_en),
    .rd_data           (sdram_data_out),
    .rd_fifo_num       (rd_fifo_num),
    .jpeg_size         (current_jpeg_size),
    .capture_done      (capture_done)
);

//-------------------------------------------------------
// 5. SPI 发射模块
//-------------------------------------------------------
wire spi_rd_en;
spi_master_tx u_spi (
    .sys_clk           (sys_clk),         
    .sys_rst_n         (rst_n),           
    .send_enable       (sys_state == SYS_SPI_TX), 
    .device_id         (device_id),       
    .token             (token),           
    .timestamp         (timestamp),       
    .jpeg_size         (current_jpeg_size),      
    .jpeg_fifo_rd_en   (spi_rd_en),       
    .jpeg_fifo_rd_data (sdram_data_out),  // 【重要修复】：传入完整的 16位 信号
    .spi_clk           (spi_sck),         
    .spi_mosi          (spi_mosi),        
    .spi_cs_n          (spi_cs_n),        
    .send_done         (spi_done)         
);

// 静态连接
assign ov5640_rst_n = 1'b1;
assign ov5640_pwdn  = 1'b0;
assign led_1 = ~sdram_init_end;
assign led_2 = ~(sys_state == SYS_CAPTURE);       
assign led_3 = ~(sys_state == SYS_SPI_TX);        
assign led_4 = ~cfg_done;

endmodule