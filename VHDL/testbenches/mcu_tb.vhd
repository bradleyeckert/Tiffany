-- m32 testbench

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

use IEEE.std_logic_textio.all;
use STD.textio.all;

ENTITY mcu_tb IS
generic (
  ROMsize:  integer := 13                           -- log2 (ROM cells) 2^13 = 32 KB
);
END mcu_tb;

ARCHITECTURE testbench OF mcu_tb IS

component mcu
generic (
  ROMsize:  integer := 13;                      	-- log2 (ROM cells) 2^13 = 32 KB
  clk_Hz:   integer := 100000000                    -- default clk in Hz
);
port (
  clk	  : in	std_logic;							-- System clock
  reset	  : in	std_logic;							-- Active high, synchronous reset
  bye	  : out	std_logic;							-- BYE encountered
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

  -- Clock period definitions
  constant clk_period: time := 10 ns;
  constant baud_period : time := 8680.55 ns; -- 115200 bps

BEGIN

  sys: mcu
  GENERIC MAP ( ROMsize => ROMsize )
  PORT MAP (
    clk => clk,  reset => reset,  bye => bye,
    rxd => rxd,  txd => txd
  );

-- Clock generator
clk_process: process
begin
  clk <= '1'; wait for clk_period/2;
  clk <= '0'; wait for clk_period/2;
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
  rxd <= '1';      wait for baud_period;        -- stop
end procedure;

procedure Keyboard(S: string) is
begin
  for i in 1 to S'length loop
    KeyChar (std_logic_vector(to_unsigned(character'pos(S(i)), 8)));
  end loop;
  KeyChar (x"0D");
  KeyChar (x"0A");
end procedure;

begin
  wait for clk_period*2.2;  reset <= '0';
  wait for 4 ms; -- wait for the ok> prompt
  Keyboard ("see see");
  wait until bye = '1';
  report "BYE encountered"  severity failure;
  wait;
end process main_process;

END testbench;

