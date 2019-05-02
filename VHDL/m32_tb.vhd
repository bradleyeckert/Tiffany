-- m32 testbench

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

use IEEE.std_logic_textio.all;
use STD.textio.all;

ENTITY m32_tb IS
generic (
  features: std_logic_vector(3 downto 0) := "0000"; -- feature set
  ROMsize:  integer := 12                       -- log2 (ROM cells)
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
variable buf_out: LINE; -- buffers between the program and file
begin
  file_open(f_status, outfile, "STD_OUTPUT", write_mode);
  while bye /= '1' loop
    wait until rising_edge(clk);
    if ereq = '1' then
      write (buf_out, character'val(to_integer(unsigned(edata_o))));
      writeline (output, buf_out);
      eack <= '1';
    else
      eack <= '0';
    end if;
  end loop;
  assert bye /= '1' report "BYE encountered" severity error;
  wait;
end process emit_process;

-- KEY stream generation uses cooked input from console

key_process: process is
file infile: text;
variable f_status: FILE_OPEN_STATUS;
variable buf_in: LINE; -- buffers between the program and file
variable S : string(1 to 1024);
variable ptr: integer := 0;
begin
  if features(0) = '1' then
    file_open(f_status, infile, "STD_INPUT", read_mode);
    while bye /= '1' loop
      readline(infile, buf_in);
      assert buf_in'length < S'length;  -- make sure S is big enough
      ptr := 0;
      if buf_in'length > 0 then
        read(buf_in, S(1 to buf_in'length));
      end if;
      while ptr < buf_in'length loop
        wait until rising_edge(clk);
        kreq <= '1';
        if kack = '1' then
          kdata_i <= std_logic_vector(to_unsigned(character'pos(S(ptr)), 8));
          ptr := ptr + 1;
          kreq <= '0';
        end if;
      end loop;
    end loop;
  end if;
  wait;
end process key_process;



main_process: process
begin
  wait for clk_period*2.2;  reset <= '0';
  wait;
end process main_process;

END testbench;

