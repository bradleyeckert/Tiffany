library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- Single-port synchronous-read ROM that will be inferred by Altera tools

ENTITY ROM32 IS
generic (
  Size:  integer := 10                          -- log2 (cells)
);
port (
  clk:    in  std_logic;                        -- System clock
  addr:   in  std_logic_vector(Size-1 downto 0);
  data_o: out std_logic_vector(31 downto 0)     -- read data
);
END ROM32;

ARCHITECTURE RTL OF ROM32 IS

-- Build a 2-D array type for the RoM
subtype word_t is std_logic_vector(31 downto 0);
type memory_t is array(2**Size-1 downto 0) of word_t;

function init_rom
  return memory_t is
  variable mem : memory_t := (others => (others => '0'));
  begin
`14`  return mem;
end init_rom;

	-- Declare the ROM signal and specify a default value.	Quartus II
	-- will create a memory initialization file (.mif) based on the
	-- default value.
	signal rom : memory_t := init_rom;

signal address: integer range 0 to 2**Size-1;
begin
address <= to_integer(unsigned(addr));

process (clk) begin
  if rising_edge(clk) then
    data_o <= rom(address);
  end if;
end process;

END ARCHITECTURE RTL;
