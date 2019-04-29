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

-- intermediate mux results
  signal r_logic, r_user, immsum: unsigned(31 downto 0);
  signal r_zeq, zeroequals, r_xpn:  unsigned(31 downto 0);
  signal r_sum:              unsigned(33 downto 0);
  signal immdata:   		 unsigned(25 downto 0);
-- final result mux (to be registered in T)
  signal result:    	unsigned(31 downto 0);

-- CPU registers
  signal opcode:    	std_logic_vector(5 downto 0);
  signal IR, immmask: 	std_logic_vector(25 downto 0);
  signal T, N:        	unsigned(31 downto 0);
  signal RP, SP, UP:  	unsigned(RAMsize-1 downto 0);
  signal PC:  	      	unsigned(23 downto 0);
  signal DebugReg:  	unsigned(31 downto 0);
  signal CARRY:     	std_logic;
  signal slot, nextslot: integer range 0 to 5;

  type   state_t is (prefetch, execute, user);
  signal state: state_t;

  type   RP_op_t is (RP_none, RP_up, RP_down);  -- pending RP operation
  signal RP_op: RP_op_t;
  type   SP_op_t is (SP_none, SP_up, SP_down);  -- pending SP operation
  signal SP_op: SP_op_t;

  type   N_src_t is (N_none, N_T, N_RAM);       -- N source
  signal N_src: N_src_t;

  type   PC_src_t is (PC_none, PC_RAM);         -- PC source
  signal PC_src: PC_src_t;

  signal operation: std_logic_vector(2 downto 0);
  signal modifier:  std_logic_vector(2 downto 0);

  type   ca_select_t is (ca_pc);
  signal ca_select: ca_select_t;

  signal new_T:     std_logic;                  -- load new T
  signal new_N:     std_logic;                  -- load new N

---------------------------------------------------------------------------------
BEGIN

--  procedure f_DROP is -- start a DROP read
--  begin
--    xaddr <= SP;     xen <= '1';    xwe <= '0';
--    SP_op <= SP_up;  new_N <= '1';  N_src <= N_RAM;
--  end procedure f_DROP;  



main: process(clk)
begin
  if (rising_edge(clk)) then
    if (reset='1') then
      RP <= to_unsigned(64, RAMsize);   PC <= to_unsigned(0, 24);
      SP <= to_unsigned(32, RAMsize);    T <= to_unsigned(0, 32);
      UP <= to_unsigned(64, RAMsize);    N <= to_unsigned(0, 32);
      DebugReg <= (others=>'0');                  CARRY <= '0';
      state <= prefetch;  ca_select <= ca_pc;     caddr <= (others=>'0');
      xen <= '0';   xwe <= '0';   xlanes <= x"0";
      bbout <= x"00";     new_T <= '0';
      kack <= '0';  ereq <= '0';  edata_o <= x"00";
      RP_op <= RP_none;   SP_op <= SP_none;     -- stack pointer operations
      modifier <= "000";  operation <= "000";   -- opcode operations
	  slot <= 0;  nextslot <= 0;
    else
      xen <= '0';                               -- RAM strobe
      case state is
      when prefetch =>  -- `caddr` is new: wait for data from ROM[PC].
        if (cready = '1') and (ca_select = ca_pc) then
          opcode <= cdata(31 downto 26);
          IR <= cdata(25 downto 0);  slot <= 0;
          state <= execute;
        end if;
      when execute =>
        operation <= opcode(2 downto 0);
        modifier  <= opcode(5 downto 3);
        RP_op <= RP_none;  new_T <= '0';  xen <= '0';
        SP_op <= SP_none;  new_N <= '0';  xwe <= '0';
		slot <= slot + 1;
        case opcode(5 downto 3) is
        when "000" => --  nop    ifc:   no:    ??     reptc  -rept  ??     -if:
          case opcode(2 downto 0) is
          when "010" => slot <= 6;  
          when others => null;
          end case;
        when "001" => --  dup    1+     2+     litx   4+     ??     ??     lit
          immdata <= resize(unsigned(opcode(2 downto 0)), 26);
          case opcode(2 downto 0) is
          when "000" => new_N <= '1';    N_src <= N_T;
          when "001" | "010" | "100" =>  new_T <= '1';
          when "011" => new_T <= '1';    immdata <= unsigned(immmask and IR);
          when "111" => new_T <= '1';    immdata <= unsigned(immmask and IR);
                        new_N <= '1';    N_src <= N_T;
          when others =>
          end case;
        when "010" => --  exit   swap   ifz:   >r     over   rp     sp     up
          new_T <= '1';
          case opcode(2 downto 0) is
          when "000" => xaddr <= std_logic_vector(RP);  
		    xen <= '1';  PC_src <= PC_RAM;  RP_op <= RP_up;  new_T <= '0';
          when "001" => 
			new_N <= '1';  N_src <= N_T;
          when "010" | "011" => 
		    xaddr <= std_logic_vector(SP);  
			xen <= '1';  new_N <= '1';  N_src <= N_RAM;  SP_op <= SP_up;
          when others => null;
          end case;
        when "011" => --  +      ??     jmp    call   c+     drop   @as    !as
          case opcode(2 downto 1) is
          when "00" | "10" => -- drop
            xaddr <= std_logic_vector(SP);
			xen <= '1';  new_N <= '1';  N_src <= N_RAM;  SP_op <= SP_up;
          when others =>
            immdata <= unsigned(immmask and IR);
          end case;
          case opcode(2 downto 0) is
          when "000" | "100" | "101" => new_T <= '1';
          when others => null;
          end case;
        when "100" => --  user
        when "101" => --  0<     c!+    w!+    0=     !+     rp!    sp!    up!
          case opcode(2 downto 0) is
          when "001" | "010" | "100" | "101" | "110" | "111" =>
            xaddr <= std_logic_vector(SP);  
			xen <= '1';  N_src <= N_RAM;  SP_op <= SP_up;
          when others => null;
          end case;
        when "110" => --  r>     c@+    w@+    w@     @+     @      c@     r@
          case opcode(2 downto 0) is
          when "000" =>
            xaddr <= std_logic_vector(RP);  
			xen <= '1';  PC_src <= PC_RAM;  RP_op <= RP_up;
          when "111" =>
            xaddr <= std_logic_vector(RP);  
			xen <= '1';  PC_src <= PC_RAM;
          when others => null;
          end case;
        when others => -- 2/     u2/    and    xor    2*     2*c    port   invert
          case opcode(2 downto 0) is
          when "010" | "011" =>
            xaddr <= std_logic_vector(SP);  
			xen <= '1';  N_src <= N_RAM;  SP_op <= SP_up;
          when others => null;
          end case;
        end case;
-- execution pipeline stage
        if new_T = '1' then
		  T <= operation;
		end if;
        if new_N = '1' then
		  case N_src is
		  when N_RAM => N <= xrdata;
		  when others => null;
		  end if;
		end case;
-- next slot
		case slot is
		when 0      => opcode <= IR(25 downto 20);
		when 1      => opcode <= IR(19 downto 14);
		when 2      => opcode <= IR(13 downto 8);
		when 3      => opcode <= IR(7 downto 2);
		when others => opcode <= "0000" & IR(1 downto 0);
		end case;
		if unsigned(slot) > 4 then	-- last slot
		
		end if;
      when user =>
        state <= execute;
      end case;
    end if;
  end if;
end process main;

with modifier select r_logic <=
    unsigned(shift_right(signed(T),1)) when "000", -- 2/
    shift_right(T,1)  			 when "001", -- u2/
    T     and    N               when "010", -- and
    T     xor    N               when "011", -- xor
    T(30 downto 0) & '0'         when "100", -- 2*
    T(30 downto 0) & CARRY       when "101", -- 2*c
    DebugReg                     when "110", -- port
    not T                        when others; -- com

zeroequals <= x"FFFFFFFF" when T=to_unsigned(0,32) 
         else x"00000000";
r_sum <= ('0' & T & (modifier(2) and CARRY)) + ('0' & N & '1');
immsum <= T + ("000000" & immdata);

with modifier select r_zeq <=
    (others => T(31))            when "000", -- 0<
    zeroequals                   when "011", -- 0=
    N                            when others;

with modifier select r_xpn <=
    unsigned(xrdata)             when "100", -- over
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
    unsigned(xrdata)             when "110", -- mem bus
    r_logic                      when "111", -- logic
    immsum                       when others; -- T + IMM

END RTL;
