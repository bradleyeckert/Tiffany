library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- Single-port synchronous-read ROM

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
signal address: integer range 0 to 2**Size-1;
begin
address <= to_integer(unsigned(addr));

process (clk) begin
  if rising_edge(clk) then
    case address is
`13`      when others => data_o <= x"00000000";
    end case;
  end if;
end process;

END ARCHITECTURE RTL;
