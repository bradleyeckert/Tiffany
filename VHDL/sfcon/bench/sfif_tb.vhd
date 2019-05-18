library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
USE IEEE.VITAL_timing.ALL;

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

  signal data:    std_logic_vector(3 downto 0);
  signal RESETNeg: std_logic;

  signal sr: std_logic_vector(31 downto 0) := x"12345678";

  constant clk_period : time := 10 ns;          -- 100 MHz


    COMPONENT s25fl064l                         -- flash device
    GENERIC (                                   -- single data rate only
        tdevice_PU          : VitalDelayType  := 4 ms;
        mem_file_name       : STRING    := "s25fl064l.mem";
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


BEGIN

data_i <= data;
g_data: for i in 0 to 3 generate
  data(i) <= data_o(i) when drive_o(i) = '1' else 'H';
end generate g_data;
RESETNeg <= not reset;

   mem: s25fl064l
        GENERIC MAP (
          tdevice_PU  => 1 ns,  -- power up very fast
          UserPreload => TRUE,  -- load a file from the current workspace
          TimingModel => "               ",
          mem_file_name => "testrom.txt")
        PORT MAP (
          SCK => SCLK,
          SO => data(1),
          CSNeg => NCS,
          IO3RESETNeg => data(3),
          WPNeg => data(2),
          SI => data(0),
          RESETNeg => RESETNeg
        );

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
   procedure fetch(a: integer) is begin
      loop
        wait until rising_edge(clk);
        caddr <= std_logic_vector(to_unsigned(a, 30));
        wait for 2 ns;
        if cready='1' then
          exit;
        end if;
      end loop;
   end procedure;
   begin
      wait for clk_period*8.2;  reset <= '0';
      for i in 0 to 15 loop
         fetch(i);
      end loop;
      for i in 12 to 15 loop
         fetch(i);
      end loop;
      wait;
   end process;

END;


