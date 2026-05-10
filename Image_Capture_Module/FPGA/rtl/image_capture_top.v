/**
 * @file image_capture_top.v
 * @brief 最终合体顶层：带 4 级 LED 硬件 Debug 指示
 */

`timescale 1ns/1ns

module image_capture_top (
    input  wire        sys_clk,        // 50MHz 晶振
    input  wire        sys_rst_n,      // 复位按键
    
    // 1. 串口与声光反馈
    input  wire        uart_rx,        // 接收指令 (9600 baud)
    output wire        beep,           // 蜂鸣器
    output wire        ds, shcp, stcp, oe, // 数码管
    
    // 【新增】4个 Debug LED (低电平亮)
    output wire        led_1,          // SDRAM 初始化完成 (常亮)
    output wire        led_2,          // 指令解析成功，正在抓图
    output wire        led_3,          // 抓图完成，正在 SPI 发送
    output wire        led_4,          // 发送完毕 (大满贯，亮1秒)
    
    // 2. 摄像头接口
    input  wire [7:0]  ov5640_data,
    input  wire        ov5640_vsync,
    input  wire        ov5640_href,
    input  wire        ov5640_pclk,
    output wire        ov5640_rst_n,
    output wire        ov5640_pwdn,
    output wire        sccb_scl,
    inout  wire        sccb_sda,
    
    // 3. SDRAM 接口
    output wire        sdram_clk, sdram_cke, sdram_cs_n, sdram_ras_n, sdram_cas_n, sdram_we_n,
    output wire [1:0]  sdram_ba, sdram_dqm,
    output wire [12:0] sdram_addr,
    inout  wire [15:0] sdram_dq,
    
    // 4. SPI 接口 (FPGA -> ESP32)
    output wire        spi_sck, spi_mosi, spi_cs_n,
    input  wire        spi_miso
);

//====================================================================//
// 1. 全局时钟生成
//====================================================================//
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

//====================================================================//
// 2. 串口命令解析与反馈
//====================================================================//
wire        cmd_start, cmd_valid;
wire [255:0] device_id;
wire [255:0] token;
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

//====================================================================//
// 3. 全局业务状态机与核心逻辑
//====================================================================//
localparam SYS_INIT      = 3'd0;
localparam SYS_IDLE      = 3'd1;
localparam SYS_CAPTURE   = 3'd2;
localparam SYS_SPI_TX    = 3'd3;

reg [2:0] sys_state;
reg       capture_req;       
wire      spi_done;       
wire      sdram_init_end;

reg       wr_rst_req;     
reg       rd_rst_req;     

always @(posedge sys_clk or negedge rst_n) begin
    if (!rst_n) begin
        sys_state <= SYS_INIT;
        capture_req <= 0; wr_rst_req <= 0; rd_rst_req <= 0;
    end else begin
        capture_req <= 0; wr_rst_req <= 0; rd_rst_req <= 0;
        case (sys_state)
            SYS_INIT: if (sdram_init_end) sys_state <= SYS_IDLE;
            SYS_IDLE: begin
                if (cmd_valid) begin 
                    wr_rst_req <= 1; 
                    capture_req <= 1; 
                    sys_state <= SYS_CAPTURE;
                end
            end
            SYS_CAPTURE: begin
                if (capture_done) begin 
                    rd_rst_req <= 1;  
                    sys_state <= SYS_SPI_TX;
                end
            end
            SYS_SPI_TX: begin
                if (spi_done) sys_state <= SYS_IDLE; 
            end
            default: sys_state <= SYS_IDLE;
        endcase
    end
end

//====================================================================//
// 4. Debug LED 状态机 (0亮 1灭)
//====================================================================//
reg led2_r, led3_r, led4_r;
reg [25:0] led4_timer;

always @(posedge sys_clk or negedge rst_n) begin
    if (!rst_n) begin
        led2_r <= 1; led3_r <= 1; led4_r <= 1; led4_timer <= 0;
    end else begin
        // LED 2: 收到指令开始亮起
        if (cmd_valid) led2_r <= 0;
        else if (sys_state == SYS_IDLE && led4_timer == 0) led2_r <= 1; // 闲置时熄灭
        
        // LED 3: 捕获完成开始亮起
        if (capture_done) led3_r <= 0;
        else if (sys_state == SYS_IDLE && led4_timer == 0) led3_r <= 1;

        // LED 4: SPI 发送完成 (亮 1 秒钟后，所有灯复位)
        if (spi_done) begin
            led4_r <= 0;
            led4_timer <= 26'd50_000_000; // 50M = 1秒
        end else if (led4_timer > 0) begin
            led4_timer <= led4_timer - 1;
            led4_r <= 0;
        end else begin
            led4_r <= 1;
        end
    end
end

assign led_1 = ~sdram_init_end; // 初始化完成后拉低点亮 (常亮)
assign led_2 = led2_r;
assign led_3 = led3_r;
//assign led_4 = led4_r;
assign led_4 = cfg_done;
//====================================================================//
// 5. OV5640 摄像头（已修复：加入初始化 + 正确数据流）
//====================================================================//

wire        cam_wr_en;
wire [15:0] cam_data;
wire        cfg_done;

// 摄像头初始化 + 数据输出
ov5640_top u_ov5640 (
    .sys_clk        (clk_25m),
    .sys_rst_n      (rst_n),
    .sys_init_done  (1'b1),

    .ov5640_pclk    (ov5640_pclk),
    .ov5640_href    (ov5640_href),
    .ov5640_vsync   (ov5640_vsync),
    .ov5640_data    (ov5640_data),

    .cfg_done       (cfg_done),

    .sccb_scl       (sccb_scl),
    .sccb_sda       (sccb_sda),

    .ov5640_wr_en   (cam_wr_en),
    .ov5640_data_out(cam_data)
);

//======================================================//
// JPEG 捕获模块（新版本）
//======================================================//
wire [7:0]  jpeg_data;
wire        jpeg_data_en;
wire [31:0] jpeg_size;
wire        capture_done_raw;

jpeg_capture u_jpeg (
    .sys_clk        (clk_25m),
    .sys_rst_n      (rst_n),
    .capture_req    (capture_req),

    .cam_wr_en      (cam_wr_en),
    .cam_data       (cam_data),
	 .ov5640_vsync   (ov5640_vsync),
    .jpeg_data      (jpeg_data),
    .jpeg_data_en   (jpeg_data_en),
    .jpeg_size      (jpeg_size),
    .capture_done   (capture_done_raw)
);

//======================================================//
// 跨时钟域同步（必须）
//======================================================//
reg cap_done_d1, cap_done_d2;

always @(posedge sys_clk or negedge rst_n) begin
    if (!rst_n) begin
        cap_done_d1 <= 0;
        cap_done_d2 <= 0;
    end else begin
        cap_done_d1 <= capture_done_raw;
        cap_done_d2 <= cap_done_d1;
    end
end

wire capture_done = cap_done_d2;
//====================================================================//
// 6. SDRAM 控制器 (存图)
//====================================================================//
wire       spi_rd_en;
wire [7:0] spi_rd_data;
wire [9:0] rd_fifo_num;

sdram_top u_sdram (
    .sys_clk         (clk_100m),       
    .clk_out         (clk_100m_shift), 
    .sys_rst_n       (rst_n),          
    
    .wr_fifo_wr_clk  (ov5640_pclk),    
    .wr_fifo_wr_req  (jpeg_data_en),   
    .wr_fifo_wr_data ({8'd0, jpeg_data}), 
    .sdram_wr_b_addr (24'd0),          
    .sdram_wr_e_addr (24'hFFFFFF),     
    .wr_burst_len    (10'd512),        
    .wr_rst          (wr_rst_req),     
    
    .rd_fifo_rd_clk  (sys_clk),        
    .rd_fifo_rd_req  (spi_rd_en),      
    .rd_fifo_rd_data (spi_rd_data),    
    .rd_fifo_num     (rd_fifo_num),    
    .sdram_rd_b_addr (24'd0),          
    .sdram_rd_e_addr (24'hFFFFFF),     
    .rd_burst_len    (10'd512),        
    .rd_rst          (rd_rst_req),     
    
    .read_valid      (sys_state == SYS_SPI_TX), 
    .init_end        (sdram_init_end),
    
    .sdram_clk(sdram_clk), .sdram_cke(sdram_cke), .sdram_cs_n(sdram_cs_n),
    .sdram_ras_n(sdram_ras_n), .sdram_cas_n(sdram_cas_n), .sdram_we_n(sdram_we_n),
    .sdram_ba(sdram_ba), .sdram_addr(sdram_addr), .sdram_dqm(sdram_dqm),
    .sdram_dq(sdram_dq)
);

//====================================================================//
// 7. SPI 高速发射引擎
//====================================================================//
reg [31:0] saved_jpeg_size;

always @(posedge sys_clk) if (capture_done) saved_jpeg_size <= jpeg_size;

spi_master_tx u_spi (
    .sys_clk           (sys_clk),
    .sys_rst_n         (rst_n),
    .send_enable       (sys_state == SYS_SPI_TX && rd_fifo_num > 10), 
    .device_id         (device_id),
    .token             (token),
    .timestamp         (timestamp),
    .jpeg_size         (saved_jpeg_size),
    .jpeg_fifo_rd_en   (spi_rd_en),
    .jpeg_fifo_rd_data (spi_rd_data), 
    .spi_clk           (spi_sck),
    .spi_mosi          (spi_mosi),
    .spi_miso          (spi_miso),
    .spi_cs_n          (spi_cs_n),
    .send_done         (spi_done)
);
endmodule
