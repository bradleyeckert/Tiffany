library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- Single-port RAM, generic

ENTITY SPRAMEZ IS
generic (
  Wide:  integer := 32;                         -- width of word
  Size:  integer := 10                          -- log2 (words)
);
port (
  clk:    in  std_logic;                        -- System clock
  reset:  in  std_logic;                        -- async reset
  en:     in  std_logic;                        -- Memory Enable
  we:     in  std_logic;                        -- Write Enable (0=read, 1=write)
  addr:   in  std_logic_vector(Size-1 downto 0);
  data_i: in  std_logic_vector(Wide-1 downto 0);-- write data
  data_o: out std_logic_vector(Wide-1 downto 0) -- read data
);
END SPRAMEZ;

ARCHITECTURE RTL OF SPRAMEZ IS

  type memtype is array (0 to 2**Size-1) of std_logic_vector(Wide-1 downto 0);
  signal mem: memtype;

---------------------------------------------------------------------------------
BEGIN

mem_proc: process(clk)
begin
  if (rising_edge(clk)) then
    if en='1' then
      if (we='1') then
        mem(to_integer(unsigned(addr))) <= data_i;
      else
        data_o <= mem(to_integer(unsigned(addr)));
      end if;
    end if;
  end if;
end process mem_proc;

END RTL;
