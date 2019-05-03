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
  reset:  in  std_logic;                        -- sync reset
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

  -- RAM early-read cache
  signal er_addr:       std_logic_vector(Size-1 downto 0);
  signal er_data:       std_logic_vector(31 downto 0);
  signal rddata:        std_logic_vector(31 downto 0);

---------------------------------------------------------------------------------
BEGIN

-- RAM early read: If reading from an address just written, use the cached value.
-- Simulates read-through, which some Block RAM doesn't support.
-- Any write clears indeterminate cache.

earlyrd: process(clk)
begin
  if (rising_edge(clk)) then
    if (reset='1') then
      er_addr <= (others=>'0');
      er_data <= (others=>'0');
    elsif (en = '1') and (we = '1') then
      er_addr <= addr;
      er_data <= data_i;
    end if;
  end if;
end process earlyrd;

data_o <= er_data when (addr = er_addr) else rddata;


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
        rddata <= lane3(to_integer(unsigned(addr)))
                & lane2(to_integer(unsigned(addr)))
                & lane1(to_integer(unsigned(addr)))
                & lane0(to_integer(unsigned(addr)));
      end if;
    end if;
  end if;
end process mem_proc;

END RTL;
