`timescale 1ns/1ns
module spi_master_tx (
    input sys_clk, sys_rst_n, send_enable,
    input [255:0] device_id, input [255:0] token, input [31:0] timestamp, input [31:0] jpeg_size,
    output jpeg_fifo_rd_en, input [7:0] jpeg_fifo_rd_data,
    output reg spi_clk, spi_mosi, spi_cs_n, send_done, input spi_miso
);
localparam SPI_CLK_DIV = 5; 
reg [3:0] state, next_state; reg [15:0] clk_cnt; reg [2:0] bit_cnt; reg [31:0] byte_cnt; reg spi_clk_en;
reg se_d1, se_d2; wire pulse = se_d1 && !se_d2;
always @(posedge sys_clk) begin se_d1 <= send_enable; se_d2 <= se_d1; end

always @(posedge sys_clk) begin
    if (!sys_rst_n) begin clk_cnt<=0; spi_clk<=1; end 
    else if (spi_clk_en) begin
        if (clk_cnt >= SPI_CLK_DIV-1) begin clk_cnt<=0; spi_clk<=~spi_clk; end else clk_cnt<=clk_cnt+1;
    end else begin clk_cnt<=0; spi_clk<=1; end
end
wire fall = spi_clk_en && (clk_cnt==SPI_CLK_DIV-1) && spi_clk;

always @(posedge sys_clk) if(!sys_rst_n) state<=0; else state<=next_state;
always @(*) begin
    next_state = state;
    case (state)
        0: if (pulse) next_state=1;
        1: if (byte_cnt>=1 && bit_cnt==7 && fall) next_state=2;
        2: if (byte_cnt>=31 && bit_cnt==7 && fall) next_state=3;
        3: if (byte_cnt>=31 && bit_cnt==7 && fall) next_state=4;
        4: if (byte_cnt>=3 && bit_cnt==7 && fall) next_state=5;
        5: if (byte_cnt>=3 && bit_cnt==7 && fall) next_state=(jpeg_size==0 ? 7 : 6);
        6: if (byte_cnt>=jpeg_size-1 && bit_cnt==7 && fall) next_state=7;
        7: if (byte_cnt>=1 && bit_cnt==7 && fall) next_state=0;
    endcase
end

wire [7:0] txb = (state==1&&byte_cnt==0)?8'h55 : (state==1&&byte_cnt==1)?8'hAA : (state==2)?device_id[255-byte_cnt*8-:8] :
                 (state==3)?token[255-byte_cnt*8-:8] : (state==4)?timestamp[31-byte_cnt*8-:8] : (state==5)?jpeg_size[31-byte_cnt*8-:8] :
                 (state==6)?jpeg_fifo_rd_data : (state==7&&byte_cnt==0)?8'hCC : (state==7)?8'h33 : 0;

always @(posedge sys_clk) begin
    if (!sys_rst_n) begin spi_cs_n<=1; spi_mosi<=0; spi_clk_en<=0; send_done<=0; bit_cnt<=0; byte_cnt<=0; end 
    else begin
        send_done<=0;
        if (state==0) begin spi_cs_n<=1; spi_clk_en<=0; bit_cnt<=0; byte_cnt<=0; if (pulse) begin spi_cs_n<=0; spi_clk_en<=1; byte_cnt<=0; end end 
        else begin
            if (fall) begin
                spi_mosi<=txb[7-bit_cnt];
                if (bit_cnt==7) begin bit_cnt<=0; if(state!=next_state) byte_cnt<=0; else byte_cnt<=byte_cnt+1; end 
                else bit_cnt<=bit_cnt+1;
            end
            if (state==7 && next_state==0) begin spi_cs_n<=1; spi_clk_en<=0; send_done<=1; end
        end
    end
end
assign jpeg_fifo_rd_en = (state==6) && fall && (bit_cnt==7);
endmodule
