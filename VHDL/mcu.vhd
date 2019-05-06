library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- MCU
-- probably want put Wishbone bus on M32 user I/O

ENTITY MCU IS
generic (
  ROMsize:  integer := 13                       	-- log2 (ROM cells) 2^13 = 32 KB
);
port (
  clk	  : in	std_logic;							-- System clock
  reset	  : in	std_logic;							-- Active high, synchronous reset
  -- Bit-banged I/O
  bbout	  : out std_logic_vector(7 downto 0);
  bbin	  : in	std_logic_vector(7 downto 0);
  -- UART
  rxd	  : in	std_logic;
  txd	  : out std_logic 
);
END MCU;


ARCHITECTURE RTL OF MCU IS

COMPONENT M32
generic (
  RAMsize:	integer := 10							-- log2 (RAM cells)
);
port (
  clk	  : in	std_logic;							-- System clock
  reset	  : in	std_logic;							-- Active high, synchronous reset
  -- Flash word-read
  caddr	  : out std_logic_vector(25 downto 0);		-- Flash memory address
  cready  : in	std_logic;							-- Flash memory data ready
  cdata	  : in	std_logic_vector(31 downto 0);		-- Flash memory read data
  -- Bit-banged I/O
  bbout	  : out std_logic_vector(7 downto 0);
  bbin	  : in	std_logic_vector(7 downto 0);
  -- Stream in
  kreq	  : in	std_logic;							-- KEY request
  kdata_i : in	std_logic_vector(7 downto 0);
  kack	  : out std_logic;							-- KEY is finished
  -- Stream out
  ereq	  : out std_logic;							-- EMIT request
  edata_o : out std_logic_vector(7 downto 0);
  eack	  : in	std_logic;							-- EMIT is finished
  -- Powerdown
  bye	  : out std_logic							-- BYE encountered
);
END COMPONENT;

  signal now_addr:  std_logic_vector(25 downto 0) := (others=>'1');
  signal bye:       std_logic;

  signal cready:    std_logic := '0';
  signal caddr:     std_logic_vector(25 downto 0);
  signal cdata:     std_logic_vector(31 downto 0);

  signal kreq:      std_logic := '0';
  signal kack:      std_logic;
  signal kdata_i:   std_logic_vector(7 downto 0) := x"27";

  signal ereq:      std_logic;
  signal eack:      std_logic := '0';
  signal edata_o:   std_logic_vector(7 downto 0);

---------------------------------------------------------------------------------
BEGIN

main: process (clk) is
begin
  if (rising_edge(clk)) then
	if (reset='1') then
	else
	end if;
  end if;
end process main;

cpu: m32
GENERIC MAP ( RAMsize => 10 )
PORT MAP (
  clk => clk,  reset => reset,  bye => bye,
  caddr => caddr,  cready => cready,  cdata => cdata,
  bbout => bbout,  bbin => bbin,
  kreq => kreq,  kdata_i => kdata_i,  kack => kack,
  ereq => ereq,  edata_o => edata_o,  eack => eack
);

rom: rom32
GENERIC MAP ( Size => ROMsize )
PORT MAP (
  clk => clk,  addr => caddr(ROMsize-1 downto 0),  data_o => cdata
);

cready <= '1' when caddr = now_addr else '0';

-- generate the `cready` signal
rom_process: process (clk) is
begin
  if rising_edge(clk) then
    if reset = '1' then
      now_addr <= (others => '1');
	else
      now_addr <= caddr;
    end if;
  end if;
end process rom_process;

END RTL;
