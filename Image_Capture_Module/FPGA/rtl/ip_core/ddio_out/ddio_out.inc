<<<<<<< HEAD:Image_Capture_Module/FPGA/project/ip_core/ddio_out/ddio_out.inc
--Copyright (C) 2025  Altera Corporation. All rights reserved.
--Your use of Altera Corporation's design tools, logic functions 
--and other software and tools, and any partner logic 
--functions, and any output files from any of the foregoing 
--(including device programming or simulation files), and any 
--associated documentation or information are expressly subject 
--to the terms and conditions of the Altera Program License 
--Subscription Agreement, the Altera Quartus Prime License Agreement,
--the Altera IP License Agreement, or other applicable license
--agreement, including, without limitation, that your use is for
--the sole purpose of programming logic devices manufactured by
--Altera and sold by Altera or its authorized distributors.  Please
--refer to the Altera Software License Subscription Agreements 
--on the Quartus Prime software download page.


FUNCTION ddio_out 
(
	datain_h[0..0],
	datain_l[0..0],
	outclock
)

RETURNS (
	dataout[0..0]
);
=======
--Copyright (C) 1991-2013 Altera Corporation
--Your use of Altera Corporation's design tools, logic functions 
--and other software and tools, and its AMPP partner logic 
--functions, and any output files from any of the foregoing 
--(including device programming or simulation files), and any 
--associated documentation or information are expressly subject 
--to the terms and conditions of the Altera Program License 
--Subscription Agreement, Altera MegaCore Function License 
--Agreement, or other applicable license agreement, including, 
--without limitation, that your use is for the sole purpose of 
--programming logic devices manufactured by Altera and sold by 
--Altera or its authorized distributors.  Please refer to the 
--applicable agreement for further details.


FUNCTION ddio_out 
(
	datain_h[0..0],
	datain_l[0..0],
	outclock
)

RETURNS (
	dataout[0..0]
);
>>>>>>> 0eefbb50fb538d866accb9c2aca1edc28577a0ed:Image_Capture_Module/FPGA/rtl/ip_core/ddio_out/ddio_out.inc
