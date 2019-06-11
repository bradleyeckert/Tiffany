library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- This version of the MCU boots and runs from SPI flash.
-- It's intended to have a wrapper containing signal names matching those of
-- the PCB's constraint file. The wrapper also connects the Fishbone bus to peripherals.

ENTITY MCU IS
generic (
  ROMsize : integer := 10;                      	-- log2 (ROM cells)
  RAMsize : integer := 10;                      	-- log2 (RAM cells)
  clk_Hz  : integer := 100000000;                   -- default clk in Hz
  BaseBlock: unsigned(7 downto 0) := x"00";         -- 64KB blocks reserved for bitstream
  PID: std_logic_vector(31 downto 0) := x"87654321" -- Product ID (for sfcon)
);
port (
  clk	  : in	std_logic;							-- System clock
  reset	  : in	std_logic;							-- Asynchronous reset
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
END MCU;


ARCHITECTURE RTL OF MCU IS

component m32fb
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
  -- Peripheral Bus
  paddr   : out	std_logic_vector(8 downto 0);       -- address
  pwrite  : out std_logic;							-- write strobe
  psel    : out std_logic;							-- start the cycle
  penable : out std_logic;							-- delayed psel
  pwdata  : out std_logic_vector(15 downto 0);      -- write data
  prdata  : in  std_logic_vector(15 downto 0);      -- read data
  pready  : in  std_logic;							-- ready to continue
  -- Fishbone Bus Master for burst transfers.
  CYC_O   : out std_logic;                      	-- Trigger burst of IMM-1 words
  WE_O    : out std_logic;                      	-- '1'=write, '0'=read.
  BLEN_O  : out std_logic_vector(7 downto 0);		-- Burst length less 1.
  BADR_O  : out std_logic_vector(31 downto 0);  	-- Block address, copy of T.
  VALID_O : out std_logic;	                    	-- AXI-type handshake for output.
  READY_I : in  std_logic;
  DAT_O	  : out std_logic_vector(31 downto 0);  	-- Outgoing data, 32-bit.
  VALID_I : in  std_logic;                      	-- AXI-type handshake for input.
  READY_O : out std_logic;
  DAT_I   : in  std_logic_vector(31 downto 0);		-- Incoming data, 32-bit.
  -- Powerdown
  bye     : out std_logic                           -- BYE encountered
);
end component;

  signal cready:    std_logic;
  signal caddr:     std_logic_vector(25 downto 0);
  signal caddrx:    std_logic_vector(29 downto 0);
  signal cdata:     std_logic_vector(31 downto 0);

  signal paddr:     std_logic_vector(8 downto 0);
  signal pwrite:    std_logic;
  signal psel:      std_logic;
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

--  signal fdata_o:  std_logic_vector(3 downto 0);
--  signal fdrive_o: std_logic_vector(3 downto 0);
--  signal fdata_i:  std_logic_vector(3 downto 0);
  signal config:   std_logic_vector(7 downto 0);
  signal xdata_i:  std_logic_vector(9 downto 0);
  signal xdata_o:  std_logic_vector(7 downto 0);
  signal xtrig:    std_logic;
  signal xbusy:    std_logic;
  signal fwait:    std_logic;
  signal sfbusy:   std_logic;

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
  busy:     out std_logic;
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

  signal CPUreset, CPUreset_i: std_logic;
  signal xtrigp, xtrigs:   std_logic;
  signal xdata_op: std_logic_vector(9 downto 0);
  -- UART wiring
  signal ready_u : std_logic;                   -- Ready for next byte to send
  signal wstb_u  : std_logic;                   -- Send strobe
  signal wdata_u : std_logic_vector(7 downto 0);-- Data to send
  signal rxfull_u: std_logic;                   -- RX buffer is full, okay for host to accept
  signal rstb_u  : std_logic;                   -- Accept RX byte
  signal rdata_u : std_logic_vector(7 downto 0);-- Received data

COMPONENT sfprog
generic (
  PID: std_logic_vector(31 downto 0) := x"87654321" -- Product ID
);
port (
  clk	  : in	std_logic;			            -- System clock
  reset	  : in	std_logic;			            -- Asynchronous reset
  hold    : out	std_logic;                      -- resets the CPU, makes SFIF trigger from xtrig
  busy	  : in	std_logic;			            -- flash is busy
  -- SPI flash
  xdata_o : out std_logic_vector(9 downto 0);
  xdata_i : in  std_logic_vector(7 downto 0);
  xtrig   : out std_logic;
  xbusy   : in  std_logic;
  -- UART register interface: connects to the UART
  ready_u : in std_logic;                    	-- Ready for next byte to send
  wstb_u  : out std_logic;                    	-- Send strobe
  wdata_u : out std_logic_vector(7 downto 0); 	-- Data to send
  rxfull_u: in  std_logic;                    	-- RX buffer is full, okay for host to accept
  rstb_u  : out std_logic;                    	-- Accept RX byte
  rdata_u : in  std_logic_vector(7 downto 0);	-- Received data
  -- UART register interface: connects to the MCU
  ready   : out std_logic;                    	-- Ready for next byte to send
  wstb    : in  std_logic;                    	-- Send strobe
  wdata   : in  std_logic_vector(7 downto 0); 	-- Data to send
  rxfull  : out std_logic;                    	-- RX buffer is full, okay to accept
  rstb    : in  std_logic;                    	-- Accept RX byte
  rdata   : out std_logic_vector(7 downto 0) 	-- Received data
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
cpu: m32fb
GENERIC MAP ( RAMsize => RAMsize )
PORT MAP (
  clk => clk,  reset => CPUreset,  bye => bye,
  caddr => caddr,  cready => cready,  cdata => cdata,
  paddr => paddr,  pwrite => pwrite,  psel => psel,  penable => penable,
  pwdata => pwdata,  prdata => prdata,  pready => pready,
  CYC_O => CYC_O,  WE_O => WE_O,  BLEN_O => BLEN_O,  BADR_O => BADR_O,
  VALID_O => VALID_O,  READY_I => READY_I,  DAT_O	=> DAT_O,
  VALID_I => VALID_I,  READY_O => READY_O,  DAT_I => DAT_I
);

flash: sfif
GENERIC MAP (RAMsize => ROMsize, CacheSize => 4, BaseBlock => BaseBlock)
PORT MAP (
  clk => clk,  reset => reset,  config => config,  busy => sfbusy,
  caddr => caddrx,  cdata => cdata,  cready => cready,
  xdata_i => xdata_i,  xdata_o => xdata_o,  xtrig => xtrig,  xbusy => xbusy,
  NCS => NCS,  SCLK => SCLK,  data_o => fdata_o,  drive_o => fdrive_o,  data_i => fdata_i
);

caddrx <= "0000" & caddr;
xtrig <= xtrigp or xtrigs;

ezuart: uart
PORT MAP (
  clk => clk,  reset => reset_a,
  ready => ready_u,    wstb => wstb_u,  wdata => wdata_u,
  rxfull => rxfull_u,  rstb => rstb_u,  rdata => rdata_u,
  bitperiod => bitperiod,  rxd => rxd,  txd => txd
);

xdata_i <= pwdata(9 downto 0) when CPUreset = '0' else xdata_op;

prog_con: sfprog
GENERIC MAP ( PID => PID )
PORT MAP (
  clk => clk,  reset => reset_a,  hold => CPUreset_i,  busy => sfbusy,
  xdata_o => xdata_op,  xdata_i => xdata_o,  xbusy => xbusy,  xtrig => xtrigp,
  ready_u => ready_u,    wstb_u => wstb_u,  wdata_u => wdata_u,
  rxfull_u => rxfull_u,  rstb_u => rstb_u,  rdata_u => rdata_u,
  ready => emitready,  wstb => emit_stb,  wdata => pwdata(7 downto 0),
  rxfull => keyready,  rstb => key_stb,   rdata => keydata
);

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
    emit_stb <= '0';  key_stb <= '0';  xtrigs <= '0';
    bitperiod <= std_logic_vector(to_unsigned(clk_Hz/115200, 16));
	prdata <= x"0000";  pready <= '0';  fwait <= '0';
    config <= x"00";
    CPUreset <= '1';
  elsif rising_edge(clk) then
    emit_stb <= '0';  key_stb <= '0';  xtrigs <= '0';
    CPUreset <= CPUreset_i;             -- synchronize reset from sfcon
    if fwait = '1' then
      if (xbusy = '0') and (xtrigs = '0') then
        fwait <= '0';  pready <= '1';   -- wait for SPI transfer to finish
        prdata <= x"00" & xdata_o;
      end if;
    elsif (psel='1') and (penable='0') then
      pready <= '1';                   	-- usual case: no wait states
      case paddr(3 downto 0) is
      when "0000" =>                    -- R=qkey, W=emit
        prdata <= "000000000000000" & keyready;
        emit_stb <= pwrite;
      when "0001" =>                    -- R=key, W=spixfer
        if pwrite='0' then
          prdata <= x"00" & keydata;
          key_stb <= '1';
        else
          xtrigs <= '1';  fwait <= '1'; -- waiting for SPI transfer
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
	  when "0100" =>					-- R=fbusy
	    prdata <= "000000000000000" & sfbusy; 			-- see flash.f
      when "0101" =>					-- R=BaseBlock
        prdata <= x"00" & std_logic_vector(BaseBlock);  -- see flash.f
      when others =>
        prdata <= x"0000";
      end case;
    else
      pready <= '0';
    end if;
  end if;
end process DPB_process;

END RTL;
