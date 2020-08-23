-- Papilio_One PCB wrapper for mcu_v1
-- XC3S250E FPGA with SST 25VF040B flash
-- 2Mb for bitstream, 2Mb for Forth

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

ENTITY Papilio_One IS
generic (
  ROMsize : integer := 11;                      	-- log2 (ROM cells)
  RAMsize : integer := 11;                      	-- log2 (RAM cells)
  clk_Hz  : integer := 32000000;
  BaseBlock: unsigned(7 downto 0) := x"04"
);
port (
  CLK32MHZ   : in  std_logic;
  -- SPI flash
  qspi_cs     : out	std_logic;
  qspi_clk    : out std_logic;
  qspi_dq     : inout std_logic_vector(3 downto 0);
  -- UART
  uart_txd_in : in	std_logic;
  uart_rxd_out: out std_logic;
  -- Simple I/Os
  led         : out std_logic_vector(7 downto 0);   -- Discrete LEDs
  sw          : in  std_logic_vector(7 downto 0)    -- Switches
);
END Papilio_One;

ARCHITECTURE RTL OF Papilio_One IS

component mcu_v1
generic (
  ROMsize : integer := 10;                      	-- log2 (ROM cells)
  RAMsize : integer := 10;                      	-- log2 (RAM cells)
  clk_Hz  : integer := 100000000;                   -- default clk in Hz
  BaseBlock: unsigned(7 downto 0) := x"00"
);
port (
  clk	  : in	std_logic;							-- System clock
  reset	  : in	std_logic;							-- Active high, synchronous reset
  bye	  : out	std_logic;							-- BYE encountered
  -- SPI flash
  NCS     : out	std_logic;                          -- chip select
  SCLK    : out	std_logic;                          -- clock
  fdata_o : out std_logic_vector(3 downto 0);
  fdrive_o: out std_logic_vector(3 downto 0);
  fdata_i : in std_logic_vector(3 downto 0);
  -- UART
  rxd	  : in	std_logic;
  txd	  : out std_logic;
  -- Fishbone Bus Master for burst transfers
  CYC_O   : out std_logic;                      	-- Trigger burst of IMM-1 words
  WE_O    : out std_logic;                      	-- '1'=write, '0'=read.
  BLEN_O  : out std_logic_vector(7 downto 0);		-- Burst length less 1.
  BADR_O  : out std_logic_vector(31 downto 0);  	-- Block address, copy of T.
  VALID_O : out std_logic;	                    	-- AXI-type handshake for output.
  READY_I : in  std_logic;
  DAT_O	  : out std_logic_vector(31 downto 0);  	-- Outgoing data, 32-bit.
  VALID_I : in  std_logic;                      	-- AXI-type handshake for input.
  READY_O : out std_logic;
  DAT_I   : in  std_logic_vector(31 downto 0)		-- Incoming data, 32-bit.
);
end component;

  signal resetcnt : integer range 0 to 7 := 7;
  signal reset   : std_logic := '1';
  signal bye     : std_logic;
  signal clk     : std_logic;
  signal fdata_o : std_logic_vector(3 downto 0);
  signal fdrive_o: std_logic_vector(3 downto 0);
  signal fdata_i : std_logic_vector(3 downto 0);

  signal CYC_O   : std_logic;
  signal WE_O    : std_logic;
  signal BLEN_O  : std_logic_vector(7 downto 0);
  signal BADR_O  : std_logic_vector(31 downto 0);
  signal VALID_O : std_logic;
  signal READY_I : std_logic;
  signal DAT_O	 : std_logic_vector(31 downto 0);
  signal VALID_I : std_logic;
  signal READY_O : std_logic;
  signal DAT_I   : std_logic_vector(31 downto 0);

  signal bbout   : std_logic_vector(7 downto 0);

BEGIN

  led(0) <= bye;
  led(7 downto 1) <= bbout(6 downto 0);

  clk <= CLK32MHZ;

  fdata_i <= qspi_dq; -- bidirectional SPI data bus
  g_data: for i in 0 to 3 generate
    qspi_dq(i) <= fdata_o(i) when fdrive_o(i) = '1' else 'Z';
  end generate g_data;

  sys: mcu_v1
  GENERIC MAP ( ROMsize => ROMsize, RAMsize => RAMsize,
    clk_Hz => 100000000, BaseBlock => BaseBlock )
  PORT MAP (
    clk => clk,  reset => reset,  bye => bye,
    NCS => qspi_cs,  SCLK => qspi_clk,  fdata_i => fdata_i,  fdata_o => fdata_o,  fdrive_o => fdrive_o,
    rxd => uart_txd_in,  txd => uart_rxd_out,
    CYC_O => CYC_O,  WE_O => WE_O,  BLEN_O => BLEN_O,  BADR_O => BADR_O,
    VALID_O => VALID_O,  READY_I => READY_I,  DAT_O	=> DAT_O,
    VALID_I => VALID_I,  READY_O => READY_O,  DAT_I => DAT_I
  );

  rst_proc: process(clk)
  begin
    if (rising_edge(clk)) then
      if resetcnt = 0 then
	    reset <= '0';
      else
        resetcnt <= resetcnt - 1;
      end if;
    end if;
  end process rst_proc;

  -- Fishbone handshaking
  wr_proc: process(clk, reset) begin
    if reset = '1' then
      READY_I <= '0';  bbout <= (others => '0');
    elsif rising_edge(clk) then
	  READY_I <= VALID_O;   -- use VALID_O as a "write enable", expect 1-cycle bursts
      if VALID_O = '1' then
--      case BADR_O(3 downto 0) is
--      when others =>
        bbout(to_integer(unsigned(DAT_O(3 downto 1)))) <= DAT_O(0);
--      end case;
      end if;
    end if;
  end process wr_proc;

  rd_proc: process(clk, reset) begin
    if reset = '1' then
      DAT_I <= x"00000000";  VALID_I <= '0';
    elsif rising_edge(clk) then
      DAT_I <= x"00000000";
	  if (CYC_O = '1') and (WE_O = '0') then
	    VALID_I <= '1';
--      case BADR_O(3 downto 0) is
--      when others =>
        DAT_I(3 downto 0) <= sw;
--      end case;
      else
	    VALID_I <= '0';
      end if;
    end if;
  end process rd_proc;

END RTL;

