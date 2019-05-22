library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

ENTITY MCU IS
generic (
  ROMsize : integer := 10;                      	-- log2 (ROM cells)
  RAMsize : integer := 10;                      	-- log2 (RAM cells)
  clk_Hz  : integer := 100000000                    -- default clk in Hz
);
port (
  clk	  : in	std_logic;							-- System clock
  reset	  : in	std_logic;							-- Asynchronous reset
  bye	  : out	std_logic;							-- BYE encountered
  -- SPI flash
  NCS     : out	std_logic;                          -- chip select
  SCLK    : out	std_logic;                          -- clock
  fdata   : inout std_logic_vector(3 downto 0);     -- 3:0 = HLD NWP SO SI, pulled high
  -- UART
  rxd	  : in	std_logic;
  txd	  : out std_logic
);
END MCU;


ARCHITECTURE RTL OF MCU IS

component m32
generic (
  RAMsize:  integer := 10                           -- log2 (RAM cells)
);
port (
  clk	  : in	std_logic;							-- System clock
  reset	  : in	std_logic;							-- Asynchronous reset
  -- Flash word-read
  caddr	  : out std_logic_vector(25 downto 0);		-- Flash memory address
  cready  : in	std_logic;							-- Flash memory data ready
  cdata	  : in	std_logic_vector(31 downto 0);		-- Flash memory read data
  -- DPB: Dumb Peripheral Bus, compare to APB (Advanced Peripheral Bus)
  paddr   : out	std_logic_vector(8 downto 0);       -- address
  pwrite  : out std_logic;							-- write strobe
  psel    : out std_logic;							-- start the cycle
  penable : out std_logic;							-- delayed psel
  pwdata  : out std_logic_vector(15 downto 0);      -- write data
  prdata  : in  std_logic_vector(15 downto 0);      -- read data
  pready  : in  std_logic;							-- ready to continue
  -- Powerdown
  bye     : out std_logic                           -- BYE encountered
);
end component;

component rom32
generic (
  Size:  integer := 10                              -- log2 (cells)
);
port (
  clk:    in  std_logic;                            -- System clock
  addr:   in  std_logic_vector(Size-1 downto 0);
  data_o: out std_logic_vector(31 downto 0)         -- read data
);
end component;

  signal cready:    std_logic;
  signal caddr:     std_logic_vector(25 downto 0);
  signal caddrx:    std_logic_vector(29 downto 0);
  signal cdata:     std_logic_vector(31 downto 0);

  signal paddr:     std_logic_vector(8 downto 0);
  signal pwrite:    std_logic;
  signal psel:      std_logic;
--  signal penable:   std_logic;
  signal pwdata:    std_logic_vector(15 downto 0);
  signal prdata:    std_logic_vector(15 downto 0);
  signal pready:    std_logic;
  signal penable:   std_logic;

  signal keyready:  std_logic;
  signal emitready: std_logic;
  signal emit_stb:  std_logic;
  signal key_stb:   std_logic;
  signal keydata:   std_logic_vector(7 downto 0);
  signal bitperiod: std_logic_vector(15 downto 0);

  signal reset_a, reset_b, reset_i: std_logic;      -- reset synchronization

component UART
  port(
    clk:       in std_logic;                    	-- CPU clock
    reset:     in std_logic;                    	-- sync reset
    ready:    out std_logic;                    	-- Ready for next byte to send
    wstb:      in std_logic;                    	-- Send strobe
    wdata:     in std_logic_vector(7 downto 0); 	-- Data to send
    rxfull:   out std_logic;                    	-- RX buffer is full, okay to accept
    rstb:      in std_logic;                    	-- Accept RX byte
    rdata:    out std_logic_vector(7 downto 0); 	-- Received data
    bitperiod: in std_logic_vector(15 downto 0);	-- Clocks per serial bit
    rxd:       in std_logic;
    txd:      out std_logic
  );
end component;

  signal fdata_o:  std_logic_vector(3 downto 0);
  signal fdrive_o: std_logic_vector(3 downto 0);
  signal fdata_i:  std_logic_vector(3 downto 0);
  signal config:   std_logic_vector(7 downto 0);
  signal xdata_i:  std_logic_vector(9 downto 0);
  signal xdata_o:  std_logic_vector(7 downto 0);
  signal xtrig:    std_logic;
  signal xbusy:    std_logic;
  signal fwait:    std_logic;

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

---------------------------------------------------------------------------------
BEGIN

-- Create a clean async reset_a signal for the modules.
-- Synchronize the falling edge to the clock.
process(clk, reset)
begin
  if (reset = '1') then
    reset_a <= '1';  reset_b <= '1';
    reset_i <= '1';
  elsif rising_edge(clk) then
    reset_a <= reset_b;  reset_b <= reset_i;
    reset_i <= '0';
  end if;
end process;

-- Instantiate the components of the MCU: CPU, ROM, and UART
cpu: m32
GENERIC MAP ( RAMsize => RAMsize )
PORT MAP (
  clk => clk,  reset => reset_a,  bye => bye,
  caddr => caddr,  cready => cready,  cdata => cdata,
  paddr => paddr,  pwrite => pwrite,  psel => psel,  penable => penable,
  pwdata => pwdata,  prdata => prdata,  pready => pready
);

flash: sfif
GENERIC MAP (RAMsize => ROMsize, CacheSize => 4)
PORT MAP (
  clk => clk,  reset => reset,  config => config,
  caddr => caddrx,  cdata => cdata,  cready => cready,
  xdata_i => xdata_i,  xdata_o => xdata_o,  xtrig => xtrig,  xbusy => xbusy,
  NCS => NCS,  SCLK => SCLK,  data_o => fdata_o,  drive_o => fdrive_o,  data_i => fdata_i
);

caddrx <= "0000" & caddr;

fdata_i <= fdata; -- bidirectional SPI data bus
g_data: for i in 0 to 3 generate
  fdata(i) <= fdata_o(i) when fdrive_o(i) = '1' else 'Z';
end generate g_data;

urt: uart
PORT MAP (
  clk => clk,  reset => reset_a,
  ready => emitready,  wstb => emit_stb,  wdata => pwdata(7 downto 0),
  rxfull => keyready,  rstb => key_stb,   rdata => keydata,
  bitperiod => bitperiod,  rxd => rxd,  txd => txd
);

xdata_i <= pwdata(9 downto 0);

-- decode the DPB
-- clk     ___----____----____----____----____----____----____----
-- psel    ____----------------___________________________________
-- penable ____________--------___________________________________
-- pready  ____________----------------___________________________ if no delay
-- psel    ____--------------------------------___________________
-- penable ____________------------------------___________________
-- pready  ____________________________----------------___________ 2T delay

DPB_process: process (clk, reset_a) is
begin
  if reset_a = '1' then
    emit_stb <= '0';  key_stb <= '0';  xtrig <= '0';
    bitperiod <= std_logic_vector(to_unsigned(clk_Hz/115200, 16));
	prdata <= x"0000";  pready <= '0';  fwait <= '0';
    config <= x"00";
  elsif rising_edge(clk) then
    emit_stb <= '0';  key_stb <= '0';  xtrig <= '0';
    if fwait = '1' then
      if xbusy = '0' then               -- waiting for SPI transfer to finish
        fwait <= '0';  pready <= '1';
        prdata <= x"00" & xdata_o;
      end if;
    elsif (psel='1') and (penable='0') then
      pready <= '1';                   -- usual case: no wait states
      case paddr(3 downto 0) is
      when "0000" =>                    -- R=qkey, W=emit
        prdata <= "000000000000000" & keyready;
        emit_stb <= pwrite;
      when "0001" =>                    -- R=key, W=spixfer
        if pwrite='0' then
          prdata <= x"00" & keydata;
          key_stb <= '1';
        else
          xtrig <= '1';  fwait <= '1';  -- waiting for SPI transfer
          pready <= '0';
        end if;
      when "0010" =>                    -- R=keyformat, W=spiconfig
        prdata <= x"0001";
        if pwrite='1' then
          config <= pwdata(7 downto 0);
        end if;
      when "0011" =>                    -- R=qemit, W=uartrate
        prdata <= "000000000000000" & emitready;
        if pwrite='1' then
          bitperiod <= pwdata;
        end if;
      when others =>
        prdata <= x"0000";
      end case;
    else
      pready <= '0';
    end if;
  end if;
end process DPB_process;

END RTL;
