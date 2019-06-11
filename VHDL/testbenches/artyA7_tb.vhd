-- Arty A7-35 testbench

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
USE IEEE.VITAL_timing.ALL;

use IEEE.std_logic_textio.all;
use STD.textio.all;

ENTITY artyA7_tb IS
generic (
  ROMsize:  integer := 10;                      	-- log2 (ROM cells)
  RAMsize:  integer := 10;                      	-- log2 (RAM cells)
  BaseBlock: unsigned(7 downto 0) := x"40"
);
END artyA7_tb;

ARCHITECTURE testbench OF artyA7_tb IS

component ArtyA7
generic (
  ROMsize : integer := 10;                      	-- log2 (ROM cells)
  RAMsize : integer := 10;                      	-- log2 (RAM cells)
  clk_Hz  : integer := 100000000;
  BaseBlock: unsigned(7 downto 0) := x"40"
);
port (
  CLK100MHZ   : in  std_logic;
  -- SPI flash
  qspi_cs     : out	std_logic;
  qspi_clk    : out	std_logic;
  qspi_dq     : inout std_logic_vector(3 downto 0);
  -- UART
  uart_txd_in : in	std_logic;
  uart_rxd_out: out std_logic;
  -- Simple I/Os
  led         : out std_logic_vector(3 downto 0);   -- Discrete LEDs
  sw          : in  std_logic_vector(3 downto 0);   -- Switches
  btn         : in  std_logic_vector(3 downto 0);   -- Buttons
  -- 3-color LEDs
  led0_r, led0_g, led0_b : out std_logic;
  led1_r, led1_g, led1_b : out std_logic;
  led2_r, led2_g, led2_b : out std_logic;
  led3_r, led3_g, led3_b : out std_logic
);
end component;

  signal reset:     std_logic := '1';
  signal CLK100MHZ: std_logic := '1';
  signal qspi_cs     : std_logic;
  signal qspi_clk    : std_logic;
  signal qspi_dq     : std_logic_vector(3 downto 0);
  signal uart_txd_in : std_logic := '1';
  signal uart_rxd_out: std_logic;
  signal led  : std_logic_vector(3 downto 0);           -- Discrete LEDs
  signal sw   : std_logic_vector(3 downto 0) := "1010"; -- Switches
  signal btn  : std_logic_vector(3 downto 0) := "0001"; -- Buttons
  signal led0_r, led0_g, led0_b : std_logic;
  signal led1_r, led1_g, led1_b : std_logic;
  signal led2_r, led2_g, led2_b : std_logic;
  signal led3_r, led3_g, led3_b : std_logic;

COMPONENT s25fl064l                             -- flash device
GENERIC (                                       -- single data rate only
    tdevice_PU          : VitalDelayType  := 4 ms;
    mem_file_name       : STRING    := "s25fl064l.mem";
    secr_file_name      : STRING    := "s25fl064lSECR.mem";
    TimingModel         : STRING;
    UserPreload         : BOOLEAN   := FALSE);
PORT (
    -- Data Inputs/Outputs
    SI                : INOUT std_ulogic := 'U'; -- serial data input/IO0
    SO                : INOUT std_ulogic := 'U'; -- serial data output/IO1
    -- Controls
    SCK               : IN    std_ulogic := 'U'; -- serial clock input
    CSNeg             : IN    std_ulogic := 'U'; -- chip select input
    WPNeg             : INOUT std_ulogic := 'U'; -- write protect input/IO2
    RESETNeg          : INOUT std_ulogic := 'U'; -- reset the chip
    IO3RESETNeg       : INOUT std_ulogic := 'U'  -- hold input/IO3
);
END COMPONENT;

  signal RESETNeg:  std_logic;

  -- Clock period definitions
  constant clk_period: time := 10 ns;
  constant baud_period : time := 8680.55 ns; -- 115200 bps

BEGIN

  sys: ArtyA7
  GENERIC MAP ( ROMsize => ROMsize, RAMsize => RAMsize,
    clk_Hz => 100000000, BaseBlock => BaseBlock )
  PORT MAP (
    CLK100MHZ => CLK100MHZ,
    qspi_cs => qspi_cs,  qspi_clk => qspi_clk,  qspi_dq => qspi_dq,
    uart_txd_in => uart_txd_in,  uart_rxd_out => uart_rxd_out,
    led => led,  sw => sw,  btn => btn,
    led0_r => led0_r,  led0_g => led0_g,  led0_b => led0_b,
    led1_r => led1_r,  led1_g => led1_g,  led1_b => led1_b,
    led2_r => led2_r,  led2_g => led2_g,  led2_b => led2_b,
    led3_r => led3_r,  led3_g => led3_g,  led3_b => led3_b
  );

  RESETNeg <= '1';
  qspi_dq <= "HHHH";      -- pullups

  mem: s25fl064l
  GENERIC MAP (
    tdevice_PU  => 1 ns,  -- power up very fast
    UserPreload => TRUE,  -- load a file from the current workspace
    TimingModel => "               ",
    secr_file_name => "secr.txt",
    mem_file_name => "testrom.txt")
  PORT MAP (
    SCK =>   qspi_clk,
    SO =>    qspi_dq(1),
    CSNeg => qspi_cs,
    IO3RESETNeg => qspi_dq(3),
    WPNeg => qspi_dq(2),
    SI =>    qspi_dq(0),
    RESETNeg => RESETNeg
  );

-- Clock generator
clk_process: process
begin
  CLK100MHZ <= '1';  wait for clk_period/2;
  CLK100MHZ <= '0';  wait for clk_period/2;
end process clk_process;

-- uart_rxd_out stream monitor outputs to console

emit_process: process is
file outfile: text;
variable f_status: FILE_OPEN_STATUS;
variable buf_out: LINE; -- EMIT fills, LF dumps
variable char: std_logic_vector(7 downto 0);
begin
  file_open(f_status, outfile, "STD_OUTPUT", write_mode);
  loop
    wait until uart_rxd_out /= '1';  wait for 1.5*baud_period; -- start bit
    for i in 0 to 7 loop
      char(i) := uart_rxd_out;  wait for baud_period;
    end loop;
    assert uart_rxd_out = '1' report "Missing STOP bit" severity error;
    wait for baud_period;
    case char(7 downto 0) is
    when x"0A" => writeline (output, buf_out); 	-- LF
    when others =>
  	  if char(7 downto 5) /= "000" then -- BL to FFh
        write (buf_out, character'val(to_integer(unsigned(char(7 downto 0)))));
  	  end if;
    end case;
  end loop;
end process emit_process;

process begin   wait until led0_r'event;
  report "LED0_r changed" severity note;
end process;
process begin   wait until led1_r'event;
  report "LED1_r changed" severity note;
end process;
process begin   wait until led2_r'event;
  report "LED2_r changed" severity note;
end process;
process begin   wait until led3_r'event;
  report "LED3_r changed" severity note;
end process;


main_process: process

procedure KeyChar (char: in std_logic_vector(7 downto 0)) is
begin              -- transmit a serial character
  uart_txd_in <= '0';      wait for baud_period;        -- start
  for i in 0 to 7 loop
  uart_txd_in <= char(i);  wait for baud_period;        -- bits
  end loop;
  uart_txd_in <= '1';      wait for baud_period*2;      -- stop*2
  -- Two stop bits are needed to prevent receiver processing delays from piling up.
  -- ACCEPT waits for QEMIT when echoing, so serial out must be faster than the input.
  -- The serial bit rate at 100 MHz could be up to about 3M BPS without a multitasker,
  -- but future multitasker will introduce a PAUSE delay of 2 or more usec.
  -- So, 460K or less is probably best.
end procedure;

procedure Keyboard(S: string) is
begin
  for i in 1 to S'length loop
    KeyChar (std_logic_vector(to_unsigned(character'pos(S(i)), 8)));
  end loop;
  KeyChar (x"0D");
--  KeyChar (x"0A");
end procedure;

begin
  wait for clk_period*2.2;  reset <= '0';
  wait for 5 ms; -- wait for the ok> prompt, 5ms is enough if ROMsize=10. Increase if more.
  Keyboard (": foo swap ; see foo"); -- give this 32 ms to execute
  wait;
end process main_process;

END testbench;

