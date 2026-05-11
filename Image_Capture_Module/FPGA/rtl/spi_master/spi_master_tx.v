`timescale 1ns/1ns

module spi_master_tx (
    input  wire         sys_clk,
    input  wire         sys_rst_n,
    input  wire         send_enable,
    input  wire [255:0] device_id,
    input  wire [255:0] token,
    input  wire [31:0]  timestamp,
    input  wire [31:0]  jpeg_size,
    output reg          jpeg_fifo_rd_en,
    input  wire [7:0]   jpeg_fifo_rd_data,
    output reg          spi_clk,
    output reg          spi_mosi,
    output reg          spi_cs_n,
    output reg          send_done,
    input  wire         spi_miso
);

localparam SPI_CLK_DIV = 5;

localparam ST_IDLE      = 4'd0;
localparam ST_HEADER    = 4'd1;
localparam ST_DEVICE_ID = 4'd2;
localparam ST_TOKEN     = 4'd3;
localparam ST_TIMESTAMP = 4'd4;
localparam ST_SIZE      = 4'd5;
localparam ST_JPEG_PRE  = 4'd6;
localparam ST_JPEG      = 4'd7;
localparam ST_TRAILER   = 4'd8;
localparam ST_FINISH    = 4'd9;

reg [3:0]  state;
reg [15:0] clk_cnt;
reg [2:0]  bit_cnt;
reg [31:0] byte_cnt;
reg [7:0]  tx_byte;
reg        spi_clk_en;
reg        send_d1;
reg        send_d2;

wire send_pulse = send_d1 & ~send_d2;
wire clk_tick = spi_clk_en && (clk_cnt == SPI_CLK_DIV - 1);

always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        send_d1 <= 1'b0;
        send_d2 <= 1'b0;
    end else begin
        send_d1 <= send_enable;
        send_d2 <= send_d1;
    end
end

always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        spi_clk <= 1'b1;
        clk_cnt <= 16'd0;
    end else if (spi_clk_en) begin
        if (clk_tick) begin
            clk_cnt <= 16'd0;
            spi_clk <= ~spi_clk;
        end else begin
            clk_cnt <= clk_cnt + 16'd1;
        end
    end else begin
        spi_clk <= 1'b1;
        clk_cnt <= 16'd0;
    end
end

always @(*) begin
    case (state)
        ST_HEADER:
            tx_byte = (byte_cnt == 32'd0) ? 8'h55 : 8'hAA;
        ST_DEVICE_ID:
            tx_byte = device_id[255 - byte_cnt[4:0] * 8 -: 8];
        ST_TOKEN:
            tx_byte = token[255 - byte_cnt[4:0] * 8 -: 8];
        ST_TIMESTAMP:
            tx_byte = timestamp[31 - byte_cnt[1:0] * 8 -: 8];
        ST_SIZE:
            tx_byte = jpeg_size[31 - byte_cnt[1:0] * 8 -: 8];
        ST_JPEG:
            tx_byte = jpeg_fifo_rd_data;
        ST_TRAILER:
            tx_byte = (byte_cnt == 32'd0) ? 8'hCC : 8'h33;
        default:
            tx_byte = 8'h00;
    endcase
end

always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        state           <= ST_IDLE;
        bit_cnt         <= 3'd0;
        byte_cnt        <= 32'd0;
        spi_mosi        <= 1'b0;
        spi_cs_n        <= 1'b1;
        spi_clk_en      <= 1'b0;
        jpeg_fifo_rd_en <= 1'b0;
        send_done       <= 1'b0;
    end else begin
        jpeg_fifo_rd_en <= 1'b0;
        send_done       <= 1'b0;

        case (state)
            ST_IDLE: begin
                spi_cs_n   <= 1'b1;
                spi_clk_en <= 1'b0;
                bit_cnt    <= 3'd0;
                byte_cnt   <= 32'd0;
                spi_mosi   <= 1'b0;

                if (send_pulse) begin
                    spi_cs_n   <= 1'b0;
                    spi_clk_en <= 1'b1;
                    state      <= ST_HEADER;
                end
            end

            ST_HEADER, ST_DEVICE_ID, ST_TOKEN, ST_TIMESTAMP, ST_SIZE, ST_JPEG, ST_TRAILER: begin
                if (clk_tick && spi_clk == 1'b1) begin
                    spi_mosi <= tx_byte[7 - bit_cnt];

                    if (bit_cnt == 3'd7) begin
                        bit_cnt <= 3'd0;

                        case (state)
                            ST_HEADER: begin
                                if (byte_cnt == 32'd1) begin
                                    byte_cnt <= 32'd0;
                                    state <= ST_DEVICE_ID;
                                end else begin
                                    byte_cnt <= byte_cnt + 32'd1;
                                end
                            end

                            ST_DEVICE_ID: begin
                                if (byte_cnt == 32'd31) begin
                                    byte_cnt <= 32'd0;
                                    state <= ST_TOKEN;
                                end else begin
                                    byte_cnt <= byte_cnt + 32'd1;
                                end
                            end

                            ST_TOKEN: begin
                                if (byte_cnt == 32'd31) begin
                                    byte_cnt <= 32'd0;
                                    state <= ST_TIMESTAMP;
                                end else begin
                                    byte_cnt <= byte_cnt + 32'd1;
                                end
                            end

                            ST_TIMESTAMP: begin
                                if (byte_cnt == 32'd3) begin
                                    byte_cnt <= 32'd0;
                                    state <= ST_SIZE;
                                end else begin
                                    byte_cnt <= byte_cnt + 32'd1;
                                end
                            end

                            ST_SIZE: begin
                                if (byte_cnt == 32'd3) begin
                                    byte_cnt <= 32'd0;
                                    if (jpeg_size == 32'd0)
                                        state <= ST_TRAILER;
                                    else
                                        state <= ST_JPEG_PRE;
                                end else begin
                                    byte_cnt <= byte_cnt + 32'd1;
                                end
                            end

                            ST_JPEG: begin
                                if (byte_cnt == jpeg_size - 32'd1) begin
                                    byte_cnt <= 32'd0;
                                    state <= ST_TRAILER;
                                end else begin
                                    byte_cnt <= byte_cnt + 32'd1;
                                    jpeg_fifo_rd_en <= 1'b1;
                                end
                            end

                            ST_TRAILER: begin
                                if (byte_cnt == 32'd1) begin
                                    byte_cnt <= 32'd0;
                                    state <= ST_FINISH;
                                end else begin
                                    byte_cnt <= byte_cnt + 32'd1;
                                end
                            end

                            default: begin
                                byte_cnt <= 32'd0;
                                state <= ST_IDLE;
                            end
                        endcase
                    end else begin
                        bit_cnt <= bit_cnt + 3'd1;
                    end
                end
            end

            ST_JPEG_PRE: begin
                jpeg_fifo_rd_en <= 1'b1;
                state <= ST_JPEG;
            end

            ST_FINISH: begin
                spi_cs_n   <= 1'b1;
                spi_clk_en <= 1'b0;
                send_done  <= 1'b1;
                state      <= ST_IDLE;
            end

            default: state <= ST_IDLE;
        endcase
    end
end

endmodule
