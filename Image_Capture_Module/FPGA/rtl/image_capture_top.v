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
    input  wire        key1,           // 板载 KEY1，低电平按下，用于本地调试触发
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
    output wire        ov5640_xclk,
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

//====================================================================//
// 业务状态机声明
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

clk_gen u_pll (
    .areset (~sys_rst_n),
    .inclk0 (sys_clk),
    .c0     (clk_100m),       
    .c1     (clk_100m_shift), 
    .c2     (clk_25m),        
    .locked (locked)
);

assign ov5640_xclk  = clk_25m;
assign ov5640_rst_n = rst_n;
assign ov5640_pwdn  = 1'b0;

//====================================================================//
// 2. 串口命令解析与反馈
//====================================================================//
wire        cmd_start, cmd_valid;
wire [255:0] device_id;
wire [255:0] token;
wire [31:0] timestamp;
wire        cmd_error;
wire        debug_cmd_valid;
wire        cmd_accept;
wire [255:0] cmd_device_id;
wire [255:0] cmd_token;
wire [31:0]  cmd_timestamp;
reg  [255:0] active_device_id;
reg  [255:0] active_token;
reg  [31:0]  active_timestamp;

localparam [255:0] DEBUG_DEVICE_ID = {
    8'h31, 8'h30, 8'h30, 8'h31, 8'h2D, 8'h30, 8'h31, 8'h2D,
    8'h30, 8'h31, {22{8'h20}}
};
localparam [255:0] DEBUG_TOKEN = {
    8'h64, 8'h65, 8'h76, 8'h69, 8'h63, 8'h65, 8'h2D, 8'h74,
    8'h6F, 8'h6B, 8'h65, 8'h6E, 8'h2D, 8'h30, 8'h30, 8'h31,
    {16{8'h20}}
};
localparam [31:0] DEBUG_TIMESTAMP = 32'd1712800000;

uart_cmd_parser u_parser (
    .sys_clk(sys_clk), .sys_rst_n(rst_n), .uart_rx(uart_rx),
    .cmd_start(cmd_start), .cmd_valid(cmd_valid),
    .device_id(device_id), .token(token), .timestamp(timestamp),
    .cmd_error(cmd_error)
);

// KEY1 调试触发：板载按键外部上拉，按下为低电平。
reg        key1_meta;
reg        key1_sync;
reg        key1_pressed_stable;
reg        key1_pressed_d;
reg [19:0] key1_debounce_cnt;
wire       key1_pressed_sample = ~key1_sync;

always @(posedge sys_clk or negedge rst_n) begin
    if (!rst_n) begin
        key1_meta           <= 1'b1;
        key1_sync           <= 1'b1;
        key1_pressed_stable <= 1'b0;
        key1_pressed_d      <= 1'b0;
        key1_debounce_cnt   <= 20'd0;
    end else begin
        key1_meta      <= key1;
        key1_sync      <= key1_meta;
        key1_pressed_d <= key1_pressed_stable;

        if (key1_pressed_sample == key1_pressed_stable) begin
            key1_debounce_cnt <= 20'd0;
        end else if (key1_debounce_cnt == 20'd999_999) begin
            key1_pressed_stable <= key1_pressed_sample;
            key1_debounce_cnt   <= 20'd0;
        end else begin
            key1_debounce_cnt <= key1_debounce_cnt + 20'd1;
        end
    end
end

assign debug_cmd_valid = key1_pressed_stable & ~key1_pressed_d;
assign cmd_accept = (sys_state == SYS_IDLE) && (cmd_valid || debug_cmd_valid);
assign cmd_device_id = debug_cmd_valid ? DEBUG_DEVICE_ID : device_id;
assign cmd_token = debug_cmd_valid ? DEBUG_TOKEN : token;
assign cmd_timestamp = debug_cmd_valid ? DEBUG_TIMESTAMP : timestamp;

buzzer_pwm_ctrl u_buzzer (
    .sys_clk(sys_clk), .sys_rst_n(rst_n), .cmd_valid(cmd_start | (debug_cmd_valid && (sys_state == SYS_IDLE))), .beep(beep)
);

rtc_display_74hc595 u_display (
    .sys_clk(sys_clk), .sys_rst_n(rst_n), .cmd_valid(cmd_accept), .timestamp(cmd_timestamp),
    .ds(ds), .shcp(shcp), .stcp(stcp), .oe(oe)
);

//====================================================================//
// 3. 全局业务状态机与核心逻辑
//====================================================================//
always @(posedge sys_clk or negedge rst_n) begin
    if (!rst_n) begin
        sys_state <= SYS_INIT;
        capture_req <= 0; wr_rst_req <= 0; rd_rst_req <= 0;
        active_device_id <= 256'd0;
        active_token <= 256'd0;
        active_timestamp <= 32'd0;
    end else begin
        wr_rst_req <= 0; rd_rst_req <= 0;
        case (sys_state)
            SYS_INIT: if (sdram_init_end && cfg_done) sys_state <= SYS_IDLE;
            SYS_IDLE: begin
                if (cmd_accept) begin
                    wr_rst_req <= 1;
                    capture_req <= 1; // 保持拉高，直到 capture_done
                    active_device_id <= cmd_device_id;
                    active_token <= cmd_token;
                    active_timestamp <= cmd_timestamp;
                    sys_state <= SYS_CAPTURE;
                end
            end
            SYS_CAPTURE: begin
                if (capture_done) begin 
                    capture_req <= 0; // 收到完成信号后再拉低
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
reg led4_r;
reg [25:0] led4_timer;

always @(posedge sys_clk or negedge rst_n) begin
    if (!rst_n) begin
        led4_r <= 1; led4_timer <= 0;
    end else begin
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
assign led_2 = ~(sys_state == SYS_CAPTURE);
assign led_3 = ~(sys_state == SYS_SPI_TX);
assign led_4 = led4_r;
//====================================================================//
// 5. OV5640 摄像头（已修复：加入初始化 + 正确数据流）
//====================================================================//

wire        cam_wr_en_unused;
wire [15:0] cam_data_unused;
wire        cfg_done;

// 摄像头初始化 + 数据输出
ov5640_top u_ov5640 (
    .sys_clk        (sys_clk),
    .sys_rst_n      (rst_n),
    .sys_init_done  (sdram_init_end),

    .ov5640_pclk    (ov5640_pclk),
    .ov5640_href    (ov5640_href),
    .ov5640_vsync   (ov5640_vsync),
    .ov5640_data    (ov5640_data),

    .cfg_done       (cfg_done),

    .sccb_scl       (sccb_scl),
    .sccb_sda       (sccb_sda),

    .ov5640_wr_en   (cam_wr_en_unused),
    .ov5640_data_out(cam_data_unused)
);

//======================================================//
// JPEG 捕获模块（新版本）
//======================================================//
wire [7:0]  jpeg_data;
wire        jpeg_data_en;

wire [31:0] jpeg_size;
wire        capture_done_raw;

ov5640_jpeg_capture u_jpeg (
    .sys_clk      (sys_clk),
    .sys_rst_n    (rst_n),
    .capture_req  (capture_req),
    .ov5640_pclk  (ov5640_pclk),
    .ov5640_href  (ov5640_href),
    .ov5640_vsync (ov5640_vsync),
    .ov5640_data  (ov5640_data),
    .sccb_scl     (),
    .sccb_sda     (),
    .jpeg_data    (jpeg_data),
    .jpeg_data_en (jpeg_data_en),
    .jpeg_size    (jpeg_size),
    .capture_done (capture_done_raw)
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
wire [15:0] sdram_rd_data;
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
    .wr_burst_len    (10'd8),
    .wr_rst          (wr_rst_req),     
    
    .rd_fifo_rd_clk  (sys_clk),        
    .rd_fifo_rd_req  (spi_rd_en),      
    .rd_fifo_rd_data (sdram_rd_data),
    .rd_fifo_num     (rd_fifo_num),    
    .sdram_rd_b_addr (24'd0),          
    .sdram_rd_e_addr (24'hFFFFFF),     
    .rd_burst_len    (10'd8),
    .rd_rst          (rd_rst_req),     
    
    .read_valid      (sys_state == SYS_SPI_TX),
    .pingpang_en     (1'b0),
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

always @(posedge sys_clk or negedge rst_n) begin
    if (!rst_n)
        saved_jpeg_size <= 32'd0;
    else if (capture_done)
        saved_jpeg_size <= jpeg_size;
end

spi_master_tx u_spi (
    .sys_clk           (sys_clk),
    .sys_rst_n         (rst_n),
    .send_enable       (sys_state == SYS_SPI_TX && rd_fifo_num > 10'd0),
    .device_id         (active_device_id),
    .token             (active_token),
    .timestamp         (active_timestamp),
    .jpeg_size         (saved_jpeg_size),
    .jpeg_fifo_rd_en   (spi_rd_en),
    .jpeg_fifo_rd_data (sdram_rd_data[7:0]),
    .spi_clk           (spi_sck),
    .spi_mosi          (spi_mosi),
    .spi_miso          (spi_miso),
    .spi_cs_n          (spi_cs_n),
    .send_done         (spi_done)
);
endmodule
