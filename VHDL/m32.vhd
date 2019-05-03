library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- M32 CPU core
-- ** PRELIMINARY work in progress **

ENTITY M32 IS
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
  bye     : out std_logic			            -- BYE encountered
);
END M32;

ARCHITECTURE RTL OF M32 IS

COMPONENT SPRAM32
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
END COMPONENT;

-- Single-port for RAM dings us a little on >R and R> and fetch/store.
-- But, is smaller on an ASIC. Let's just use single-port.

  signal RAM_en:    std_logic;                     -- Memory Enable
  signal RAM_we:    std_logic;                     -- Write Enable (0=read, 1=write)
  signal RAM_addr:  std_logic_vector(RAMsize-1 downto 0);
  signal RAM_d:     std_logic_vector(31 downto 0); -- write data
  signal RAMlanes:  std_logic_vector(3 downto 0);
  signal RAM_q:     std_logic_vector(31 downto 0); -- read data

-- intermediate mux results
  signal zeroequals: 	unsigned(31 downto 0);
  signal TNsum:         unsigned(33 downto 0);

-- CPU registers
  signal opcode:    	std_logic_vector(5 downto 0);
  signal IR: 	        std_logic_vector(31 downto 0);
  signal T, N:        	unsigned(31 downto 0);
  signal RP, SP, UP:  	unsigned(RAMsize-1 downto 0);
  signal PC:  	      	unsigned(25 downto 0);
  signal DebugReg:  	unsigned(31 downto 0);
  signal CARRY:     	std_logic;
  signal slot: 			integer range 0 to 7;

  type   state_t is (changed, stalled, execute, fetch, fetchc, fetchd, fetchsm, userinit, user);
  signal state: state_t;

  type   skip_t is (skip_none, skip_nc, skip_ge, skip_if);
  signal skip: skip_t;                          -- skip type
  type   rept_t is (rept_none, rept_nc, rept_n);
  signal rept: rept_t;                          -- repeat type
  signal skipping:      std_logic;              -- don't execute

  signal new_T:         std_logic;              -- load new T
  signal T_src:  std_logic_vector(3 downto 0);  -- T source
  signal carryin:       std_logic;
  signal immdata:   	unsigned(25 downto 0);
  signal userdata:   	unsigned(31 downto 0);

  signal new_N:         std_logic;              -- load new N
  signal N_src:  std_logic_vector(1 downto 0);  -- N source
  signal Noffset:       unsigned(2 downto 0);	-- offset for "11"

  type   PC_src_t is (PC_inc, PC_imm, PC_RAM);
  signal PC_src: PC_src_t;                      -- PC source

  type   Port_src_t is (Port_none, Port_T);
  signal Port_src: Port_src_t;                  -- DbgPort source

  type   WR_src_t is (WR_none, WR_T, WR_N, WR_PC);
  signal WR_src: WR_src_t;                      -- write source
  signal WR_addr:  	    unsigned(RAMsize-1 downto 0);
  signal WR_size, RD_size:   std_logic_vector(1 downto 0); -- 1/2/4
  signal WR_align, RD_align: std_logic_vector(1 downto 0); -- alignment
  signal Tpacked:   	unsigned(31 downto 0);	-- packed read data

  signal RPinc, SPinc:  std_logic;              -- post-increment
  signal RPdec, SPdec:  std_logic;              -- post-decrement
  signal RPload, SPload, UPload: std_logic;     -- load from T

  signal userFNsel:     unsigned(3 downto 0);   -- user function select
  signal cycles:   	    unsigned(35 downto 0);  -- 16-cycle resolution counter
  -- 2^36/100M = 687 seconds (10 minutes)
  signal ereq_i:        std_logic;
  signal row:           std_logic_vector(2 downto 0);

-- read and writes share access to single-port RAM
  signal RAM_raddr:     unsigned(RAMsize-1 downto 0);
  signal RAM_waddr:     unsigned(RAMsize-1 downto 0);
  signal RAM_read:      std_logic;

---------------------------------------------------------------------------------
BEGIN

RAM_addr <= std_logic_vector(RAM_waddr) when RAM_we = '1'
       else std_logic_vector(RAM_raddr);
RAM_en <= RAM_we or RAM_read;

-- asynchronously decode the opcode to form a read-request signal.

--|   | 0        | 1        | 2        | 3        | 4        | 5        | 6        | 7        |
--|:-:|:--------:|:--------:|:--------:|:--------:|:--------:|:--------:|:--------:|:--------:|
--| 0 | nop      | dup      | exit     | +        | 2\*      | port     |          |          |
--| 1 | *no:*    | 1+       | r>       |          | 2\*c     | **user** | c@+      | c!+      |
--| 2 |          | rp       | r@       | and      | 2/       | **jmp**  | w@+      | w!+      |
--| 3 |          | sp       |          | xor      | u2/      | **call** | w@       | >r       |
--| 4 | *reptc*  | 4+       |          | c+       | 0=       | **litx** | @+       | !+       |
--| 5 | *-rept*  | up       |          |          | 0<       | **@as**  | @        | rp!      |
--| 6 | *-if:*   |          |          |          | invert   | **!as**  | c@       | sp!      |
--| 7 | *ifc:*   | over     | *ifz:*   | drop     | swap     | **lit**  |          | up!      |

a_read: process(state, opcode, SPinc, RPinc, SP, RP, T)
begin
  if SPinc = '1' then
    RAM_raddr <= SP + 1;
  else
    RAM_raddr <= SP;
  end if;
  RAM_read <= '0';
  case state is
  when fetch =>
    if T(31) = '1' then
      RAM_read <= '1';
      RAM_raddr <= T(RAMsize+1 downto 2);
    end if;
  when execute =>
    case opcode is
    when "000010" | "001010" | "010010" => -- exit, r>, r@
      if RPinc = '1' then
        RAM_raddr <= RP + 1;
      else
        RAM_raddr <= RP;
      end if;
    when others => null;
    end case;
    case opcode(2 downto 0) is
    when "010" | "011" | "111" => -- columns 2, 3, 7
      RAM_read <= '1';
    when others => null;
    end case;
  when others => null;
  end case;
end process a_read;


main: process(clk)

procedure p_sdup is -- write setup: mem[--SP]=N
begin
  WR_src <= WR_N;
  if SPinc = '1' then
    WR_addr <= SP;
  else
    WR_addr <= SP - 1;
    SPdec <= '1';
  end if;
  new_N <= '1';
end procedure p_sdup;

procedure p_rdup is -- write setup: mem[--RP]=?
begin
  if RPinc = '1' then
    WR_addr <= RP;
  else
    WR_addr <= RP - 1;
    RPdec <= '1';
  end if;
end procedure p_rdup;

procedure p_rdrop is -- read mem[RP++], could be T or PC
begin
  if RPinc='1' then
    RAM_addr <= std_logic_vector(RP + 1);
  else
    RAM_addr <= std_logic_vector(RP);
  end if;
  RAM_en <= '1';  RAM_we <= '0';  RAMlanes <= x"F";
  RPinc <= '1';
end procedure p_rdrop;

procedure p_sdrop is -- read N = mem[SP++]
begin
  if SPinc='1' then
    RAM_addr <= std_logic_vector(SP + 1);
  else
    RAM_addr <= std_logic_vector(SP);
  end if;
  RAM_en <= '1';  RAM_we <= '0';  RAMlanes <= x"F";
  SPinc <= '1';
  new_N <= '1';  N_src <= "01";
end procedure p_sdrop;

begin
  if (rising_edge(clk)) then
    if (reset='1') then
      RP <= to_unsigned(64, RAMsize);     PC <= to_unsigned(0, 26);
      SP <= to_unsigned(32, RAMsize);      T <= x"00000000";
      UP <= to_unsigned(64, RAMsize);      N <= x"00000000";
      DebugReg <= (others=>'0');     Tpacked <= x"00000000";
      CARRY <= '0';  carryin <= '0';  cycles <= (others=>'0');
      state <= changed;   slot <= 0;   caddr <= (others=>'0');
      RAM_en <= '0';  RAMlanes <= x"0";   RAM_d <= x"00000000"; -- RAM
      RAM_we <= '0';  PC_src <= PC_inc;  RAM_addr <= (others=>'0');
      new_T <= '0';  T_src <= x"0";  immdata <= (others=>'0');
      new_N <= '0';  N_src <= "00";  Noffset <= "000";
      Port_src <= Port_none;        userdata <= x"00000000";
      opcode <= "000000";                 IR <= (others=>'0');
      kack <= '0';  ereq_i <= '0';   edata_o <= x"00";  bye <= '0';
      -- stack post-inc/dec strobes
      RPinc <= '0';  RPdec <= '0';  RPload <= '0';
      SPinc <= '0';  SPdec <= '0';  SPload <= '0';  UPload <= '0';
      -- read/write control
      WR_src <= WR_none;  WR_addr <= to_unsigned(0, RAMsize);
      WR_size <= "00";  WR_align <= "00";  bbout <= x"00";
      RD_size <= "00";  RD_align <= "00";  userFNsel <= x"0";
      skip <= skip_none;  rept <= rept_none;
    else
--------------------------------------------------------------------------------
      cycles <= cycles + 1;
      if eack = '1' then
        ereq_i <= '0';
      end if;
-- execution pipeline stage
-- T: 16:1 mux
      if new_T = '1' then
        case T_src is
        when "0000" => T <= N;
        when "0001" => T <= T + resize(immdata, 32);
        when "0010" => T <= T and N;
        when "0011" => T <= T xor N;
        when "0100" => T <= TNsum(32 downto 1);                    CARRY <= TNsum(33);
        when "0101" => T <= zeroequals;
        when "0110" => T <= unsigned(RAM_q);
        when "0111" => T <=   resize(immdata, 32);
        when "1000" => T <= T(30 downto 0) & (carryin and CARRY);  CARRY <= T(31);
        when "1001" => T <= (others => T(31));
        when "1010" => T <= (carryin and T(31)) & T(31 downto 1);  CARRY <= T(0);
        when "1011" => T <= userdata;
        when "1100" => T <= T(7 downto 0) & immdata(23 downto 0);
        when "1101" => T <= DebugReg;
        when "1110" => T <= not T;
        when others => T <= unsigned(Tpacked);
        end case;
      end if;
-- N: 4:1 mux
      if new_N = '1' then
        case N_src is
        when "00"   => N <= T;
        when "01"   => N <= unsigned(RAM_q);
        when "10"   => N <= N + 1;
        when others => N <= T + Noffset;
        end case;
      end if;
-- port:
      case Port_src is
      when Port_T => DebugReg <= T;
      when others => null;
      end case;
-- write:
      if WR_src /= WR_none then
        RAM_we <= '1';  RAMlanes <= x"F";
        RAM_waddr <= WR_addr;
        case WR_src is
        when WR_T =>
          RAM_d <= std_logic_vector(T);
        when WR_N =>
          case WR_size is
          when "01" => -- byte
            case WR_align is
            when "00" =>   RAMlanes <= "0001";
              RAM_d <= std_logic_vector(N);
            when "01" =>   RAMlanes <= "0010";
              RAM_d <= std_logic_vector(N(23 downto 0) & N(7 downto 0));
            when "10" =>   RAMlanes <= "0100";
              RAM_d <= std_logic_vector(N(15 downto 0) & N(15 downto 0));
            when others => RAMlanes <= "1000";
              RAM_d <= std_logic_vector(N(7 downto 0) & N(23 downto 0));
            end case;
          when "10" => -- half
            if WR_align(1) = '0' then
              RAMlanes <= "0011";
              RAM_d <= std_logic_vector(N);
            else
              RAMlanes <= "1100";
              RAM_d <= std_logic_vector(N(15 downto 0) & N(15 downto 0));
            end if;
          when others =>
            RAM_d <= std_logic_vector(N);
          end case;
        when WR_PC => RAM_d <= std_logic_vector(resize(PC, 32));
        when others => null;
        end case;
      else
        RAM_en <= '0';  RAM_we <= '0';
      end if;
-- RP:
      if RPinc = '1' then
        if RPdec = '0' then
          RP <= RP + 1;
        end if;
      else
        if RPdec = '1' then
          RP <= RP - 1;
        end if;
      end if;
      if RPload = '1' then                      -- override
        RP <= T(RAMsize-1 downto 0);
      end if;
-- SP:
      if SPinc = '1' then
        if SPdec = '0' then
          SP <= SP + 1;
        end if;
      else
        if SPdec = '1' then
          SP <= SP - 1;
        end if;
      end if;
      if SPload = '1' then                      -- override
        SP <= T(RAMsize-1 downto 0);
      end if;
-- UP:
      if UPload = '1' then
        UP <= T(RAMsize-1 downto 0);
      end if;

      new_T <= '0';   T_src <= "0000";
      new_N <= '0';   N_src <= "00";
      Port_src <= Port_none;
      RPload <= '0';  RPinc <= '0';  RPdec <= '0';
      SPload <= '0';  SPinc <= '0';  SPdec <= '0';
      UPload <= '0';  WR_src <= WR_none;

--------------------------------------------------------------------------------
-- FSM
      case state is
      when changed =>  -- change in control flow
        case PC_src is
        when PC_imm =>  PC <= immdata;
                     caddr <= std_logic_vector(immdata);
        when PC_RAM =>  PC <= unsigned(RAM_q(25 downto 0));
                     caddr <= RAM_q(25 downto 0);
        when others => null;
        end case;
        state <= stalled;
      when stalled =>  -- `caddr` is new: wait for data from ROM[PC].
        if cready = '1' then
          opcode <= cdata(31 downto 26);        -- grab the instruction group
          IR <= cdata;  slot <= 0;
          PC    <= PC + 1;                      -- bump the PC
          caddr <= std_logic_vector(PC + 1);
          state <= execute;
        end if;
      when fetch =>  -- T is now available for use as a read address
        if T(31) = '1' then
          if RD_size="00" then
            new_T <= '1';  T_src <= "0110";     -- cell
            state <= execute;
          else
            state <= fetchd;                    -- next state gets the unaligned data
          end if;
        else
        end if;
      when fetchc =>  -- fetchc is in progress with RD_align byte position, RD_size=0,1,2 cell,byte,half
        if cready = '1' then
          Tpacked <= unsigned(cdata);			-- register input from the outside world
          case RD_size is
          when "01" | "10" => state <= fetchsm;	-- some shift-and-mask is needed
          when others => state <= execute;		-- cell-sized fetch
            new_T <= '1';  T_src <= "1111";
          end case;
        end if;
      when fetchd =>
        Tpacked <= unsigned(RAM_q);			-- register input from the outside world
        state <= fetchsm;
      when fetchsm =>  							-- Tpacked has byte or half data
        if RD_size(0) = '1' then
          case RD_align is
          when "00"   => Tpacked <= x"000000" & Tpacked(7 downto 0);
          when "01"   => Tpacked <= x"000000" & Tpacked(15 downto 8);
          when "10"   => Tpacked <= x"000000" & Tpacked(23 downto 16);
          when others => Tpacked <= x"000000" & Tpacked(31 downto 24);
          end case;
        else
          if RD_align(1) = '1' then
            Tpacked <= x"0000" & Tpacked(31 downto 16);
          else
            Tpacked <= x"0000" & Tpacked(15 downto 0);
          end if;
        end if;
        state <= execute;  new_T <= '1';  T_src <= "1111";
      when userinit =>                          -- T is still indeterminate
        state <= user;  userFNsel <= immdata(3 downto 0);
      when user =>                              -- calculate userdata
        new_T <= '1';  T_src <= "1011";  userdata <= x"00000000";
        state <= execute;  kack <= '0';         -- default is single-cycle operation
        case userFNsel is
        when "0000" => userdata(0) <= kreq;                             -- vmQkey
        when "0001" => userdata(7 downto 0) <= unsigned(kdata_i);       -- vmKey
          kack <= '1';
        when "0010" => edata_o <= std_logic_vector(T(7 downto 0));      -- vmEmit
          ereq_i <= '1';
        when "0011" => userdata(0) <= not ereq_i;                       -- vmQemit
        when "0100" => userdata <= cycles(35 downto 4);                 -- Counter
        when "0101" =>                           -- bit-bang version of SPIflashXfer
          userdata(0) <= bbin(to_integer(T(2 downto 0)));               -- read bbin
          if T(3) = '1' then
            bbout(to_integer(T(2 downto 0))) <= N(0);                   -- write bbout
          end if;
        when "0110" => bye <= '1';
        when "0111" => userdata(2 downto 0) <= "101";
        when others => null;
        end case;
      when execute =>
--------------------------------------------------------------------------------
-- decoding pipeline stage
-- 2,3,6,7 = 010 011 110 111, if bit 1 is set wait for write to finish
        if skipping = '1' then
          state <= stalled;
        else
          if (opcode(1) = '0') or ((WR_src = WR_none) and (RAM_we = '0')) then
-- strobes:
            WR_size <= "00";  RD_size <= "00";
-- slot:
            slot <= slot + 1;
            if slot = 5 then
              state <= stalled;
            else
              case slot is
              when 0      => opcode <= IR(25 downto 20);
              when 1      => opcode <= IR(19 downto 14);
              when 2      => opcode <= IR(13 downto 8);
              when 3      => opcode <= IR(7 downto 2);
              when others => opcode <= "0000" & IR(1 downto 0);
              end case;
-- default immdata:
              case slot is
              when 0      => immdata <= unsigned ("11111111111111111111111111" and IR(25 downto 0));
              when 1      => immdata <= unsigned ("00000011111111111111111111" and IR(25 downto 0));
              when 2      => immdata <= unsigned ("00000000000011111111111111" and IR(25 downto 0));
              when 3      => immdata <= unsigned ("00000000000000000011111111" and IR(25 downto 0));
              when others => immdata <= unsigned ("00000000000000000000000011" and IR(25 downto 0));
              end case;
            end if;
-- decode:
            rept <= rept_none;
            skip <= skip_none;
            carryin <= '0';

--|   | 0        | 1        | 2        | 3        | 4        | 5        | 6        | 7        |
--|:-:|:--------:|:--------:|:--------:|:--------:|:--------:|:--------:|:--------:|:--------:|
--| 0 | nop      | dup      | exit     | +        | 2\*      | port     |          |          |
--| 1 | *no:*    | 1+       | r>       |          | 2\*c     | **user** | c@+      | c!+      |
--| 2 |          | rp       | r@       | and      | 2/       | **jmp**  | w@+      | w!+      |
--| 3 |          | sp       |          | xor      | u2/      | **call** | w@       | >r       |
--| 4 | *reptc*  | 4+       |          | c+       | 0=       | **litx** | @+       | !+       |
--| 5 | *-rept*  | up       |          |          | 0<       | **@as**  | @        | rp!      |
--| 6 | *-if:*   |          |          |          | invert   | **!as**  | c@       | sp!      |
--| 7 | *ifc:*   | over     | *ifz:*   | drop     | swap     | **lit**  |          | up!      |

            case opcode is
            when "000001" => p_sdup;                                        -- dup
            when "000010" => p_rdrop;  PC_src <= PC_RAM;                    -- exit
            when "000011" => p_sdrop;  new_T <= '1';  T_src <= "0100";      -- +
            when "000100" => new_T <= '1';  T_src <= "1000";                -- 2*
            when "000101" => new_T <= '1';  T_src <= "1101";                -- port
                             Port_src <= Port_T;
            when "001000" => state <= stalled;  -- no need to wait          -- no:
            when "001001" | "100001" => new_T <= '1';  T_src <= "0001";     -- 1+, 4+
                             immdata <= resize(unsigned(opcode(5 downto 3)), 26);
            when "001010" => p_rdrop;  p_sdup;  new_T <= '1';               -- r>
            when "001100" => new_T <= '1'; T_src <= "1000"; carryin <= '1'; -- 2*c
            when "001101" => state <= userinit;                				-- user
            when "001110" => p_sdup;  RD_size <= "01";  state <= fetch;     -- c@+
                new_N <= '1';  N_src <= "11";  Noffset <= "001";
            when "001111" | "010111" | "100111" =>                          -- c!+, w!+, !+
                -- T = N+offset | mem[T] = N | N = sdrop
                p_sdrop; immdata <= resize(unsigned(opcode(5 downto 3)), 26);
                new_T <= '1';  T_src <= "0001";
                WR_src <= WR_N;  WR_size <= opcode(1 downto 0);
                WR_addr <= T(RAMsize+1 downto 2);
                WR_align <= std_logic_vector(T(1 downto 0));
            when "010001" => new_T <= '1';             T_src <= "0001";     -- rp
                             immdata <= resize(unsigned(RP), 26);
            when "010010" => p_rdrop;  p_sdup;  new_T <= '1'; RPinc <= '0'; -- r@
            when "010011" => p_sdrop;       new_T <= '1';  T_src <= "0010"; -- and
            when "010100" => new_T <= '1';  T_src <= "1010";                -- 2/
            when "010101" => state <= changed;  PC_src <= PC_imm;           -- jmp
            when "010110" => p_sdup;  RD_size <= "10";  state <= fetch;     -- w@+
                             new_N <= '1';  N_src <= "11";  Noffset <= "010";
            when "011001" => new_T <= '1';             T_src <= "0001";     -- sp
                             immdata <= resize(unsigned(SP), 26);
            when "011011" => p_sdrop;       new_T <= '1';  T_src <= "0011"; -- xor
            when "011100" => new_T <= '1'; T_src <= "1010"; carryin <= '1'; -- u2/
            when "011101" => state <= changed;  PC_src <= PC_imm;           -- call
                             p_rdup;   WR_src <= WR_PC;
            when "011110" => RD_size <= "10";  state <= fetch;              -- w@
            when "011111" => p_sdrop;  new_T <= '1';                    	-- >r
                             p_rdup;   WR_src <= WR_T;  -- N=mem[SP++]; T=N; mem[--RP]=T;
            when "100000" => rept <= rept_nc;  new_N <= '1'; N_src <= "10"; -- reptc
            when "100011" => p_sdrop;  new_T <= '1';  T_src <= "0100";      -- c+
                             carryin <= '1';
            when "100100" => new_T <= '1';  T_src <= "1001";                -- 0<
            when "100101" => new_T <= '1';  T_src <= "1100";                -- litx
            when "100110" => p_sdup;  RD_size <= "00";  state <= fetch;     -- @+
                             new_N <= '1';  N_src <= "11";  Noffset <= "100";
            when "101000" => rept <= rept_n;  new_N <= '1';  N_src <= "10"; -- -rept
            when "101001" => new_T <= '1';  T_src <= "0001";                -- up
                             immdata <= resize(unsigned(UP), 26);
            when "101100" => new_T <= '1';  T_src <= "0101";                -- 0=
            ---- "101101" => state <= stalled;							    -- @as
            when "101110" => RD_size <= "00";  state <= fetch;              -- @
            when "101111" => p_sdrop;  RPload <= '1'; 		                -- rp!
            when "110000" => skip <= skip_ge;                               -- -if:
            when "110100" => new_T <= '1';  T_src <= "1110";                -- invert
            ---- "110101" => state <= stalled;								-- !as
            when "110110" => RD_size <= "01";  state <= fetch;              -- c@
            when "110111" => p_sdrop;  SPload <= '1'; 		                -- sp!
            when "111000" => skip <= skip_nc;                               -- ifc:
            when "111001" => p_sdup;   new_T <= '1';                        -- over
            when "111010" => p_sdrop;  new_T <= '1';  skip <= skip_if;      -- ifz:
            when "111011" => p_sdrop;                                       -- drop
            when "111100" => new_N <= '1';  new_T <= '1';                   -- swap
            when "111101" => state <= stalled;  p_sdup;                     -- lit
                             new_T <= '1';  T_src <= "0111";
            when "111111" => p_sdrop;  UPload <= '1'; 		                -- up!
            when others => null;
            end case;
-- repeats:
            case rept is                          -- able to override slot
            when rept_n  =>
              if N(16)='0' then
                slot <= 0;  state <= execute;  opcode <= IR(31 downto 26);
              end if;
            when rept_nc  =>
              if CARRY='0' then
                slot <= 0;  state <= execute;  opcode <= IR(31 downto 26);
              end if;
            when others => null;
            end case;
-- skips:
            case skip is                          -- conditional skips
            when skip_if  =>
              if T /= x"00000000" then
                state <= stalled;
              end if;
            when skip_nc  =>
              if CARRY='0' then
                state <= stalled;
              end if;
            when skip_ge  =>
              if T(31)='0' then
                state <= stalled;
              end if;
            when others => null;
            end case;
          end if;
        end if;
      end case;
    end if;
  end if;
end process main;

process(skip, T, CARRY) is
begin
  skipping <= '0';
  case skip is                                  -- conditional skips
  when skip_if  =>
    if T /= x"00000000" then
      skipping <= '1';
    end if;
  when skip_nc  =>
    if CARRY='0' then
      skipping <= '1';
    end if;
  when skip_ge  =>
    if T(31)='0' then
      skipping <= '1';
    end if;
  when others => null;
  end case;
end process;


zeroequals <= x"FFFFFFFF" when T=to_unsigned(0,32) else x"00000000";
TNsum <= ('0' & T & (carryin and CARRY)) + ('0' & N & '1');
ereq <= ereq_i;
row <= opcode(5 downto 3);

spram: spram32
GENERIC MAP (Size => RAMsize)
PORT MAP (
  clk => clk,  reset => reset,
  en => RAM_en,  we => RAM_we,  addr => RAM_addr,
  data_i => RAM_d,  lanes => RAMlanes,  data_o => RAM_q
);

END RTL;
