library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

ENTITY sfif_tb IS
END sfif_tb;

ARCHITECTURE behavior OF sfif_tb IS

COMPONENT sfif
generic (
  RAMsize:   integer := 10;                     -- log2 (RAM cells)
  CacheSize: integer := 4;
  BaseBlock: unsigned(7 downto 0) := x"00"
);
port (clk:   in std_logic;
  reset:     in std_logic;                      -- reset is async
-- ROM interface
  caddr:     in std_logic_vector(29 downto 0);
  cdata:    out std_logic_vector(31 downto 0);
  cready:   out std_logic;
-- Configuration
  config:    in std_logic_vector(7 downto 0);   -- HW configuration
-- SPIxfer
  xdata_i:   in std_logic_vector(9 downto 0);
  xdata_o:  out std_logic_vector(7 downto 0);
  xtrig:     in std_logic;
  xbusy:    out std_logic;
-- 6-wire SPI flash connection
  NCS:      out std_logic;                      -- chip select
  SCLK:     out std_logic;                      -- clock
  data_o:   out std_logic_vector(3 downto 0);
  drive_o:  out std_logic_vector(3 downto 0);
  data_i:    in std_logic_vector(3 downto 0)
);
END COMPONENT;

  signal clk:     std_logic := '1';
  signal reset:   std_logic := '1';
  signal caddr:   std_logic_vector(29 downto 0) := (others => '0');
  signal cdata:   std_logic_vector(31 downto 0);
  signal cready:  std_logic;
  signal config:  std_logic_vector(7 downto 0) := "00000000";
  signal xdata_i: std_logic_vector(9 downto 0) := "0101101001";
  signal xdata_o: std_logic_vector(7 downto 0);
  signal xtrig:   std_logic := '0';
  signal xbusy:   std_logic;
  signal NCS:     std_logic;                    -- chip select
  signal SCLK:    std_logic;                    -- clock
  signal data_o:  std_logic_vector(3 downto 0);
  signal drive_o: std_logic_vector(3 downto 0);
  signal data_i:  std_logic_vector(3 downto 0) := "0000";

  signal sr: std_logic_vector(31 downto 0) := x"12345678";

  constant clk_period : time := 10 ns;          -- 100 MHz

-- {rate, speed, dummy, hasmode} in [7:6], [5:4], [3:2], and [1] respectively.
-- rate:	Select rate for all ROM reads: 0=single, 1=double, 2=quad; Default = 0.
-- speed:	SPI interface speed: 3,2=clk/2, 1=clk/4, 0=clk/8; Default = 0.
-- dummy:	Number of dummy cycles to wait for read data: {4, 6, 8, 10}; Default = 2 (8).
-- hasmode:	Include a mode byte after the address: Default = 0.

BEGIN

   -- Instantiate the Unit Under Test (UUT)
   uut: sfif
        GENERIC MAP (RAMsize => 2, CacheSize => 2)
        PORT MAP (
          clk => clk,  reset => reset,  config => config,
          caddr => caddr,  cdata => cdata,  cready => cready,
          xdata_i => xdata_i,  xdata_o => xdata_o,  xtrig => xtrig,  xbusy => xbusy,
          NCS => NCS,  SCLK => SCLK,  data_o => data_o,  drive_o => drive_o,  data_i => data_i
        );

   -- System clock
   clk_process :process is
   begin
      wait for clk_period/2;  clk <= '0';
      wait for clk_period/2;  clk <= '1';
   end process;

   -- Stimulus process
   stim_proc: process is
   begin
      wait for clk_period*8.2;  reset <= '0';
      wait until rising_edge(clk);
      for i in 0 to 10 loop
        caddr <= std_logic_vector(to_unsigned(i, 30));
        wait for 2 ns;
        wait until cready = '1';
        wait until rising_edge(clk);
      end loop;
      wait;
   end process;

   process (clk) is
   begin
      if rising_edge(clk) then
        data_i(0) <= sr(0) after 2 ns;
        sr <= sr(30 downto 0) & sr(31);
      end if;
   end process;

END;


