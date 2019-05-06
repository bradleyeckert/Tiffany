-- m32 testbench

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

use IEEE.std_logic_textio.all;
use STD.textio.all;

ENTITY m32_tb IS
generic (
  ROMsize:  integer := 13                       -- log2 (ROM cells) 2^13 = 32 KB
);
END m32_tb;

ARCHITECTURE testbench OF m32_tb IS

component m32
generic (
  RAMsize:  integer := 10                       -- log2 (RAM cells)
);
port (
  clk     : in  std_logic;                      -- System clock
  reset   : in  std_logic;                      -- Active high, synchronous reset
  -- Flash word-read
  caddr   : out std_logic_vector(25 downto 0);  -- Flash memory address
  cready  : in  std_logic;                      -- Flash memory data ready
  cdata   : in  std_logic_vector(31 downto 0);  -- Flash memory read data
  -- Bit-banged I/O
  bbout   : out std_logic_vector(7 downto 0);
  bbin    : in  std_logic_vector(7 downto 0);
  -- Stream in
  kreq    : in  std_logic;                      -- KEY request
  kdata_i : in  std_logic_vector(7 downto 0);
  kack    : out std_logic;                      -- KEY is finished
  -- Stream out
  ereq    : out std_logic;                      -- EMIT request
  edata_o : out std_logic_vector(7 downto 0);
  eack    : in  std_logic;                      -- EMIT is finished
  -- Powerdown
  bye     : out std_logic                       -- BYE encountered
);
end component;

component rom32
generic (
  Size:  integer := 10                          -- log2 (cells)
);
port (
  clk:    in  std_logic;                        -- System clock
  addr:   in  std_logic_vector(Size-1 downto 0);
  data_o: out std_logic_vector(31 downto 0)     -- read data
);
end component;

  signal now_addr:  std_logic_vector(25 downto 0) := (others=>'1');

  signal clk:       std_logic := '1';
  signal reset:     std_logic := '1';
  signal bye:       std_logic;

  signal cready:    std_logic := '0';
  signal caddr:     std_logic_vector(25 downto 0);
  signal cdata:     std_logic_vector(31 downto 0);
  signal bbout:     std_logic_vector(7 downto 0);
  signal bbin:      std_logic_vector(7 downto 0) := x"00";

  signal kreq:      std_logic := '0';
  signal kack:      std_logic;
  signal kdata_i:   std_logic_vector(7 downto 0) := x"27";

  signal ereq:      std_logic;
  signal eack:      std_logic := '0';
  signal edata_o:   std_logic_vector(7 downto 0);

  -- Clock period definitions
  constant clk_period: time := 10 ns;

BEGIN

  cpu: m32
  GENERIC MAP ( RAMsize => 10 )
  PORT MAP (
    clk => clk,  reset => reset,  bye => bye,
    caddr => caddr,  cready => cready,  cdata => cdata,
    bbout => bbout,  bbin => bbin,
    kreq => kreq,  kdata_i => kdata_i,  kack => kack,
    ereq => ereq,  edata_o => edata_o,  eack => eack
  );

  rom: rom32
  GENERIC MAP ( Size => ROMsize )
  PORT MAP (
    clk => clk,  addr => caddr(ROMsize-1 downto 0),  data_o => cdata
  );

cready <= '1' when caddr = now_addr else '0';

-- generate the `cready` signal
rom_process: process (clk) is
begin
  if rising_edge(clk) then
    now_addr <= caddr;
  end if;
end process rom_process;

-- Clock generator
clk_process: process
begin
  clk <= '1'; wait for clk_period/2;
  clk <= '0'; wait for clk_period/2;
end process clk_process;

-- EMIT stream monitor outputs to console

emit_process: process is
file outfile: text;
variable f_status: FILE_OPEN_STATUS;
variable buf_out: LINE; -- EMIT fills, CR dumps
begin
  file_open(f_status, outfile, "STD_OUTPUT", write_mode);
  while bye /= '1' loop
    wait until rising_edge(clk);
    if (ereq = '1') and (eack = '0') then
	  case edata_o is
	  when x"0A" => writeline (output, buf_out); 	-- LF
	  when others =>
	    if edata_o(7 downto 5) /= "000" then
          write (buf_out, character'val(to_integer(unsigned(edata_o))));
	    end if;
	  end case;
      eack <= '1';
    else
      eack <= '0';
    end if;  end loop;
  report "BYE encountered" severity failure;
  wait;
end process emit_process;


main_process: process

procedure KeyChar(c: std_logic_vector(7 downto 0)) is
begin
  kdata_i <= c;
  kreq <= '1';
  wait until kack = '1';
  wait for clk_period;
  kreq <= '0';
  wait for clk_period;
end procedure;

procedure Keyboard(S: string) is
begin
  for i in 1 to S'length loop
    KeyChar (std_logic_vector(to_unsigned(character'pos(S(i)), 8)));
  end loop;
  KeyChar (x"0D");
  KeyChar (x"0A");
end procedure;

begin
  wait for clk_period*2.2;  reset <= '0';
  Keyboard ("see see");
  wait;
end process main_process;

END testbench;

