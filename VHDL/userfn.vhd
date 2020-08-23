library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- M32 user functions

-- You should have different architectures depending on the features you want.
-- This (math1) architecture has hardware multiply and divide.

entity userfn is
generic (
  RAMsize: integer := 10                        -- log2 (RAM cells)
);
port (
  clk     : in  std_logic;                      -- System clock
  reset   : in  std_logic;                      -- Asynchronous reset
  -- Parameters
  N, T    : in  unsigned(31 downto 0);          -- top of stack
  fnsel   : in  unsigned(7 downto 0);           -- function select
  result  : out unsigned(31 downto 0);          -- output
  -- RAM access
  RAM_i	  : in  std_logic_vector(31 downto 0);
  RAM_o	  : out std_logic_vector(31 downto 0);
  RAMaddr : out unsigned(RAMsize-1 downto 0);
  RAMwe   : out std_logic;                      -- write enable
  RAMre   : out std_logic;                      -- read enable
  -- Fishbone Bus Master for burst transfers, feeds thru to MCU
  CYC_O   : out std_logic;                      -- Trigger burst of IMM-1 words
  WE_O    : out std_logic;                      -- '1'=write, '0'=read.
  BLEN_O  : out std_logic_vector(7 downto 0);	-- Burst length less 1.
  BADR_O  : out std_logic_vector(31 downto 0);  -- Block address, copy of T.
  VALID_O : out std_logic;	                    -- AXI-type handshake for output.
  READY_I : in  std_logic;
  DAT_O	  : out std_logic_vector(31 downto 0);  -- Outgoing data, 32-bit.
  VALID_I : in  std_logic;                      -- AXI-type handshake for input.
  READY_O : out std_logic;
  DAT_I   : in  std_logic_vector(31 downto 0);	-- Incoming data, 32-bit.
  -- handshaking
  start   : in  std_logic;                      -- strobe
  busy    : out std_logic                       -- crunching...
);
end userfn;

architecture math1 of userfn is

  signal cycles:   unsigned(41 downto 0);       -- tick counter
  signal counter:  integer range 0 to 32;
  signal xo, yo:   unsigned(31 downto 0);       -- math inputs
  signal divisor:  unsigned(31 downto 0);       -- register
  signal diffd:    unsigned(32 downto 0);       -- divider subtractor
  signal ma, mq:   unsigned(31 downto 0);       -- mul registers
  signal mulsum:   unsigned(32 downto 0);       -- mul adder
  type state_t is (idle, multiplying, dividing, divdone, burst_rd, burst_rdx, burst_wr, burst_wrx);
  signal state: state_t;

  signal burstcnt, burstlen: unsigned(7 downto 0); -- burst count and length
  signal RAMaddr_i: unsigned(RAMsize-1 downto 0);
  type memtype is array (0 to 7) of std_logic_vector(31 downto 0);
  signal mem: memtype;							-- shallow FIFO
  signal head, tail, flen: unsigned(2 downto 0);-- FIFO pointers
  signal RAMre_i, RAMre0: std_logic;
  signal VALID_OI, READY_OI: std_logic;

---------------------------------------------------------------------------------
begin

BLEN_O <= std_logic_vector(burstlen);
RAMaddr <= RAMaddr_i;  RAMre <= RAMre_i;
flen <= head - tail;
VALID_O <= VALID_OI;
RAMwe <= READY_OI;
RAM_o <= DAT_I;
READY_O <= READY_OI;

main: process (clk, reset) begin
  if (reset='1') then
    divisor <= x"00000000";  xo <= x"00000000";  ma <= x"00000000";  busy <= '0';
    result  <= x"00000000";  yo <= x"00000000";  mq <= x"00000000";  state <= idle;
    cycles <= (others=>'0');  counter <= 0;
	RAMaddr_i <= (others=>'0');  RAMre_i <= '0';
    CYC_O <= '0';  WE_O <= '0';  BADR_O <= (others=>'0');
    DAT_O  <= x"00000000";  READY_OI <= '0';  burstcnt <= x"00";  burstlen <= x"00";
	head <= "000";  tail <= "000";  RAMre0 <= '0';  VALID_OI <= '0';
  elsif (rising_edge(clk)) then
    cycles <= cycles + 1;
    case state is
    when idle =>
      if start = '1' then
        case fnsel is
        when x"02" =>                           -- Counter: 1024 beats per tick
          result <= cycles(41 downto 10);
        when x"03" =>                           -- set divisor, get remainder or mult result
          divisor <= T;  result <= yo;
        when x"04" =>
          if T >= divisor then                  -- division overflow
            xo <= x"FFFFFFFF";                  -- quotient
            yo <= x"00000000";                  -- remainder
          else
            state <= dividing;                  -- start UM/MOD
            yo <= T;  xo <= N;  counter <= 31;  busy <= '1';
          end if;
        when x"05" =>
          state <= multiplying;                 -- start UM*
          counter <= 32;  busy <= '1';
          ma <= x"00000000";  mq <= N;          -- use dedicated registers instead of yo, xo
		when x"07" =>
          burstlen <= T(7 downto 0);			-- set burst length
		when x"08" =>							-- read burst
          BADR_O <= std_logic_vector(T);
          burstcnt <= burstlen;  WE_O <= '0';  CYC_O <= '1';  busy <= '1';
		  state <= burst_rd;  RAMaddr_i <= N(RAMsize-1 downto 0);
		when x"09" =>							-- write burst
          BADR_O <= std_logic_vector(T);
          burstcnt <= burstlen;  WE_O <= '1';  CYC_O <= '1';  busy <= '1';
		  state <= burst_wr;  RAMaddr_i <= N(RAMsize-1 downto 0);
		  RAMre_i <= '1';
        when others => null;
        end case;
      end if;
    when multiplying =>                         -- iterative multiply
      if mq(0) = '1' then
        ma <= mulsum(32 downto 1);
        mq <= mulsum(0) & mq(31 downto 1) ;
      else
        ma <= '0' & ma(31 downto 1);            -- a_q >> 1
        mq <= ma(0) & mq(31 downto 1) ;
      end if;
      if counter = 0 then
        result <= mq;  yo <= ma;
        busy <= '0';  state <= idle;
      else
        counter <= counter - 1;
      end if;
    when dividing =>                            -- binary long division
      if (diffd(32)='0') or (yo(31)='1') then
        xo <= xo(30 downto 0) & '1';
        yo <= diffd(31 downto 0);
      else
        xo <= xo(30 downto 0) & '0';
        yo <= yo(30 downto 0) & xo(31);
      end if;
      if counter = 0 then
        state <= divdone;
      else
        counter <= counter - 1;
      end if;
    when divdone =>
      result <= xo;
      busy <= '0';  state <= idle;
	when burst_wr =>							-- Data RAM --> bus
	  RAMre0 <= RAMre_i;
	  RAMre_i <= not flen(2);
	  -- use a FIFO to buffer the data coming from CPU data RAM
	  if RAMre0 = '1' then						-- push into FIFO
	    mem(to_integer(head)) <= RAM_i;
	    head <= head + 1;
	  end if;
	  -- track sync-read RAM address with RAMre_i
      if RAMre_i = '1' then
        RAMaddr_i <= RAMaddr_i + 1;
      end if;
	  -- when data is in the FIFO, output a word to DAT_O and wait for READY_I
      if (flen /= 0) and ((READY_I = '1') or (VALID_OI = '0')) then
        DAT_O <= mem(to_integer(tail));
		VALID_OI <= '1';
        tail <= tail + 1;
	    if burstcnt = 0 then
          state <= burst_wrx;
        else
	      burstcnt <= burstcnt - 1;
		end if;
	  end if;
	when burst_wrx =>
      if (READY_I = '1') then					-- last cycle in write burst
	    VALID_OI <= '0';
	    CYC_O <= '0';  RAMre_i <= '0';
        busy <= '0';  state <= idle;
		head <= "000";  tail <= "000";
	  end if;
	when burst_rd =>
	  if VALID_I = '1' then
	    READY_OI <= '1';
	    if burstcnt = 0 then
          state <= burst_rdx;
        else
	      burstcnt <= burstcnt - 1;
		end if;
      else
	    READY_OI <= '0';
	  end if;
	  if READY_OI = '1' then
        RAMaddr_i <= RAMaddr_i + 1;
	  end if;
	when burst_rdx =>
	  READY_OI <= '0';  CYC_O <= '0';
      busy <= '0';  state <= idle;
    end case;
  end if;
end process main;

-- dividend subtractor for um/mod
diffd <= ('0'&yo(30 downto 0)&xo(31)) - ('0'&divisor);

-- multiplier adder
mulsum <= ('0' & ma) + ('0' & T);

end math1;
