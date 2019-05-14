library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- M32 user functions

-- You should have different architectures depending on the features you want.
-- This (math1) architecture has hardware multiply and divide.

entity userfn is
port (
  clk     : in  std_logic;                      -- System clock
  reset   : in  std_logic;                      -- Asynchronous reset
  -- Parameters
  N, T    : in  unsigned(31 downto 0);          -- top of stack
  fnsel   : in  unsigned(7 downto 0);           -- function select
  result  : out unsigned(31 downto 0);          -- output
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
  type   state_t is (idle, multiplying, dividing, divdone);
  signal state: state_t;

---------------------------------------------------------------------------------
begin


main: process (clk, reset) begin
  if (reset='1') then
    divisor <= x"00000000";  xo <= x"00000000";  ma <= x"00000000";  busy <= '0';
    result  <= x"00000000";  yo <= x"00000000";  mq <= x"00000000";  state <= idle;
    cycles <= (others=>'0');  counter <= 0;
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
    end case;
  end if;
end process main;

-- dividend subtractor for um/mod
diffd <= ('0'&yo(30 downto 0)&xo(31)) - ('0'&divisor);

-- multiplier adder
mulsum <= ('0' & ma) + ('0' & T);

end math1;
