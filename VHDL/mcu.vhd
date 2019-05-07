library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- A synthesizable MCU
-- Example:  ANS    HelloWorld
-- ROMsize    13    10
-- RAMsize    10     8

ENTITY MCU IS
generic (
  ROMsize:  integer := 10;                      	-- log2 (ROM cells)
  RAMsize:  integer :=  8;                      	-- log2 (RAM cells)
  clk_Hz:   integer := 100000000                    -- default clk in Hz
);
port (
  clk	  : in	std_logic;							-- System clock
  reset	  : in	std_logic;							-- Asynchronous reset
  bye	  : out	std_logic;							-- BYE encountered
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

  signal now_addr:  std_logic_vector(25 downto 0) := (others=>'1');

  signal cready:    std_logic;
  signal caddr:     std_logic_vector(25 downto 0);
  signal cdata:     std_logic_vector(31 downto 0);

  signal paddr:     std_logic_vector(8 downto 0);
  signal pwrite:    std_logic;
  signal psel:      std_logic;
  signal penable:   std_logic;
  signal pwdata:    std_logic_vector(15 downto 0);
  signal prdata:    std_logic_vector(15 downto 0);
  signal pready:    std_logic;

  signal keyready:  std_logic;
  signal emitready: std_logic;
  signal emit_stb:  std_logic;
  signal key_stb:   std_logic;
  signal keydata:   std_logic_vector(7 downto 0);
  signal bitperiod: std_logic_vector(15 downto 0);

  signal reset_a, reset_i: std_logic;               -- reset synchronization

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

---------------------------------------------------------------------------------
BEGIN

-- Create a clean async reset_a signal for the modules.
-- Synchronize the falling edge to the clock.
process(clk, reset)
begin
  if (reset = '1') then
    reset_a <= '1';
    reset_i <= '1';
  elsif rising_edge(clk) then
    reset_a <= reset_i;
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

rom: rom32
GENERIC MAP ( Size => ROMsize )
PORT MAP (
  clk => clk,  addr => caddr(ROMsize-1 downto 0),  data_o => cdata
);

urt: uart
PORT MAP (
  clk => clk,  reset => reset_a,
  ready => emitready,  wstb => emit_stb,  wdata => pwdata(7 downto 0),
  rxfull => keyready,  rstb => key_stb,   rdata => keydata,
  bitperiod => bitperiod,  rxd => rxd,  txd => txd
);

cready <= '1' when caddr = now_addr else '0';

-- generate the `cready` signal
rom_process: process (clk, reset_a) is
begin
  if reset_a = '1' then
    now_addr <= (others => '1');
  elsif rising_edge(clk) then
    now_addr <= caddr;
  end if;
end process rom_process;

-- decode the DPB
DPB_process: process (clk, reset_a) is
begin
  if reset_a = '1' then
    emit_stb <= '0';
    key_stb <= '0';
    bitperiod <= std_logic_vector(to_unsigned(clk_Hz/115200, 16));
  elsif rising_edge(clk) then
    emit_stb <= '0';
    key_stb <= '0';
    pready <= psel;                   -- usual case: no wait states
    if (psel='1') and (pready='0') then
      case paddr(3 downto 0) is
      when "0000" =>                  -- R=qkey, W=emit
        prdata <= "000000000000000" & keyready;
        emit_stb <= pwrite;
      when "0001" =>                  -- R=key, W=spixfer
        if pwrite='0' then
          prdata <= x"00" & keydata;
          key_stb <= '1';
        end if;
      when "0010" =>                  -- R=keyformat, W=spirate
        prdata <= x"0001";
      when "0011" =>                  -- R=qemit, W=uartrate
        prdata <= "000000000000000" & emitready;
        if pwrite='1' then
          bitperiod <= pwdata;
        end if;
      when others =>
        prdata <= x"0000";
      end case;
    end if;
  end if;
end process DPB_process;

END RTL;
