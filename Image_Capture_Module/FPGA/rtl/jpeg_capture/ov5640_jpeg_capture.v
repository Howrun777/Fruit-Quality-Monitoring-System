`timescale 1ns/1ns

module ov5640_jpeg_capture (
    input  wire        sys_clk, sys_rst_n, capture_req,
    input  wire        ov5640_pclk, ov5640_href, ov5640_vsync,
    input  wire [7:0]  ov5640_data,
    output wire        sccb_scl, inout wire sccb_sda,
    
    output reg  [7:0]  jpeg_data,
    output reg         jpeg_data_en,
    output reg  [31:0] jpeg_size,
    output reg         capture_done
);

// [此处请保留你的 I2C/SCCB 初始化逻辑]

// JPEG 截取 FSM (pclk 时钟域)
reg [1:0] cap_state;
reg req_sync1, req_sync2;
always @(posedge ov5640_pclk) begin req_sync1 <= capture_req; req_sync2 <= req_sync1; end

reg [7:0] data_d1; reg href_d1;
reg [31:0] byte_cnt; reg wait_d9;

always @(posedge ov5640_pclk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        cap_state <= 0; jpeg_data <= 0; jpeg_data_en <= 0; 
        jpeg_size <= 0; capture_done <= 0; byte_cnt <= 0;
    end else begin
        capture_done <= 0; jpeg_data_en <= 0;
        data_d1 <= ov5640_data; href_d1 <= ov5640_href;
        
        case (cap_state)
            0: if (req_sync2) cap_state <= 1; // 收到请求，准备抓图
            1: begin // 找 FF D8
                if (href_d1 && data_d1 == 8'hFF && ov5640_data == 8'hD8) begin
                    jpeg_data <= 8'hFF; jpeg_data_en <= 1;
                    byte_cnt <= 1; cap_state <= 2;
                end
            end
            2: begin // 存图中，找 FF D9
                if (href_d1) begin
                    if (data_d1 == 8'hFF && ov5640_data == 8'hD9) begin
                        jpeg_data <= 8'hFF; jpeg_data_en <= 1;
                        byte_cnt <= byte_cnt + 1; wait_d9 <= 1;
                        cap_state <= 3;
                    end else begin
                        jpeg_data <= data_d1; jpeg_data_en <= 1;
                        byte_cnt <= byte_cnt + 1;
                    end
                end
            end
            3: begin // 写入最后一个 D9 并结束
                if (wait_d9) begin
                    jpeg_data <= 8'hD9; jpeg_data_en <= 1;
                    jpeg_size <= byte_cnt + 1; wait_d9 <= 0;
                    capture_done <= 1; cap_state <= 0;
                end
            end
        endcase
    end
end
endmodule