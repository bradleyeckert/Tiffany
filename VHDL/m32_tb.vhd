-- m32 testbench

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

use IEEE.MATH_REAL.ALL;
use IEEE.std_logic_textio.all;
use STD.textio.all;

ENTITY m32_tb IS
END m32_tb;

ARCHITECTURE testbench OF m32_tb IS

component m32
generic (
  ROMsize:  integer := 10;                      -- log2 (ROM cells)
  RAMsize:  integer := 10                       -- log2 (RAM cells)
);
port (
  clk     : in  std_logic;                      -- System clock
  reset   : in  std_logic;                      -- Active high, synchronous reset
  -- Flash word-read
  caddr   : out std_logic_vector(25 downto 0);  -- Flash memory address
  cready  : in  std_logic;                      -- Flash memory data ready
  cdata   : in  std_logic_vector(31 downto 0);  -- Flash memory read data
  -- Bit-banged I/O
  bbout   : out std_logic_vector(7 downto 0);
  bbin    : in  std_logic_vector(7 downto 0);
  -- Stream in
  kreq    : in  std_logic;                      -- KEY request
  kdata_i : in  std_logic_vector(7 downto 0);
  kack    : out std_logic;                      -- KEY is finished
  -- Stream out
  ereq    : out std_logic;                      -- EMIT request
  edata_o : out std_logic_vector(7 downto 0);
  eack    : in  std_logic                       -- EMIT is finished
);
end component;

  signal clk:       std_logic := '1';
  signal reset:     std_logic := '1';

  signal cready:    std_logic := '1';           -- feed all NOPs
  signal caddr:     std_logic_vector(25 downto 0);
  signal cdata:     std_logic_vector(31 downto 0) := x"00000000";
  signal bbout:     std_logic_vector(7 downto 0);
  signal bbin:      std_logic_vector(7 downto 0) := x"00";

  signal kreq:      std_logic := '0';
  signal kack:      std_logic;
  signal kdata_i:   std_logic_vector(7 downto 0) := x"27";

  signal ereq:      std_logic;
  signal eack:      std_logic := '0';
  signal edata_o:   std_logic_vector(7 downto 0);

  -- Clock period definitions
  constant clk_period: time := 10 ns;

BEGIN

  cpu: m32
  GENERIC MAP ( ROMsize => 10, RAMsize => 10 )
  PORT MAP (
    clk => clk,  reset => reset,
    caddr => caddr,  cready => cready,  cdata => cdata,
    bbout => bbout,  bbin => bbin,
    kreq => kreq,  kdata_i => kdata_i,  kack => kack,
    ereq => ereq,  edata_o => edata_o,  eack => eack
  );

-- Clock generator
clk_process: process
begin
  clk <= '1'; wait for clk_period/2;
  clk <= '0'; wait for clk_period/2;
end process;

main_process: process
begin
  wait for clk_period*2.2;  reset <= '0';
  wait;
end process;

END testbench;

