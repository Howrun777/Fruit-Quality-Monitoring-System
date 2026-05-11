`timescale 1ns/1ns

module image_capture_top (
    input  wire        sys_clk,
    input  wire        sys_rst_n,

    input  wire        uart_rx,
    output wire        beep,
    output wire        ds,
    output wire        shcp,
    output wire        stcp,
    output wire        oe,

    output wire        led_1,
    output wire        led_2,
    output wire        led_3,
    output wire        led_4,

    input  wire [7:0]  ov5640_data,
    input  wire        ov5640_vsync,
    input  wire        ov5640_href,
    input  wire        ov5640_pclk,
    output wire        ov5640_xclk,
    output wire        ov5640_rst_n,
    output wire        ov5640_pwdn,
    output wire        sccb_scl,
    inout  wire        sccb_sda,

    output wire        sdram_clk,
    output wire        sdram_cke,
    output wire        sdram_cs_n,
    output wire        sdram_ras_n,
    output wire        sdram_cas_n,
    output wire        sdram_we_n,
    output wire [1:0]  sdram_ba,
    output wire [1:0]  sdram_dqm,
    output wire [12:0] sdram_addr,
    inout  wire [15:0] sdram_dq,

    output wire        spi_sck,
    output wire        spi_mosi,
    output wire        spi_cs_n,
    input  wire        spi_miso
);

wire clk_100m;
wire clk_100m_shift;
wire clk_25m;
wire locked;
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
assign ov5640_rst_n = 1'b1;
assign ov5640_pwdn = 1'b0;

wire         cmd_start;
wire         cmd_valid;
wire [255:0] device_id;
wire [255:0] token;
wire [31:0]  timestamp;
wire         cmd_error;

uart_cmd_parser u_parser (
    .sys_clk   (sys_clk),
    .sys_rst_n (rst_n),
    .uart_rx   (uart_rx),
    .cmd_start (cmd_start),
    .cmd_valid (cmd_valid),
    .device_id (device_id),
    .token     (token),
    .timestamp (timestamp),
    .cmd_error (cmd_error)
);

buzzer_pwm_ctrl u_buzzer (
    .sys_clk   (sys_clk),
    .sys_rst_n (rst_n),
    .cmd_valid (cmd_start),
    .beep      (beep)
);

rtc_display_74hc595 u_display (
    .sys_clk   (sys_clk),
    .sys_rst_n (rst_n),
    .cmd_valid (cmd_valid),
    .timestamp (timestamp),
    .ds        (ds),
    .shcp      (shcp),
    .stcp      (stcp),
    .oe        (oe)
);

localparam SYS_INIT    = 3'd0;
localparam SYS_IDLE    = 3'd1;
localparam SYS_CAPTURE = 3'd2;
localparam SYS_SPI_TX  = 3'd3;

reg [2:0] sys_state;
reg       capture_req;
reg       wr_rst_req;
reg       rd_rst_req;

wire        capture_done_pclk;
wire        spi_done;
wire        sdram_init_end;
wire        cfg_done;
wire        cam_wr_en_unused;
wire [15:0] cam_data_unused;
wire [7:0]  jpeg_data;
wire        jpeg_data_en;
wire [31:0] jpeg_size_pclk;
reg  [31:0] saved_jpeg_size;
reg         capture_done_meta;
reg         capture_done_sync;
reg         capture_done_sync_d;
reg  [31:0] jpeg_size_meta;
reg  [31:0] jpeg_size_sync;
wire        capture_done = capture_done_sync & ~capture_done_sync_d;

wire        spi_rd_en;
wire [15:0] sdram_data_out;
wire [9:0]  rd_fifo_num;

always @(posedge sys_clk or negedge rst_n) begin
    if (!rst_n) begin
        sys_state   <= SYS_INIT;
        capture_req <= 1'b0;
        wr_rst_req  <= 1'b0;
        rd_rst_req  <= 1'b0;
    end else begin
        capture_req <= 1'b0;
        wr_rst_req  <= 1'b0;
        rd_rst_req  <= 1'b0;

        case (sys_state)
            SYS_INIT: begin
                if (sdram_init_end && cfg_done)
                    sys_state <= SYS_IDLE;
            end

            SYS_IDLE: begin
                if (cmd_valid) begin
                    wr_rst_req  <= 1'b1;
                    capture_req <= 1'b1;
                    sys_state   <= SYS_CAPTURE;
                end
            end

            SYS_CAPTURE: begin
                if (capture_done) begin
                    rd_rst_req <= 1'b1;
                    sys_state  <= SYS_SPI_TX;
                end
            end

            SYS_SPI_TX: begin
                if (spi_done)
                    sys_state <= SYS_IDLE;
            end

            default: sys_state <= SYS_IDLE;
        endcase
    end
end

always @(posedge sys_clk or negedge rst_n) begin
    if (!rst_n) begin
        capture_done_meta   <= 1'b0;
        capture_done_sync   <= 1'b0;
        capture_done_sync_d <= 1'b0;
        jpeg_size_meta      <= 32'd0;
        jpeg_size_sync      <= 32'd0;
        saved_jpeg_size     <= 32'd0;
    end else begin
        capture_done_meta   <= capture_done_pclk;
        capture_done_sync   <= capture_done_meta;
        capture_done_sync_d <= capture_done_sync;
        jpeg_size_meta      <= jpeg_size_pclk;
        jpeg_size_sync      <= jpeg_size_meta;

        if (capture_done)
            saved_jpeg_size <= jpeg_size_sync;
    end
end

ov5640_top u_ov5640 (
    .sys_clk         (sys_clk),
    .sys_rst_n       (rst_n),
    .sys_init_done   (sdram_init_end),
    .ov5640_pclk     (ov5640_pclk),
    .ov5640_href     (ov5640_href),
    .ov5640_vsync    (ov5640_vsync),
    .ov5640_data     (ov5640_data),
    .cfg_done        (cfg_done),
    .sccb_scl        (sccb_scl),
    .sccb_sda        (sccb_sda),
    .ov5640_wr_en    (cam_wr_en_unused),
    .ov5640_data_out (cam_data_unused)
);

ov5640_jpeg_capture u_capture (
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
    .jpeg_size    (jpeg_size_pclk),
    .capture_done (capture_done_pclk)
);

sdram_top u_sdram (
    .sys_clk         (clk_100m),
    .clk_out         (clk_100m_shift),
    .sys_rst_n       (rst_n),

    .wr_fifo_wr_clk  (ov5640_pclk),
    .wr_fifo_wr_req  (jpeg_data_en),
    .wr_fifo_wr_data ({8'd0, jpeg_data}),
    .sdram_wr_b_addr (24'd0),
    .sdram_wr_e_addr (24'hFFFFFF),
    .wr_burst_len    (10'd1),
    .wr_rst          (wr_rst_req),

    .rd_fifo_rd_clk  (sys_clk),
    .rd_fifo_rd_req  (spi_rd_en),
    .sdram_rd_b_addr (24'd0),
    .sdram_rd_e_addr (24'hFFFFFF),
    .rd_burst_len    (10'd1),
    .rd_rst          (rd_rst_req),
    .rd_fifo_rd_data (sdram_data_out),
    .rd_fifo_num     (rd_fifo_num),

    .read_valid      (sys_state == SYS_SPI_TX),
    .pingpang_en     (1'b0),
    .init_end        (sdram_init_end),

    .sdram_clk       (sdram_clk),
    .sdram_cke       (sdram_cke),
    .sdram_cs_n      (sdram_cs_n),
    .sdram_ras_n     (sdram_ras_n),
    .sdram_cas_n     (sdram_cas_n),
    .sdram_we_n      (sdram_we_n),
    .sdram_ba        (sdram_ba),
    .sdram_addr      (sdram_addr),
    .sdram_dqm       (sdram_dqm),
    .sdram_dq        (sdram_dq)
);

spi_master_tx u_spi (
    .sys_clk           (sys_clk),
    .sys_rst_n         (rst_n),
    .send_enable       (sys_state == SYS_SPI_TX && rd_fifo_num > 10'd0),
    .device_id         (device_id),
    .token             (token),
    .timestamp         (timestamp),
    .jpeg_size         (saved_jpeg_size),
    .jpeg_fifo_rd_en   (spi_rd_en),
    .jpeg_fifo_rd_data (sdram_data_out[7:0]),
    .spi_clk           (spi_sck),
    .spi_mosi          (spi_mosi),
    .spi_cs_n          (spi_cs_n),
    .send_done         (spi_done),
    .spi_miso          (spi_miso)
);

assign led_1 = ~sdram_init_end;
assign led_2 = ~(sys_state == SYS_CAPTURE);
assign led_3 = ~(sys_state == SYS_SPI_TX);
assign led_4 = ~cfg_done;

endmodule
