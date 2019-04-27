library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- Single-port RAM, 32-bit with 4 byte lanes.

ENTITY SPRAM32 IS
generic (
  Size:  integer := 10                          -- log2 (cells)
);
port (
  clk:    in  std_logic;                        -- System clock
  en:     in  std_logic;                        -- Memory Enable
  we:     in  std_logic;                        -- Write Enable (0=read, 1=write)
  addr:   in  std_logic_vector(Size-1 downto 0);
  data_i: in  std_logic_vector(31 downto 0);    -- write data
  lanes:  in  std_logic_vector(3 downto 0);
  data_o: out std_logic_vector(31 downto 0)     -- read data
);
END SPRAM32;

ARCHITECTURE RTL OF SPRAM32 IS

  type memtype is array (0 to 2**Size-1) of std_logic_vector(7 downto 0);
  signal lane0, lane1, lane2, lane3: memtype;

---------------------------------------------------------------------------------
BEGIN

mem_proc: process(clk)
begin
  if (rising_edge(clk)) then
    if en='1' then
      if (we='1') then
        if lanes(0)='1' then
          lane0(to_integer(unsigned(addr))) <= data_i(7 downto 0);
        end if;
        if lanes(1)='1' then
          lane1(to_integer(unsigned(addr))) <= data_i(15 downto 8);
        end if;
        if lanes(2)='1' then
          lane2(to_integer(unsigned(addr))) <= data_i(23 downto 16);
        end if;
        if lanes(3)='1' then
          lane3(to_integer(unsigned(addr))) <= data_i(31 downto 24);
        end if;
      else
        data_o <= lane3(to_integer(unsigned(addr)))
                & lane2(to_integer(unsigned(addr)))
                & lane1(to_integer(unsigned(addr)))
                & lane0(to_integer(unsigned(addr)));
      end if;
    end if;
  end if;
end process mem_proc;

END RTL;
