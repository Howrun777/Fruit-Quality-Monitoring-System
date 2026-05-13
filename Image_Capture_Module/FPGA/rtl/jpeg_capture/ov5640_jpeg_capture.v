`timescale 1ns/1ns

module ov5640_jpeg_capture (
    input  wire        sys_clk,
    input  wire        sys_rst_n,
    input  wire        capture_req,
    input  wire        ov5640_pclk,
    input  wire        ov5640_href,
    input  wire        ov5640_vsync,
    input  wire [7:0]  ov5640_data,
    output wire        sccb_scl,
    inout  wire        sccb_sda,

    output reg  [7:0]  jpeg_data,
    output reg         jpeg_data_en,
    output reg  [31:0] jpeg_size,
    output reg         capture_done
);

assign sccb_scl = 1'bz;
assign sccb_sda = 1'bz;

localparam ST_IDLE     = 3'd0;
localparam ST_SEARCH   = 3'd1;
localparam ST_STREAM   = 3'd2;
localparam ST_DONE     = 3'd3;
localparam ST_WAIT_LOW = 3'd4;

reg [2:0]  state;
reg        req_meta;
reg        req_sync;
reg        req_sync_d;
wire       req_active = req_sync;

reg        vsync_d;
reg        href_d;
reg [7:0]  data_d;
reg [31:0] byte_cnt;

always @(posedge ov5640_pclk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        req_meta   <= 1'b0;
        req_sync   <= 1'b0;
        req_sync_d <= 1'b0;
    end else begin
        req_meta   <= capture_req;
        req_sync   <= req_meta;
        req_sync_d <= req_sync;
    end
end

always @(posedge ov5640_pclk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        state        <= ST_IDLE;
        vsync_d      <= 1'b0;
        href_d       <= 1'b0;
        data_d       <= 8'd0;
        byte_cnt     <= 32'd0;
        jpeg_data    <= 8'd0;
        jpeg_data_en <= 1'b0;
        jpeg_size    <= 32'd0;
        capture_done <= 1'b0;
    end else begin
        vsync_d      <= ov5640_vsync;
        href_d       <= ov5640_href;
        data_d       <= ov5640_data;
        jpeg_data_en <= 1'b0;
        capture_done <= 1'b0;

        case (state)
            ST_IDLE: begin
                byte_cnt <= 32'd0;
                if (req_active)
                    state <= ST_SEARCH;
            end

            ST_SEARCH: begin
                byte_cnt <= 32'd0;
                if (!req_active) begin
                    state <= ST_IDLE;
                end else if (href_d && ov5640_href && data_d == 8'hFF && ov5640_data == 8'hD8) begin
                    jpeg_data    <= 8'hFF;
                    jpeg_data_en <= 1'b1;
                    byte_cnt     <= 32'd1;
                    state        <= ST_STREAM;
                end
            end

            ST_STREAM: begin
                if (!req_active) begin
                    state <= ST_IDLE;
                end else if (href_d) begin
                    jpeg_data    <= data_d;
                    jpeg_data_en <= 1'b1;
                    byte_cnt     <= byte_cnt + 32'd1;

                    if (data_d == 8'hFF && ov5640_data == 8'hD9) begin
                        state <= ST_DONE;
                    end
                end
            end

            ST_DONE: begin
                jpeg_data    <= 8'hD9;
                jpeg_data_en <= 1'b1;
                jpeg_size    <= byte_cnt + 32'd1;
                capture_done <= 1'b1;
                state        <= ST_WAIT_LOW;
            end

            ST_WAIT_LOW: begin
                byte_cnt <= 32'd0;
                capture_done <= 1'b1;
                if (!req_active)
                    state <= ST_IDLE;
            end

            default: state <= ST_IDLE;
        endcase

        // 只有在接收数据流的中途(ST_STREAM)，遇到下一帧的 VSYNC 上升沿(帧异常结束)，才强制退出
        // 原来错误地检测 VSYNC 下降沿，会误将新帧开始误判为异常，导致错过 FF D8
        if (state == ST_STREAM && ov5640_vsync && !vsync_d &&
            !(href_d && data_d == 8'hFF && ov5640_data == 8'hD9)) begin
            state <= ST_IDLE;
        end
    end
end

endmodule
