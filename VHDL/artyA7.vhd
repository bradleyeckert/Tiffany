-- mcu_v1 RTL

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

ENTITY ArtyA7 IS
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
END ArtyA7;

ARCHITECTURE RTL OF ArtyA7 IS

component mcu
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
  fdata   : inout std_logic_vector(3 downto 0);     -- 3:0 = HLD NWP SO SI, pulled high
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

  signal reset   : std_logic := '1';
  signal bye     : std_logic;
  signal clk     : std_logic;

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

  signal bbout   : std_logic_vector(3 downto 0);

BEGIN

  led(0) <= bye;
  led(3 downto 1) <= bbout(2 downto 0);
  clk <= CLK100MHZ;

  sys: mcu
  GENERIC MAP ( ROMsize => ROMsize, RAMsize => RAMsize,
    clk_Hz => 100000000, BaseBlock => BaseBlock )
  PORT MAP (
    clk => clk,  reset => reset,  bye => bye,
    NCS => qspi_cs,  SCLK => qspi_clk,  fdata => qspi_dq,
    rxd => uart_txd_in,  txd => uart_rxd_out,
    CYC_O => CYC_O,  WE_O => WE_O,  BLEN_O => BLEN_O,  BADR_O => BADR_O,
    VALID_O => VALID_O,  READY_I => READY_I,  DAT_O	=> DAT_O,
    VALID_I => VALID_I,  READY_O => READY_O,  DAT_I => DAT_I
  );

  rst_proc: process(clk)
  begin
    if (rising_edge(clk)) then
	  reset <= '0';
    end if;
  end process rst_proc;

  -- Fishbone handshaking
  wr_proc: process(clk, reset) begin
    if reset = '1' then
      READY_I <= '0';  bbout <= (others => '0');
      led0_r <= '0';  led0_g <= '0';  led0_b <= '0';
      led1_r <= '0';  led1_g <= '0';  led1_b <= '0';
      led2_r <= '0';  led2_g <= '0';  led2_b <= '0';
      led3_r <= '0';  led3_g <= '0';  led3_b <= '0';
    elsif rising_edge(clk) then
	  READY_I <= VALID_O;   -- use VALID_O as a "write enable", expect 1-cycle bursts
      if VALID_O = '1' then
        case BADR_O(3 downto 0) is
        when "0000" => led0_r <= DAT_O(2);  led0_g <= DAT_O(1);  led0_b <= DAT_O(0);
        when "0001" => led1_r <= DAT_O(2);  led1_g <= DAT_O(1);  led1_b <= DAT_O(0);
        when "0010" => led2_r <= DAT_O(2);  led2_g <= DAT_O(1);  led2_b <= DAT_O(0);
        when "0011" => led3_r <= DAT_O(2);  led3_g <= DAT_O(1);  led3_b <= DAT_O(0);
        when others => bbout(to_integer(unsigned(DAT_O(2 downto 1))))   <= DAT_O(0);
        end case;
      end if;
    end if;
  end process wr_proc;

  rd_proc: process(clk, reset) begin
    if reset = '1' then
    elsif rising_edge(clk) then
      DAT_I <= x"00000000";
	  if (CYC_O = '1') and (WE_O = '0') then
	    VALID_I <= '1';
        case BADR_O(3 downto 0) is
        when "0000" => DAT_I(3 downto 0) <= sw;
        when others => DAT_I(3 downto 0) <= btn;
        end case;
      else
	    VALID_I <= '0';
      end if;
    end if;
  end process rd_proc;

END RTL;

