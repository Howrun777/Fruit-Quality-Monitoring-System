`timescale 1ns/1ns

module uart_cmd_parser (
    input  wire        sys_clk,            
    input  wire        sys_rst_n,          
    input  wire        uart_rx,            
    
    output reg         cmd_start,          
    output reg         cmd_valid,          
    output reg  [255:0] device_id,          
    output reg  [255:0] token,              
    output reg  [31:0] timestamp,          
    output reg         cmd_error           
);

localparam ID_MAX_LEN = 32, TOKEN_MAX_LEN = 32, TIME_LEN = 10;  
localparam ASCII_C = 8'h43, ASCII_A = 8'h41, ASCII_M = 8'h4D;
localparam ASCII_BAR = 8'h7C;
localparam ASCII_0 = 8'h30, ASCII_9 = 8'h39;

localparam IDLE=0, WAIT_C=1, WAIT_A=2, WAIT_M=3;
localparam EXTRACT_ID=4, EXTRACT_TOKEN=5, EXTRACT_TIME=6, DONE=7, ERROR=8;

wire rx_done; wire [7:0] rx_data;
reg [3:0] state, state_next;
reg [5:0] id_cnt, token_cnt, time_cnt;
reg [7:0] id_buffer [0:31];     
reg [7:0] token_buffer [0:31];  
reg [31:0] temp_timestamp;      

uart_rx #(
    .CLK_FREQ(50_000_000),
    .UART_BAUD(9600)
) uart_rx_inst (
    .sys_clk(sys_clk), .sys_rst_n(sys_rst_n),
    .uart_rx(uart_rx), .rx_done(rx_done), .rx_data(rx_data)
);

// 智能超时定时器 (10ms)
reg [19:0] timeout_cnt;
always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) timeout_cnt <= 0;
    else if (state != EXTRACT_TIME) timeout_cnt <= 0; // 只在提取时间时生效
    else if (rx_done) timeout_cnt <= 0;               // 来新数据就清零
    else timeout_cnt <= timeout_cnt + 1;
end
wire timeout = (timeout_cnt >= 20'd500_000); // 10ms

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
            // 完美边界处理：遇到非数字（如回车/空格），立刻完成！
            if (rx_done && (rx_data < ASCII_0 || rx_data > ASCII_9)) begin
                state_next = DONE;
            end 
            // 完美边界处理：时间戳正好收满 10 个数字，立刻完成！
            else if (rx_done && time_cnt >= TIME_LEN - 1) begin
                state_next = DONE;
            end
            // 完美边界处理：短数字（如发了 60）且后续没发回车，超时 10ms 后强行完成！
            else if (timeout && time_cnt > 0) begin
                state_next = DONE;
            end
        end
        
        DONE, ERROR: state_next = IDLE;
        default: state_next = IDLE;
    endcase
end

integer i;
always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        id_cnt <= 0; token_cnt <= 0; time_cnt <= 0; temp_timestamp <= 0;
        for (i=0; i<32; i=i+1) id_buffer[i] <= 8'h20;
        for (i=0; i<32; i=i+1) token_buffer[i] <= 8'h20;
        cmd_start <= 0;
    end else begin
        cmd_start <= 0; 
        if (state == IDLE && rx_done && rx_data == ASCII_C) cmd_start <= 1; 
        
        if (state == IDLE) begin
            id_cnt <= 0; token_cnt <= 0; time_cnt <= 0; temp_timestamp <= 0; 
            for (i=0; i<32; i=i+1) id_buffer[i] <= 8'h20;
            for (i=0; i<32; i=i+1) token_buffer[i] <= 8'h20;
        end else if (rx_done) begin
            if (state == EXTRACT_ID && rx_data != ASCII_BAR && id_cnt < ID_MAX_LEN) begin
                id_buffer[id_cnt] <= rx_data;
                id_cnt <= id_cnt + 1;
            end
            else if (state == EXTRACT_TOKEN && rx_data != ASCII_BAR && token_cnt < TOKEN_MAX_LEN) begin
                token_buffer[token_cnt] <= rx_data;
                token_cnt <= token_cnt + 1;
            end
            // 收到数字就进行移位累加
            else if (state == EXTRACT_TIME && rx_data >= ASCII_0 && rx_data <= ASCII_9 && time_cnt < TIME_LEN) begin
                temp_timestamp <= (temp_timestamp << 3) + (temp_timestamp << 1) + (rx_data - ASCII_0);
                time_cnt <= time_cnt + 1;
            end
        end
    end
end

always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        cmd_valid <= 0; cmd_error <= 0;
        device_id <= 0; token <= 0; timestamp <= 0;
    end else begin
        cmd_valid <= 0; cmd_error <= 0;
        if (state == DONE) begin
            cmd_valid <= 1;  
            // 对于短数字(60)或满长数字(1777818165)，这套机制保证 temp_timestamp 都是准的！
            timestamp <= temp_timestamp; 
            device_id <= {id_buffer[0], id_buffer[1], id_buffer[2], id_buffer[3],
                          id_buffer[4], id_buffer[5], id_buffer[6], id_buffer[7],
                          id_buffer[8], id_buffer[9], id_buffer[10], id_buffer[11],
                          id_buffer[12], id_buffer[13], id_buffer[14], id_buffer[15],
                          id_buffer[16], id_buffer[17], id_buffer[18], id_buffer[19],
                          id_buffer[20], id_buffer[21], id_buffer[22], id_buffer[23],
                          id_buffer[24], id_buffer[25], id_buffer[26], id_buffer[27],
                          id_buffer[28], id_buffer[29], id_buffer[30], id_buffer[31]};
            token <= {token_buffer[0], token_buffer[1], token_buffer[2], token_buffer[3],
                      token_buffer[4], token_buffer[5], token_buffer[6], token_buffer[7],
                      token_buffer[8], token_buffer[9], token_buffer[10], token_buffer[11],
                      token_buffer[12], token_buffer[13], token_buffer[14], token_buffer[15],
                      token_buffer[16], token_buffer[17], token_buffer[18], token_buffer[19],
                      token_buffer[20], token_buffer[21], token_buffer[22], token_buffer[23],
                      token_buffer[24], token_buffer[25], token_buffer[26], token_buffer[27],
                      token_buffer[28], token_buffer[29], token_buffer[30], token_buffer[31]};
        end else if (state == ERROR) begin
            cmd_error <= 1;
        end
    end
end
endmodule
