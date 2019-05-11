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
  signal counter:  integer range 0 to 31;
  signal xo, yo:   unsigned(31 downto 0);       -- math inputs
  signal divisor:  unsigned(31 downto 0);       -- register
  signal diffd:    unsigned(32 downto 0);       -- divider subtractor
  signal xom, yom: unsigned(15 downto 0);       -- multiplier inputs
  signal product:  unsigned(31 downto 0);       -- multiplier output
  signal mulsum:   unsigned(47 downto 0);       -- multiplier sum
  type   state_t is (idle, multiplying, dividing, divdone);
  signal state: state_t;

---------------------------------------------------------------------------------
begin

main: process (clk, reset) begin
  if (reset='1') then
    divisor <= x"00000000";  xo <= x"00000000";  xom <= x"0000";  busy <= '0';
    result  <= x"00000000";  yo <= x"00000000";  yom <= x"0000";  state <= idle;
    product <= x"00000000";  cycles <= (others=>'0');  counter <= 0;
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
          counter <= 5;  busy <= '1';
          xom <= T(15 downto 0);  yom <= N(15 downto 0);
        when others => null;
        end case;
      end if;
    when multiplying =>                         -- with 16x16 hardware multiply
      product <= xom * yom;                     -- 16x16 multiplier with registered output
      case counter is
      when 5 =>                 xom <= T(31 downto 16);  yom <= N(31 downto 16);
      when 4 => xo <= product;  xom <= T(31 downto 16);  yom <= N(15 downto 0);
      when 3 => yo <= product;  xom <= T(15 downto 0);   yom <= N(31 downto 16);
      when 2 => yo <= mulsum(47 downto 16);  xo(31 downto 16) <= mulsum(15 downto 0);
      when others => yo <= mulsum(47 downto 16);
        result <= mulsum(15 downto 0) & xo(15 downto 0);
        busy <= '0';  state <= idle;
      end case;
      counter <= counter - 1;
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

-- intermediate result adder for um*
mulsum <= (yo & xo(31 downto 16)) + (x"0000" & product);

end math1;
