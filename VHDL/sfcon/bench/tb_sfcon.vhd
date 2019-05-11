LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.std_logic_unsigned.all;
USE ieee.numeric_std.ALL;
USE IEEE.VITAL_timing.ALL;

ENTITY bench IS
END bench;

-- Test single-data-rate flash reading

ARCHITECTURE behavior OF bench IS

    -- Component Declaration for the Unit Under Test (UUT)

    COMPONENT sfif
     port (clk:    in std_logic;
       reset:      in std_logic;
   -- ROM wishbone interface
       rom_addr:   in std_logic_vector(23 downto 0);
       rom_data:  out std_logic_vector(7 downto 0);
       rom_stb:    in std_logic;
       rom_ack:   out std_logic;
   -- I/O wishbone interface
       io_addr:    in std_logic_vector(1 downto 0);
       io_data_i:  in std_logic_vector(7 downto 0);
       io_data_o: out std_logic_vector(7 downto 0);
       io_we:      in std_logic;
       io_stb:     in std_logic;
       io_ack:    out std_logic;
   -- serial flash
       SCLK:     out std_logic;                 -- clock                    6
       MOSI:   inout std_logic;                 -- to flash        / sio1   2
       MISO:   inout std_logic;                 -- data from flash / sio0   5
       NCS:      out std_logic;                 -- chip select              1
       NWP:    inout std_logic;                 -- write protect   / sio2   3
       NHOLD:  inout std_logic);                -- hold            / sio3   7
    END COMPONENT;

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

   --Inputs
   signal clk : std_logic;
   signal reset : std_logic;
   signal rom_addr : std_logic_vector(23 downto 0);
   signal io_data_i : std_logic_vector(7 downto 0);
   signal rom_stb : std_logic := '0';
   signal io_stb : std_logic := '0';
   signal io_addr : std_logic_vector(1 downto 0) := "00";
   signal io_we : std_logic := '0';

   --Outputs
   signal rom_data : std_logic_vector(7 downto 0);
   signal io_data_o : std_logic_vector(7 downto 0);
   signal rom_ack : std_logic;
   signal io_ack : std_logic;

   -- Others
   signal SCLK: std_logic;
   signal MOSI:  std_logic;
   signal MISO:  std_logic;
   signal NCS:  std_logic;
   signal NWP:  std_logic;
   signal RESETNeg:  std_logic := '1';
   signal NHOLD: std_logic;

   signal DeviceID : std_logic_vector(23 downto 0);

   -- Clock period definitions
   constant clk_period : time := 10 ns; -- 50M sclk

BEGIN

        -- Instantiate the Unit Under Test (UUT)
   uut: sfif
        PORT MAP (
          clk => clk,
          reset => reset,
          rom_addr => rom_addr,
          rom_data => rom_data,
          rom_stb => rom_stb,
          rom_ack => rom_ack,
          io_addr => io_addr,
          io_data_i => io_data_i,
          io_data_o => io_data_o,
          io_we => io_we,
          io_stb => io_stb,
          io_ack => io_ack,
          SCLK => SCLK,
          MOSI => MOSI,
          MISO => MISO,
          NCS  => NCS,
          NWP  => NWP,
          NHOLD => NHOLD
        );

   mem: s25fl064l
        GENERIC MAP (
          tdevice_PU  => 1 us,  -- power up very fast
          UserPreload => TRUE,  -- load a file from the current workspace
          TimingModel => "               ",
          mem_file_name => "testrom.txt")
        PORT MAP (
          SCK => SCLK,
          SO => MISO,
          CSNeg => NCS,
          IO3RESETNeg => NHOLD,
          WPNeg => NWP,
          SI => MOSI,
          RESETNeg => RESETNeg
        );

   -- Clock process definitions
   clk_process :process is
   begin
      clk <= '0';  wait for clk_period/2;
      clk <= '1';  wait for clk_period/2;
   end process;

   -- Stimulus process
   stim_proc: process is
   procedure romfetch (addr: in std_logic_vector(23 downto 0)) is
   begin
      rom_addr <= addr;       rom_stb <= '1';
      wait until rom_ack='1';
      wait until clk='0';
      wait until clk='1';     rom_stb <= '0';
      wait for clk_period*1.2;
   end procedure;
   procedure io_write (dat: in std_logic_vector(7 downto 0); adr: in std_logic_vector(1 downto 0)) is
   begin
      io_data_i <= dat;       io_addr <= adr;
      io_we <= '1';           io_stb <= '1';
      wait until io_ack='1';
      wait until CLK='0'; wait until CLK='1'; io_stb <= '0';
      wait until io_ack='0';
      wait for clk_period*2;
   end procedure;
   procedure io_read (adr: std_logic_vector(1 downto 0)) is
   begin
      io_data_i <= x"00";     io_addr <= adr;
      io_we <= '0';           io_stb <= '1';
      wait until io_ack='1';
      wait until CLK='0'; wait until CLK='1'; io_stb <= '0';
      wait until io_ack='0';
      wait for clk_period*2;
   end procedure;
   begin
      reset <= '1';  wait for clk_period*2;
      reset <= '0';
      NWP <= '1';
      NHOLD <= '1';
      wait for 1.1 us;                          -- power-up delay
      io_write(x"00","11");                     -- set SPI speed
      io_write(x"9F","01");                     -- start a transfer: Device ID
      io_write(x"01","00");
      io_read("00"); DeviceID(23 downto 16) <= io_data_o;
      io_write(x"02","00");
      io_read("00"); DeviceID(15 downto 8) <= io_data_o;
      io_write(x"03","10");                     -- raise CS after the next read
      io_read("00"); DeviceID(7 downto 0) <= io_data_o;
      wait for clk_period*33.3;
      romfetch(x"000003");
      romfetch(x"000004");
      romfetch(x"000005");
      romfetch(x"000006");
      romfetch(x"000000");
      wait for 0.2 us;
      romfetch(x"000001");
      wait for 0.3 us;
      romfetch(x"000002");
      romfetch(x"000006");
      wait for 1 us;
      romfetch(x"000000");
      wait;
   end process;

END;










