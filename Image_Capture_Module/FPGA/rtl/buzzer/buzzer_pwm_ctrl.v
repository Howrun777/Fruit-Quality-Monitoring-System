/**
 * @file buzzer_pwm_ctrl.v
 * @brief Buzzer PWM Control Module
 * 
 * Generates 3kHz PWM to drive passive buzzer for 2 seconds.
 */

`timescale 1ns/1ns

module buzzer_pwm_ctrl (
    input  wire        sys_clk,        
    input  wire        sys_rst_n,      
    input  wire        cmd_valid,      
    
    output reg         beep            
);

// Parameters
localparam BEEP_FREQ = 3000;           
localparam BEEP_DURATION = 32'd100_000_000;  // 2s at 50MHz

// Fixed Formula for true 3kHz PWM
localparam PWM_PERIOD = 50_000_000 / BEEP_FREQ;  // 16666 counts
localparam PWM_DUTY   = PWM_PERIOD / 2;          // 8333 counts

reg [31:0] duration_cnt;
reg        beep_active;
reg [15:0] pwm_cnt;

// State Control
always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        duration_cnt <= 32'd0;
        beep_active  <= 1'b0;
    end else if (cmd_valid) begin // cmd_valid 是单周期高电平
        duration_cnt <= 32'd1;
        beep_active  <= 1'b1;
    end else if (beep_active) begin
        if (duration_cnt >= BEEP_DURATION) begin
            duration_cnt <= 32'd0;
            beep_active  <= 1'b0;
        end else begin
            duration_cnt <= duration_cnt + 32'd1;
        end
    end
end

// PWM Generator
always @(posedge sys_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        pwm_cnt <= 16'd0;
        beep    <= 1'b0;
    end else if (beep_active) begin
        if (pwm_cnt >= PWM_PERIOD - 1) pwm_cnt <= 16'd0;
        else pwm_cnt <= pwm_cnt + 16'd1;
        
        beep <= (pwm_cnt < PWM_DUTY) ? 1'b1 : 1'b0;
    end else begin
        pwm_cnt <= 16'd0;
        beep    <= 1'b0;
    end
end

endmodule