library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- M32 CPU core
-- ** PRELIMINARY work in progress **

ENTITY M32 IS
generic (
  ROMsize:  integer := 10;                      -- log2 (ROM cells)
  RAMsize:  integer := 10                       -- log2 (RAM cells)
);
port (
  clk     : in  std_logic;                      -- System clock
  reset   : in  std_logic;                      -- Active high, synchronous reset
  -- Flash word-read
  caddr   : out std_logic_vector(23 downto 0);  -- Flash memory address
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
  eack    : in  std_logic                       -- EMIT is finished
);
END M32;

ARCHITECTURE RTL OF M32 IS

COMPONENT SPRAM32
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
END COMPONENT;

-- Single-port for RAM dings us a little on >R and R> and fetch/store.
-- But, is smaller on an ASIC. Let's just use single-port.

  signal xen:    std_logic;                     -- Memory Enable
  signal xwe:    std_logic;                     -- Write Enable (0=read, 1=write)
  signal xaddr:  std_logic_vector(ROMsize-1 downto 0);
  signal xwdata: std_logic_vector(31 downto 0); -- write data
  signal xlanes: std_logic_vector(3 downto 0);
  signal xrdata: std_logic_vector(31 downto 0); -- read data

-- intermediate results
  signal r_logic, r_user, immsum: std_logic_vector(31 downto 0);
  signal r_zeq, zeroequals:  std_logic_vector(31 downto 0);
  signal r_sum:              std_logic_vector(33 downto 0);
  signal immdata, immmask:   std_logic_vector(25 downto 0);
-- final result (to be registered in T)
  signal result:  std_logic_vector(31 downto 0); -- result mux

-- CPU registers
  signal IR:        std_logic_vector(31 downto 0);
  signal T:         std_logic_vector(31 downto 0);
  signal N:         std_logic_vector(31 downto 0);
  signal RP:        std_logic_vector(RAMsize-1 downto 0);
  signal SP:        std_logic_vector(RAMsize-1 downto 0);
  signal UP:        std_logic_vector(RAMsize-1 downto 0);
  signal PC:  	    std_logic_vector(23 downto 0);
  signal DebugReg:  std_logic_vector(31 downto 0);
  signal CARRY:     std_logic;
  signal slot:      integer range 0 to 5;

  type   IRstate_t is (prefetch, execute, user);
  signal IRstate: IRstate_t;

  type   RP_op_t is (RP_none, RP_up, RP_down);  -- pending RP operation
  signal RP_op: RP_op_t;

  type   N_src_t is (N_none, N_T, N_RAM);       -- N source
  signal N_src: N_src_t;

  type   PC_src_t is (PC_none, PC_RAM);         -- PC source
  signal PC_src: PC_src_t;

  signal operation: std_logic_vector(2 downto 0);
  signal modifier:  std_logic_vector(2 downto 0);
  signal RP_op:     std_logic_vector(1 downto 0);


  type   ca_select_t is (ca_pc);
  signal ca_select: ca_select_t;

  signal bbout:  std_logic_vector(7 downto 0);
  signal bbin:   std_logic_vector(3 downto 0);

  signal new_T:     std_logic;                  -- load new T
  signal new_N:     std_logic;                  -- load new N

---------------------------------------------------------------------------------
BEGIN

  function f_DUP (pointer : in std_logic_vector) is
  begin
    xaddr <= SP;  xen <= '1';
    new_N <= '1';  N_src <= N_RAM;
    SP_op <= SP_up;
  end function f_DUP;

main: process(clk)
begin
  if (rising_edge(clk)) then
    if (reset='1') then
      RP <= std_logic_vector(to_unsigned(64, 32));   PC <= (others=>'0');
      SP <= std_logic_vector(to_unsigned(32, 32));    T <= (others=>'0');
      UP <= std_logic_vector(to_unsigned(64, 32));    N <= (others=>'0');
      DebugReg <= (others=>'0');                  CARRY <= '0';
      state <= prefetch;  ca_select = ca_pc;      faddr <= (others=>'0');
      xen <= '0';   xwe <= '0';   xlanes <= x"0";
      bbout <= x"00";     new_T <= '0';
      kack <= '0';  ereq <= '0';  edata_o <= x"00";
      RP_op <= RP_none;   SP_op <= SP_none;     -- stack pointer operations
      modifier <= "000";  operation <= "000";   -- opcode operations
    else
      xen <= '0';                               -- RAM strobe
      case state is
      when prefetch =>          -- `caddr` is new: wait for data from ROM[PC].
        if (cready = '1') and (ca_select = ca_pc) then
          opcode <= cdata(31 downto 26);
          IR <= cdata(25 downto 0);
          state <= execute;
        end if;
      when execute =>
        operation <= opcode(2 downto 0);
        modifier  <= opcode(5 downto 3);
        RP_op <= RP_none;
        SP_op <= SP_none;
        new_T <= '0';
        new_N <= '0';
        xen <= '0';
        xwe <= '0';

-- |         | 0         | 1          | 2         | 3         | 4         | 5         | 6         | 7         |
-- |:-------:|:---------:|:----------:|:---------:|:---------:|:---------:|:---------:|:---------:|:---------:|
-- | **0**   | nop       | dup        | exit      | +         | user      | 0<        | r>        | 2/        |
-- | **1**   | ifc:      | 1+         | swap      |           |           | c!+       | c@+       | u2/       |
-- | **2**   | no:       |            | ifz:      | **jmp**   |           | w!+       | w@+       | and       |
-- | **3**   |           | **litx**   | >r        | **call**  |           | 0=        | w@        | xor       |
-- | **4**   | reptc     | 4+         | over      | c+        |           | !+        | @+        | 2\*       |
-- | **5**   | -rept     |            | rp        | drop      |           | rp!       | @         | 2\*c      |
-- | **6**   | -if:      |            | sp        | **@as**   |           | sp!       | c@        | port      |
-- | **7**   |           | **lit**    | up        | **!as**   |           | up!       | r@        | invert    |
-- |:-------:|:---------:|:----------:|:---------:|:---------:|:---------:|:---------:|:---------:|:---------:|
-- | *mux*   | *none*    | *T+offset* | *XP / N*  | *N +/- T* | *user*    | *0= / N*  | *mem bus* | *logic*   |
--
-- The opcode map is optimized for LUT4 implementation. opcode[2:0] selects T from a 7:1 mux (column).
-- opcode[5:3] selects the row within the column, sometimes with some decoding.
-- There are 2 or 3 levels of logic between registers and the T mux.

        case opcode(5 downto 3) is
        when "000" => --  nop  ifc:  no:   ??    reptc  -rept  ??    -if:
          -- no reads
        when "001" => --  dup  1+    2+    litx  4+     ??     ??    lit
          immdata <= "00000000000000000000000" & opcode(2 downto 0);
          case opcode(2 downto 0) is
          when "000" => new_N <= '1';    N_src <= N_T;
          when "001" | "010" | "100" =>  new_T <= '1';
          when "011" => new_T <= '1';  immdata <= immmask and IR;
          when "111" => new_T <= '1';  immdata <= immmask and IR;
                        new_N <= '1';    N_src <= N_T;
          when others =>
          end case;
        when "010" => --  exit swap  ifz:  >r    over   rp     sp    up
          new_T <= '1';
          case opcode(2 downto 0) is
          when "000" => xaddr <= RP;  xen <= '1';  PC_src <= PC_RAM;  RP_op <= RP_up;  new_T <= '0';
          when "001" => new_N <= '1';  N_src <= N_T;
          when "010" | "011" => xaddr <= SP;  xen <= '1';  new_N <= '1';  N_src <= N_RAM;  SP_op <= SP_up;
          when others => null;
          end case;
        when "011" => --  +    ??    jmp   call  c+     drop   @as   !as
          case opcode(2 downto 1) is
          when "00" | "10" => -- drop
            xaddr <= SP;  xen <= '1';  N_src <= N_RAM;  SP_op <= SP_up;
          when others =>
            immdata <= immmask and IR;
          end case;
          case opcode(2 downto 0) is
          when "000" | "100" | "101" => new_T <= '1';
          when others => null;
          end case;
        when "100" => --  user
        when "101" => --  0<   c!+   w!+   0=    !+     rp!    sp!   up!
          case opcode(2 downto 0) is
          when "001" | "010" | "100" | "101" | "110" | "111" =>
            xaddr <= SP;  xen <= '1';  N_src <= N_RAM;  SP_op <= SP_up;
          when others => null;
          end case;
        when "110" => --  r>   c@+   w@+   w@    @+     @      c@    r@
          case opcode(2 downto 0) is
          when "000" =>
            xaddr <= RP;  xen <= '1';  PC_src <= PC_RAM;  RP_op <= RP_up;
          when "111" =>
            xaddr <= RP;  xen <= '1';  PC_src <= PC_RAM;
          when others => null;
          end case;
        when others => -- 2/   u2/   and   xor   2*     2*c    port  invert
          case opcode(2 downto 0) is
          when "010" | "011" =>
            xaddr <= SP;  xen <= '1';  N_src <= PC_RAM;  SP_op <= SP_up;
          when others => null;
          end case;
-- execution pipeline stage
        case T_op is
        when T_zless => T <= (others=>T(31);
        when T_2div  =>
        when T_nop   => null;
        end case;
      when user =>
        state <= execute;
      end case;
    end if;
  end if;
end process main;

with modifier select r_logic <=
    T(31) & T(31 downto 1)       when "000", -- 2/
    '0'   & T(31 downto 1)       when "001", -- u2/
    T     and    N               when "010", -- and
    T     xor    N               when "011", -- xor
    T(30 downto 0) & '0'         when "100", -- 2*
    T(30 downto 0) & CARRY       when "101", -- 2*c
    DebugReg                     when "110", -- port
    not T                        when "111"; -- invert

zeroequals <= x"FFFFFFFF" when T=0 else x"00000000";
r_sum <= ('0' & T & (modifier(2) and CARRY)) + ('0' * N & '1');
immsum <= T + ("000000" & immdata);

with modifier select r_zeq <=
    (others => T(31))            when "000", -- 0<
    zeroequals                   when "011", -- 0=
    N                            when others;

with modifier select r_xpn <=
    xrdata                       when "100", -- over
    T + RP                       when "101", -- rp
    T + SP                       when "110", -- sp
    T + UP                       when "111", -- up
    N                            when others; -- swap, >r, ifz: exit

with slot select immmask <=
    "11111111111111111111111111" when 0,
    "00000011111111111111111111" when 1,
    "00000000000011111111111111" when 2,
    "00000000000000000011111111" when 3,
    "00000000000000000000000011" when others;

with operation select result <=
    r_xpn                        when "010", -- XP / N
    r_sum(32 downto 1)           when "011", -- adder
    r_user                       when "100", -- user
    r_zeq                        when "101", -- 0= / N
    xrdata                       when "110", -- mem bus
    r_logic                      when "111"; -- logic
    immsum                       when others; -- T + IMM

END RTL;
