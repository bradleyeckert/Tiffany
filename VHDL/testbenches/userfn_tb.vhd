library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

ENTITY userfn_tb IS
END userfn_tb;

ARCHITECTURE behavior OF userfn_tb IS

COMPONENT userfn
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
END COMPONENT;

   signal clk   : std_logic := '1';
   signal reset : std_logic := '1';
   signal start : std_logic := '0';
   signal busy  : std_logic;
   signal N     : unsigned(31 downto 0) := x"12345678";
   signal T     : unsigned(31 downto 0) := x"87654321";
   signal result: unsigned(31 downto 0);
   signal fnsel : unsigned(7 downto 0) := x"05"; -- default is um*

   constant clk_period : time := 10 ns;         -- 100 MHz

BEGIN

   -- Instantiate the Unit Under Test (UUT)
   uut: userfn
        PORT MAP (
          clk => clk,  reset => reset,
          N => N,  T => T,  fnsel => fnsel,  result => result,
          start => start,  busy => busy
        );

   -- System clock
   clk_process :process is
   begin
      wait for clk_period/2;  clk <= '0';
      wait for clk_period/2;  clk <= '1';
   end process;

   -- Stimulus process
   stim_proc: process is
   begin
      wait for clk_period*2.2;  reset <= '0';
      wait for clk_period*1.3;  start <= '1';
      wait for clk_period;      start <= '0';
      wait until busy = '0';
      wait for clk_period*1.5;
      assert result = x"70B88D78" report "Wrong UM* lower result";
      T <= to_unsigned(1000, 32);
      fnsel <= x"03";           start <= '1';
      wait for clk_period;      start <= '0';
      assert result = x"09A0CD05" report "Wrong UM* upper result";
      T <= to_unsigned(0, 32);
      N <= to_unsigned(123456, 32);
      fnsel <= x"04";           start <= '1';
      wait for clk_period;      start <= '0';
      wait until busy = '0';
      wait for clk_period*0.5;
      assert to_integer(result) = 123 report "Wrong quotient";
      fnsel <= x"03";           start <= '1';
      wait for clk_period;      start <= '0';
      assert to_integer(result) = 456 report "Wrong remainder";
      wait;
   end process;

END;


