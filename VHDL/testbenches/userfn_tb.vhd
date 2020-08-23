library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

ENTITY userfn_tb IS
generic (
  RAMsize: integer := 10                        -- log2 (RAM cells)
);
END userfn_tb;

ARCHITECTURE behavior OF userfn_tb IS

COMPONENT userfn
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
END COMPONENT;

  signal clk     : std_logic := '1';
  signal reset   : std_logic := '1';
  signal start   : std_logic := '0';
  signal busy    : std_logic;
  signal N       : unsigned(31 downto 0) := x"12345678";
  signal T       : unsigned(31 downto 0) := x"87654321";
  signal result  : unsigned(31 downto 0);
  signal fnsel   : unsigned(7 downto 0) := x"05"; -- default is um*
  -- RAM access
  signal RAM_i	 : std_logic_vector(31 downto 0) := x"00000000";
  signal RAM_o	 : std_logic_vector(31 downto 0);
  signal RAMaddr : unsigned(RAMsize-1 downto 0);
  signal RAMwe   : std_logic;
  signal RAMre   : std_logic;
  -- Fishbone Bus Master for burst transfers, feeds thru to MCU
  signal CYC_O   : std_logic;
  signal WE_O    : std_logic;
  signal BLEN_O  : std_logic_vector(7 downto 0);
  signal BADR_O  : std_logic_vector(31 downto 0);
  signal VALID_O : std_logic;
  signal READY_I : std_logic := '1';
  signal DAT_O	 : std_logic_vector(31 downto 0);
  signal VALID_I : std_logic := '1';
  signal READY_O : std_logic;
  signal DAT_I   : std_logic_vector(31 downto 0) := x"00000000";

  constant clk_period : time := 10 ns;         -- 100 MHz
  type memtype is array (0 to 2**RAMsize-1) of std_logic_vector(31 downto 0);
  signal mem: memtype;

BEGIN

  -- Fishbone handshaking
  wr_proc: process(clk)
  begin
    if (rising_edge(clk)) then
	  READY_I <= VALID_O and not READY_I;
    end if;
  end process wr_proc;

  rd_proc: process(clk)
  variable x: std_logic_vector(31 downto 0);
  begin
    if (rising_edge(clk)) then
	  if (CYC_O = '1') and (WE_O = '0') then
	    VALID_I <= '1';
		if READY_O = '1' then
		  DAT_I <= std_logic_vector(unsigned(DAT_I) + 3);
		end if;
      else
	    VALID_I <= '0';
		DAT_I <= x"12340000";
      end if;
    end if;
  end process rd_proc;

  -- Instantiate the Unit Under Test (UUT)
  uut: userfn
  GENERIC MAP (RAMsize => RAMsize)
  PORT MAP (
    clk => clk,  reset => reset,
    N => N,  T => T,  fnsel => fnsel,  result => result,
    RAM_i => RAM_i,  RAM_o => RAM_o,  RAMaddr => RAMaddr,
    RAMwe => RAMwe,  RAMre => RAMre,
    CYC_O => CYC_O,  WE_O => WE_O,  BLEN_O => BLEN_O,  BADR_O => BADR_O,
    VALID_O => VALID_O,  READY_I => READY_I,  DAT_O => DAT_O,
    VALID_I => VALID_I,  READY_O => READY_O,  DAT_I => DAT_I,
    start => start,  busy => busy
  );

  -- System clock
  clk_process :process is
  begin
    wait for clk_period/2;  clk <= '0';
    wait for clk_period/2;  clk <= '1';
  end process;

  mem_proc: process(clk)
  variable init: integer := 1;
  begin
    if (rising_edge(clk)) then
	  if init = 1 then
	     init := 0;
		 for i in 0 to 2**RAMsize-1 loop
		   mem(i) <= std_logic_vector(to_unsigned(263*i, 32));
		 end loop;
	  end if;
      if RAMwe = '1' then
         mem(to_integer(unsigned(RAMaddr))) <= RAM_o;
      elsif RAMre = '1' then
         RAM_i <= mem(to_integer(unsigned(RAMaddr))) after 3 ns;
      end if;
    end if;
  end process mem_proc;

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

    T <= to_unsigned(9, 32);
    fnsel <= x"07";           start <= '1';
    wait for clk_period;      start <= '0';  wait for clk_period;
    T <= to_unsigned(100, 32); -- to BUS[100]
    N <= to_unsigned(200, 32); -- from RAM[200]
    fnsel <= x"09";           start <= '1';  -- burst write
    wait for clk_period;      start <= '0';
    wait until busy = '0';
	wait for clk_period*8;

    fnsel <= x"08";           start <= '1';  -- burst read
    wait for clk_period;      start <= '0';
    wait until busy = '0';
	wait for clk_period*12;

    T <= x"00000000";	-- single-word bursts
    fnsel <= x"07";           start <= '1';
    wait for clk_period;      start <= '0';  wait for clk_period;
    T <= to_unsigned(100, 32); -- to BUS[100]
    N <= to_unsigned(200, 32); -- from RAM[200]
    fnsel <= x"09";           start <= '1';  -- burst write
    wait for clk_period;      start <= '0';
    wait until busy = '0';
	wait for clk_period*8;

    fnsel <= x"08";           start <= '1';  -- burst read
    wait for clk_period;      start <= '0';
    wait until busy = '0';

    wait;
  end process;

END;


