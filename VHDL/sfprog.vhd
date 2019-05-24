library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- SPI flash xport to UART interface
-- Intercepts UART escape sequences:
-- 10h 0Eh 12h puts the MCU in reset and starts accepting commands from the UART.
-- 10h 0Fh 12h releases the MCU and allows UART to feed through.
-- 10h 00h-03h translates to 10h-13h.

ENTITY sfprog IS
generic (
  PID: unsigned(31 downto 0) := x"12345678"     -- Product ID
);
port (
  clk	  : in	std_logic;			            -- System clock
  reset	  : in	std_logic;			            -- Asynchronous reset
  hold    : out	std_logic;                      -- resets the CPU, makes SFIF trigger from xtrig
  busy	  : in	std_logic;			            -- flash is busy
  -- SPI flash
  xdata_o : out std_logic_vector(9 downto 0);
  xdata_i : in  std_logic_vector(7 downto 0);
  xtrig   : out std_logic;
  xbusy   : in  std_logic;
  -- UART register interface: connects to the UART
  ready_u : in std_logic;                    	-- Ready for next byte to send
  wstb_u  : out std_logic;                    	-- Send strobe
  wdata_u : out std_logic_vector(7 downto 0); 	-- Data to send
  rxfull_u: in  std_logic;                    	-- RX buffer is full, okay for host to accept
  rstb_u  : out std_logic;                    	-- Accept RX byte
  rdata_u : in  std_logic_vector(7 downto 0);	-- Received data
  -- UART register interface: connects to the MCU
  ready   : out std_logic;                    	-- Ready for next byte to send
  wstb    : in  std_logic;                    	-- Send strobe
  wdata   : in  std_logic_vector(7 downto 0); 	-- Data to send
  rxfull  : out std_logic;                    	-- RX buffer is full, okay to accept
  rstb    : in  std_logic;                    	-- Accept RX byte
  rdata   : out std_logic_vector(7 downto 0) 	-- Received data
);
END sfprog;


ARCHITECTURE RTL OF sfprog IS

  type c_state_t is (c_idle, c_esc, c_ready);
  signal c_state: c_state_t;

  signal fdata_o:  std_logic_vector(3 downto 0);
  signal fdrive_o: std_logic_vector(3 downto 0);
  signal fdata_i:  std_logic_vector(3 downto 0);
  signal config:   std_logic_vector(7 downto 0);
  signal sfbusy:   std_logic;

---------------------------------------------------------------------------------
BEGIN

rdata <= rdata_u;                                   -- feed through

main: process (clk, reset) is
begin
  if reset = '1' then
    c_state <= c_idle;
    wstb_u <= '0';
    wdata_u <= x"00";
    rstb_u <= '0';
  elsif rising_edge(clk) then
  end if;
end process main;

END RTL;
