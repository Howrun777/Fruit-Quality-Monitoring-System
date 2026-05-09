`timescale 1ns/1ns

module uart_cmd_parser (
    input  wire        sys_clk,            
    input  wire        sys_rst_n,          
    input  wire        uart_rx,            
    
    output reg         cmd_start,          
    output reg         cmd_valid,          
    // 【修改 1】：输出位宽改为 256 位 (32字节)
    output reg [255:0] device_id,          
    output reg [255:0] token,              
    output reg  [31:0] timestamp,          
    output reg         cmd_error           
);

// 【修改 2】：常量长度限制扩大
localparam ID_MAX_LEN = 32, TOKEN_MAX_LEN = 32, TIME_LEN = 10;  
localparam ASCII_C = 8'h43, ASCII_A = 8'h41, ASCII_M = 8'h4D;
localparam ASCII_BAR = 8'h7C;
localparam ASCII_0 = 8'h30, ASCII_9 = 8'h39;

localparam IDLE=0, WAIT_C=1, WAIT_A=2, WAIT_M=3;
localparam EXTRACT_ID=4, EXTRACT_TOKEN=5, EXTRACT_TIME=6, DONE=7, ERROR=8;

wire rx_done; wire [7:0] rx_data;
reg [3:0] state, state_next;
reg [5:0] id_cnt, token_cnt, time_cnt;

// 【修改 3】：缓存数组深度扩大到 32 字节
reg [7:0] id_buffer [0:31];     
reg [7:0] token_buffer [0:31];  
reg [31:0] temp_timestamp;      

uart_rx uart_rx_inst (
    .sys_clk(sys_clk), .sys_rst_n(sys_rst_n),
    .uart_rx(uart_rx), .rx_done(rx_done), .rx_data(rx_data)
);

reg [19:0] timeout_cnt;
always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) timeout_cnt <= 0;
    else if (state != EXTRACT_TIME) timeout_cnt <= 0; 
    else if (rx_done) timeout_cnt <= 0;               
    else timeout_cnt <= timeout_cnt + 1;
end
wire timeout = (timeout_cnt >= 20'd500_000); 

always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) state <= IDLE; else state <= state_next;
end

always @(*) begin
    state_next = state;
    case (state)
        IDLE: if (rx_done && rx_data == ASCII_C) state_next = WAIT_C;
        WAIT_C: if (rx_done) state_next = (rx_data == ASCII_C) ? WAIT_C : (rx_data == ASCII_A ? WAIT_A : IDLE);
        WAIT_A: if (rx_done) state_next = (rx_data == ASCII_C) ? WAIT_C : (rx_data == ASCII_M ? WAIT_M : IDLE);
        WAIT_M: if (rx_done) state_next = (rx_data == ASCII_C) ? WAIT_C : (rx_data == ASCII_BAR ? EXTRACT_ID : IDLE);
        
        EXTRACT_ID: if (rx_done) begin
            if (rx_data == ASCII_BAR) state_next = EXTRACT_TOKEN;
            else if (id_cnt >= ID_MAX_LEN) state_next = ERROR;
        end
        
        EXTRACT_TOKEN: if (rx_done) begin
            if (rx_data == ASCII_BAR) state_next = EXTRACT_TIME;
            else if (token_cnt >= TOKEN_MAX_LEN) state_next = ERROR;
        end
        
        EXTRACT_TIME: begin
            if (rx_done && (rx_data < ASCII_0 || rx_data > ASCII_9)) state_next = DONE;
            else if (rx_done && time_cnt >= TIME_LEN - 1) state_next = DONE;
            else if (timeout && time_cnt > 0) state_next = DONE;
        end
        
        DONE, ERROR: state_next = IDLE;
        default: state_next = IDLE;
    endcase
end

integer i;
always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        id_cnt <= 0; token_cnt <= 0; time_cnt <= 0; temp_timestamp <= 0;
        for (i=0; i<32; i=i+1) id_buffer[i] <= 0;
        for (i=0; i<32; i=i+1) token_buffer[i] <= 0;
        cmd_start <= 0;
    end else begin
        cmd_start <= 0; 
        if (state == IDLE && rx_done && rx_data == ASCII_C) cmd_start <= 1; 
        
        if (state == IDLE) begin
            id_cnt <= 0; token_cnt <= 0; time_cnt <= 0; temp_timestamp <= 0; 
        end else if (rx_done) begin
            if (state == EXTRACT_ID && rx_data != ASCII_BAR && id_cnt < ID_MAX_LEN) begin
                id_buffer[id_cnt] <= rx_data; // 取消了 4 个字节的长度限制
                id_cnt <= id_cnt + 1;
            end
            else if (state == EXTRACT_TOKEN && rx_data != ASCII_BAR && token_cnt < TOKEN_MAX_LEN) begin
                token_buffer[token_cnt] <= rx_data; // 取消了 8 个字节的长度限制
                token_cnt <= token_cnt + 1;
            end
            else if (state == EXTRACT_TIME && rx_data >= ASCII_0 && rx_data <= ASCII_9 && time_cnt < TIME_LEN) begin
                temp_timestamp <= (temp_timestamp << 3) + (temp_timestamp << 1) + (rx_data - ASCII_0);
                time_cnt <= time_cnt + 1;
            end
        end
    end
end

integer j;
always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        cmd_valid <= 0; cmd_error <= 0;
        device_id <= 0; token <= 0; timestamp <= 0;
    end else begin
        cmd_valid <= 0; cmd_error <= 0;
        if (state == DONE) begin
            cmd_valid <= 1;  
            timestamp <= temp_timestamp; 
            // 【修改 4】：循环拼接 32 个字节
            for(j=0; j<32; j=j+1) begin
                device_id[255 - j*8 -: 8] <= id_buffer[j];
                token[255 - j*8 -: 8]     <= token_buffer[j];
            end
        end else if (state == ERROR) begin
            cmd_error <= 1;
        end
    end
end
endmodule