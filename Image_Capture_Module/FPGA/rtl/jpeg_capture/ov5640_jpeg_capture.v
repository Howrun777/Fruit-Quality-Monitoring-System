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

localparam ST_IDLE       = 3'd0;
localparam ST_WAIT_FRAME = 3'd1;
localparam ST_STREAM     = 3'd2;
localparam ST_PAD        = 3'd3;
localparam ST_DRAIN      = 3'd4;
localparam ST_WAIT_LOW   = 3'd5;

localparam [3:0]  SDRAM_BURST_WORDS = 4'd8;
localparam [15:0] DRAIN_CYCLES      = 16'd1024;

reg [2:0]  state;
reg        req_meta;
reg        req_sync;
wire       req_active = req_sync;

reg        vsync_d;
reg        prev_valid;
reg [7:0]  prev_byte;
reg        soi_seen;
reg [3:0]  pad_count;
reg [15:0] drain_count;
reg [31:0] byte_cnt;

function [3:0] burst_pad_count;
    input [31:0] size;
    begin
        if (size[2:0] == 3'd0)
            burst_pad_count = 4'd0;
        else
            burst_pad_count = SDRAM_BURST_WORDS - {1'b0, size[2:0]};
    end
endfunction

always @(posedge ov5640_pclk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        req_meta   <= 1'b0;
        req_sync   <= 1'b0;
    end else begin
        req_meta   <= capture_req;
        req_sync   <= req_meta;
    end
end

always @(posedge ov5640_pclk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        state        <= ST_IDLE;
        vsync_d      <= 1'b0;
        prev_valid   <= 1'b0;
        prev_byte    <= 8'd0;
        soi_seen     <= 1'b0;
        pad_count    <= 4'd0;
        drain_count  <= 16'd0;
        byte_cnt     <= 32'd0;
        jpeg_data    <= 8'd0;
        jpeg_data_en <= 1'b0;
        jpeg_size    <= 32'd0;
        capture_done <= 1'b0;
    end else begin
        vsync_d      <= ov5640_vsync;
        jpeg_data_en <= 1'b0;
        capture_done <= 1'b0;

        case (state)
            ST_IDLE: begin
                byte_cnt    <= 32'd0;
                prev_valid  <= 1'b0;
                soi_seen    <= 1'b0;
                pad_count   <= 4'd0;
                drain_count <= 16'd0;
                if (req_active)
                    state <= ST_WAIT_FRAME;
            end

            ST_WAIT_FRAME: begin
                byte_cnt   <= 32'd0;
                prev_valid <= 1'b0;
                soi_seen   <= 1'b0;
                if (!req_active) begin
                    state <= ST_IDLE;
                end else if (ov5640_vsync && !vsync_d) begin
                    state <= ST_STREAM;
                end
            end

            ST_STREAM: begin
                if (!req_active) begin
                    state <= ST_IDLE;
                end else if (ov5640_vsync && !vsync_d && byte_cnt != 32'd0) begin
                    jpeg_size   <= byte_cnt;
                    pad_count   <= burst_pad_count(byte_cnt);
                    drain_count <= 16'd0;
                    state       <= (burst_pad_count(byte_cnt) == 4'd0) ? ST_DRAIN : ST_PAD;
                end else if (ov5640_href) begin
                    jpeg_data    <= ov5640_data;
                    jpeg_data_en <= 1'b1;
                    byte_cnt     <= byte_cnt + 32'd1;
                    prev_byte    <= ov5640_data;
                    prev_valid   <= 1'b1;

                    if (prev_valid && prev_byte == 8'hFF && ov5640_data == 8'hD8)
                        soi_seen <= 1'b1;

                    if (soi_seen && prev_valid && prev_byte == 8'hFF && ov5640_data == 8'hD9) begin
                        jpeg_size   <= byte_cnt + 32'd1;
                        pad_count   <= burst_pad_count(byte_cnt + 32'd1);
                        drain_count <= 16'd0;
                        state       <= (burst_pad_count(byte_cnt + 32'd1) == 4'd0) ? ST_DRAIN : ST_PAD;
                    end
                end
            end

            ST_PAD: begin
                jpeg_data    <= 8'h00;
                jpeg_data_en <= 1'b1;
                if (pad_count <= 4'd1) begin
                    pad_count   <= 4'd0;
                    drain_count <= 16'd0;
                    state       <= ST_DRAIN;
                end else begin
                    pad_count <= pad_count - 4'd1;
                end
            end

            ST_DRAIN: begin
                if (drain_count == DRAIN_CYCLES) begin
                    capture_done <= 1'b1;
                    state        <= ST_WAIT_LOW;
                end else begin
                    drain_count <= drain_count + 16'd1;
                end
            end

            ST_WAIT_LOW: begin
                byte_cnt     <= 32'd0;
                prev_valid   <= 1'b0;
                soi_seen     <= 1'b0;
                capture_done <= 1'b1;
                if (!req_active)
                    state <= ST_IDLE;
            end

            default: state <= ST_IDLE;
        endcase
    end
end

endmodule
