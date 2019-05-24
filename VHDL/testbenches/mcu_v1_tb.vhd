-- m32 testbench

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
USE IEEE.VITAL_timing.ALL;

use IEEE.std_logic_textio.all;
use STD.textio.all;

ENTITY mcu_tb IS
generic (
  ROMsize:  integer := 10;                      	-- log2 (ROM cells)
  RAMsize:  integer := 10                       	-- log2 (RAM cells)
);
END mcu_tb;

ARCHITECTURE testbench OF mcu_tb IS

component mcu
generic (
  ROMsize : integer := 13;                      	-- log2 (ROM cells)
  RAMsize : integer := 10;                      	-- log2 (RAM cells)
  clk_Hz  : integer := 100000000                    -- default clk in Hz
);
port (
  clk	  : in	std_logic;							-- System clock
  reset	  : in	std_logic;							-- Active high, synchronous reset
  bye	  : out	std_logic;							-- BYE encountered
  -- SPI flash
  NCS     : out	std_logic;                          -- chip select
  SCLK    : out	std_logic;                          -- clock
  fdata   : inout std_logic_vector(3 downto 0);     -- 3:0 = HLD NWP SO SI, pulled high
  -- UART
  rxd	  : in	std_logic;
  txd	  : out std_logic
);
end component;

  signal clk:       std_logic := '1';
  signal reset:     std_logic := '1';
  signal bye:       std_logic;

  signal rxd:       std_logic := '1';
  signal txd:       std_logic;

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
  signal NCS, SCLK: std_logic;
  signal fdata: std_logic_vector(3 downto 0);

  -- Clock period definitions
  constant clk_period: time := 10 ns;
  constant baud_period : time := 8680.55 ns; -- 115200 bps

BEGIN

  sys: mcu
  GENERIC MAP ( ROMsize => ROMsize, RAMsize => RAMsize, clk_Hz => 100000000 )
  PORT MAP (
    clk => clk,  reset => reset,  bye => bye,
    NCS => NCS,  SCLK => SCLK,  fdata => fdata,
    rxd => rxd,  txd => txd
  );

  RESETNeg <= not reset;
  fdata <= "HHHH";      -- pullups

  mem: s25fl064l
  GENERIC MAP (
    tdevice_PU  => 1 ns,  -- power up very fast
    UserPreload => TRUE,  -- load a file from the current workspace
    TimingModel => "               ",
    secr_file_name => "secr.txt",
    mem_file_name => "testrom.txt")
  PORT MAP (
    SCK => SCLK,
    SO => fdata(1),
    CSNeg => NCS,
    IO3RESETNeg => fdata(3),
    WPNeg => fdata(2),
    SI => fdata(0),
    RESETNeg => RESETNeg
  );

-- Clock generator
clk_process: process
begin
  clk <= '1';  wait for clk_period/2;
  clk <= '0';  wait for clk_period/2;
end process clk_process;

-- TXD stream monitor outputs to console

emit_process: process is
file outfile: text;
variable f_status: FILE_OPEN_STATUS;
variable buf_out: LINE; -- EMIT fills, LF dumps
variable char: std_logic_vector(7 downto 0);
begin
  file_open(f_status, outfile, "STD_OUTPUT", write_mode);
  loop
    wait until txd /= '1';  wait for 1.5*baud_period; -- start bit
    for i in 0 to 7 loop
      char(i) := txd;  wait for baud_period;
    end loop;
    assert txd = '1' report "Missing STOP bit" severity error;
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


main_process: process

procedure KeyChar (char: in std_logic_vector(7 downto 0)) is
begin              -- transmit a serial character
  rxd <= '0';      wait for baud_period;        -- start
  for i in 0 to 7 loop
  rxd <= char(i);  wait for baud_period;        -- bits
  end loop;
  rxd <= '1';      wait for baud_period*5;        -- stop
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
  wait until bye = '1';
  report "BYE encountered"  severity failure;
  wait;
end process main_process;

END testbench;

