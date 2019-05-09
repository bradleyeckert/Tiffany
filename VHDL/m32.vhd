library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- M32 CPU core
-- ** version 0, does not have @as and !as **

ENTITY M32 IS
generic (
  options:  std_logic_vector(1 downto 0) := "00";   -- feature set
  RAMsize:  integer := 10                           -- log2 (RAM cells)
);
port (
  clk     : in  std_logic;                          -- System clock
  reset   : in  std_logic;                          -- Asynchronous reset
  -- Flash word-read
  caddr   : out std_logic_vector(25 downto 0);      -- Flash memory address
  cready  : in  std_logic;                          -- Flash memory data ready
  cdata   : in  std_logic_vector(31 downto 0);      -- Flash memory read data
  -- DPB: Dumb Peripheral Bus, compare to APB (Advanced Peripheral Bus)
  paddr   : out std_logic_vector(8 downto 0);       -- address
  pwrite  : out std_logic;                          -- write strobe
  psel    : out std_logic;                          -- start the cycle
  penable : out std_logic;                          -- delayed psel
  pwdata  : out std_logic_vector(15 downto 0);      -- write data
  prdata  : in  std_logic_vector(15 downto 0);      -- read data
  pready  : in  std_logic;                          -- ready to continue
  -- Powerdown
  bye     : out std_logic                           -- BYE encountered
);
END M32;

ARCHITECTURE RTL OF M32 IS

COMPONENT SPRAM32
generic (
  Size:  integer := 10                              -- log2 (cells)
);
port (
  clk:    in  std_logic;                            -- System clock
  reset:  in  std_logic;                            -- async reset
  en:     in  std_logic;                            -- Memory Enable
  we:     in  std_logic;                            -- Write Enable (0=read, 1=write)
  addr:   in  std_logic_vector(Size-1 downto 0);
  data_i: in  std_logic_vector(31 downto 0);        -- write data
  lanes:  in  std_logic_vector(3 downto 0);
  data_o: out std_logic_vector(31 downto 0)         -- read data
);
END COMPONENT;

-- Using single-port for RAM dings us a little on >R and R> and fetch/store.
-- But, it is half the size of DPRAM on an ASIC. Let's just use single-port.

  signal RAM_en:    std_logic;                      -- Memory Enable
  signal RAM_we:    std_logic;                      -- Write Enable (0=read, 1=write)
  signal RAM_addr:  std_logic_vector(RAMsize-1 downto 0);
  signal RAM_d:     std_logic_vector(31 downto 0);  -- write data
  signal RAMlanes:  std_logic_vector(3 downto 0);
  signal RAM_q:     std_logic_vector(31 downto 0);  -- read data

-- intermediate T mux results
  signal zeroequal: unsigned(31 downto 0);
  signal TNsum:     unsigned(33 downto 0);          -- T + N with carrt in and out

-- CPU registers
  signal opcode:    std_logic_vector(5 downto 0);
  signal IR:        std_logic_vector(31 downto 0);
  signal T, N:      unsigned(31 downto 0);
  signal RP,SP,UP:  unsigned(RAMsize-1 downto 0);
  signal PC:        unsigned(25 downto 0);
  signal DebugReg:  unsigned(31 downto 0);
  signal CARRY:     std_logic;
  signal slot:      integer range 0 to 7;

  type   state_t is (changed, stalled, execute, fetch, fetchc, fetchd, fetchsm,
                     userinit, user, pwait, dividing, divdone, multiplying);
  signal state: state_t;

  signal skip_if, skip_nc, skip_ge: std_logic;      -- skip type
  signal rept_mi, rept_nc: std_logic;               -- repeat type
  signal skipping, repeating: std_logic;
  signal noexecute: std_logic;                      -- don't execute

  signal new_T:     std_logic;                      -- load new T
  signal T_src:     std_logic_vector(3 downto 0);   -- T source
  signal carryin:   std_logic;                      -- adder or shifter gets a carry in
  signal immdata:   unsigned(31 downto 0);
  signal userdata:  unsigned(31 downto 0);          -- user states output this

  signal new_N:     std_logic;                      -- load new N
  signal N_src:     std_logic_vector(1 downto 0);   -- N source
  signal Noffset:   unsigned(2 downto 0);           -- offset for "11"

  type   PC_src_t is (PC_inc, PC_imm, PC_RAM);
  signal PC_src: PC_src_t;                          -- PC source

  type   Port_src_t is (Port_none, Port_T);
  signal Port_src: Port_src_t;                      -- DbgPort source

  type   WR_src_t is (WR_none, WR_T, WR_N, WR_PC);
  signal WR_src: WR_src_t;                          -- RAM write source
  type   WR_dest_t is (WR_miSP, WR_miRP, WR_aT);
  signal WR_dest: WR_dest_t;                        -- RAM write address
  signal WR_size, RD_size: std_logic_vector(1 downto 0); -- 1/2/4
  signal RD_align:  std_logic_vector(1 downto 0);   -- alignment
  signal Tpacked:   unsigned(31 downto 0);          -- packed read data from FSM

  signal RPinc, SPinc:  std_logic;                  -- post-increment stack pointers
  signal RPload, SPload, UPload: std_logic;         -- load from T

  signal userFNsel: unsigned(3 downto 0);           -- user function select
  signal cycles:    unsigned(41 downto 0);          -- hardware counter
  -- COUNTER increments at Fclk/1024.
  signal held:      std_logic;                      -- stay of execution
  signal error:     std_logic;                      -- signal an error
  signal errorcode: integer range 0 to 31;          -- complement of Forth "throw code"

-- read and writes share access to single-port RAM
  signal RAM_raddr: unsigned(RAMsize-1 downto 0);
  signal RAM_waddr: unsigned(RAMsize-1 downto 0);
  signal RAM_read:  std_logic;

-- options use extra registers which will be pruned if not used.
  signal counter:  integer range 0 to 31;
  signal xo, yo:   unsigned(31 downto 0);           -- math inputs
  signal divisor:  unsigned(31 downto 0);           -- register
  signal diffd:    unsigned(32 downto 0);           -- divider subtractor
  signal xom, yom: unsigned(15 downto 0);           -- multiplier inputs
  signal product:  unsigned(31 downto 0);           -- multiplier output
  signal mulsum:   unsigned(47 downto 0);           -- multiplier sum


-- signal name: string(1 to 5); -- show opcode name in the waveform viewer

---------------------------------------------------------------------------------
BEGIN

RAM_addr <= std_logic_vector(RAM_waddr) when RAM_we = '1'
       else std_logic_vector(RAM_raddr);
RAM_en <= RAM_we or RAM_read;

-- asynchronously decode the opcode to form a read-request signal.

--|   | 0       | 1     | 2      | 3     | 4      | 5        | 6     | 7     |
--|:-:|:-------:|:-----:|:------:|:-----:|:------:|:--------:|:-----:|:-----:|
--| 0 | nop     | dup   | exit   | +     | 2\*    | port     |       |       |
--| 1 | *no:*   | 1+    | r>     |       | 2\*c   | **user** | c@+   | c!+   |
--| 2 |         | rp    | r@     | and   | 2/     | **jmp**  | w@+   | w!+   |
--| 3 |         | sp    |        | xor   | u2/    | **call** | w@    | >r    |
--| 4 | *reptc* | 4+    |        | c+    | 0=     | **litx** | @+    | !+    |
--| 5 | *-rept* | up    |        |       | 0<     | **@as**  | @     | rp!   |
--| 6 | *-if:*  |       |        |       | invert | **!as**  | c@    | sp!   |
--| 7 | *ifc:*  | over  | *ifz:* | drop  | swap   | **lit**  |       | up!   |

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
    when o"02" | o"12" | o"22" =>  -- exit, r>, r@
      if RPinc = '1' then
        RAM_raddr <= RP + 1;
      else
        RAM_raddr <= RP;
      end if;
    when others => null;
    end case;
    case opcode(2 downto 0) is
    when "010" | "011" | "111" =>  -- columns 2, 3, 7
      RAM_read <= '1';
    when others => null;
    end case;
  when others => null;
  end case;
end process a_read;


main: process(clk, reset)

procedure p_sdup is -- write setup: mem[--SP]=N
begin
  WR_src <= WR_N;
  WR_dest <= WR_miSP;
  new_N <= '1';
end procedure p_sdup;

procedure p_rdup is -- write setup: mem[--RP]=?
begin
  WR_dest <= WR_miRP;
end procedure p_rdup;

-- No writes to stack space are pending.
-- Pops trigger an increment in the execute phase.

procedure p_rdrop is -- read mem[RP++]
begin
  RPinc <= '1';
end procedure p_rdrop;

procedure p_sdrop is -- read N = mem[SP++]
begin
  SPinc <= '1';
  new_N <= '1';  N_src <= "01";
end procedure p_sdrop;

begin
  if (reset='1') then
    RP <= to_unsigned(64, RAMsize);   PC <= to_unsigned(0, 26);   slot <= 0;
    SP <= to_unsigned(32, RAMsize);    T <= x"00000000";     carryin <= '0';
    UP <= to_unsigned(64, RAMsize);    N <= x"00000000";       CARRY <= '0';
    DebugReg <= (others=>'0');   Tpacked <= x"00000000";   state <= changed;
    cycles <= (others=>'0');           RAM_d <= x"00000000";   RAMlanes <= x"0";
    caddr <= (others=>'0');           IR <= x"00000000"; opcode <= "000000";
    RAM_we <= '0';              userdata <= x"00000000";   PC_src <= PC_inc;
    new_T <= '0';  T_src <= x"0";  immdata <= x"00000000";
    new_N <= '0';  N_src <= "00";  Noffset <= "000";   Port_src <= Port_none;
    bye <= '0';
    -- stack post-inc/dec strobes
    RPinc <= '0';  RPload <= '0';  WR_dest <= WR_miSP;
    SPinc <= '0';  SPload <= '0';  UPload <= '0';
    -- read/write control
    WR_size <= "00";  WR_src <= WR_none;  error <= '0';  errorcode <= 0;
    RD_size <= "00";  RD_align <= "00";   userFNsel <= x"0";
    skip_nc <= '0';  skip_if <= '0';  skip_ge <= '0';
    rept_mi <= '0';  rept_nc <= '0';  noexecute <= '0';  paddr <= (others=>'0');
    pwrite <= '0';  psel <= '0';  penable <= '0';  pwdata <= x"0000";
    -- math
    counter <= 0;  divisor <= x"00000000";  xo <= x"00000000";  yo <= x"00000000";
    xom <= x"0000";  yom <= x"0000";
  elsif (rising_edge(clk)) then
--------------------------------------------------------------------------------
    cycles <= cycles + 1;
    noexecute <= skipping;
-- execution pipeline stage
-- T: 16:1 mux
    if noexecute = '0' then
      if new_T = '1' then
        case T_src is
        when "0000" => T <= N;
        when "0001" => T <= T + immdata;
        when "0010" => T <= T and N;
        when "0011" => T <= T xor N;
        when "0100" => T <= TNsum(32 downto 1);                    CARRY <= TNsum(33);
        when "0101" => T <= zeroequal;
        when "0110" => T <= unsigned(RAM_q);
        when "0111" => T <= immdata;
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
        case WR_dest is
        when WR_miSP => RAM_waddr <= SP - 1;
          if SPinc = '0' then
            SP <= SP - 1;
          end if;
        when WR_miRP => RAM_waddr <= RP - 1;
          if RPinc = '0' then
            RP <= RP - 1;
          end if;
        when WR_aT =>
          RAM_waddr <= T(RAMsize+1 downto 2);
          if T(31) = '0' then
            error <= '1';  errorcode <= 19;   -- writing to ROM
          end if;
          errorcode <= 22;                    -- misaligned write
          case WR_size is
          when "00" => error <= T(0) or T(1);
          when "10" => error <= T(0);
          when others => null;
          end case;
        end case;
        case WR_src is
        when WR_T =>
          RAM_d <= std_logic_vector(T);
        when WR_N =>
          case WR_size is
          when "01" => -- byte
            case T(1 downto 0) is
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
            if T(1) = '0' then
              RAMlanes <= "0011";
              RAM_d <= std_logic_vector(N);
            else
              RAMlanes <= "1100";
              RAM_d <= std_logic_vector(N(15 downto 0) & N(15 downto 0));
            end if;
          when others =>
            RAM_d <= std_logic_vector(N);
          end case;
        when WR_PC => RAM_d <= "0000" & std_logic_vector(PC) & "00";
        when others => null;
        end case;
        WR_size <= "00";
      else
        RAM_we <= '0';
      end if;
-- RP:
      if RPinc = '1' then
        RP <= RP + 1;
      end if;
      if RPload = '1' then
        RP <= T(RAMsize+1 downto 2);
      end if;
-- SP:
      if SPinc = '1' then
        SP <= SP + 1;
      end if;
      if SPload = '1' then
        SP <= T(RAMsize+1 downto 2);
      end if;
-- UP:
      if UPload = '1' then
        UP <= T(RAMsize+1 downto 2);
      end if;
    end if;

    new_T <= '0';   T_src <= "0000";
    new_N <= '0';   N_src <= "00";
    Port_src <= Port_none;
    RPload <= '0';  RPinc <= '0';
    SPload <= '0';  SPinc <= '0';
    UPload <= '0';  WR_src <= WR_none;

--------------------------------------------------------------------------------
-- FSM
    case state is
    when changed =>  -- change in control flow
      case PC_src is
      when PC_imm =>  PC <= immdata(25 downto 0);
                   caddr <= std_logic_vector(immdata(25 downto 0));
      when PC_RAM =>  PC <= unsigned(RAM_q(27 downto 2));
                   caddr <= RAM_q(27 downto 2);
      when others => null;
      end case;
      PC_src <= PC_inc;
      skip_if <= '0';  skip_nc <= '0';  skip_ge <= '0';
      state <= stalled;
    when stalled =>  -- `caddr` is new: wait for data from ROM[PC].
      if cready = '1' then
        opcode <= cdata(31 downto 26);      -- grab the instruction group
        IR <= cdata;  slot <= 0;            -- save the whole IR for repeats
        PC    <= PC + 1;                    -- bump the PC
        caddr <= std_logic_vector(PC + 1);
        skip_if <= '0';  skip_nc <= '0';  skip_ge <= '0';
        if error = '1' then                 -- not so fast...
          state <= changed;  PC_src <= PC_imm;
          p_rdup;   WR_src <= WR_PC;
          immdata <= x"00000002";           -- error code is in port
          DebugReg <= not (to_unsigned(errorcode, 32));
        else
          state <= execute;
        end if;
        error <= '0';
      end if;
    when fetch =>  -- T is now available for use as a read address
      RD_align <= std_logic_vector(T(1 downto 0));
      if T(31) = '1' then
        if RD_size="00" then
          new_T <= '1';  T_src <= "0110";   -- cell from RAM, ready now
          if T(1 downto 0) /= "00" then
            error <= '1';  errorcode <= 22; -- misaligned read
          end if;
          state <= execute;
        else
          state <= fetchd;                  -- next state gets the unaligned data
        end if;
      else
        caddr <= std_logic_vector(T(27 downto 2));
        state <= fetchc;                    -- fetch from ROM space
      end if;
    when fetchc =>                          -- wait for ROM to return the data
      if cready = '1' then
        Tpacked <= unsigned(cdata);         -- register the input
        caddr <= std_logic_vector(PC);      -- restore code address bus
        case RD_size is
        when "01" | "10" => state <= fetchsm; -- some shift-and-mask is needed
        when others => state <= execute;    -- cell-sized fetch
          new_T <= '1';  T_src <= "1111";   -- T gets Tpacked
          if RD_align /= "00" then
            error <= '1';  errorcode <= 22;
          end if;
        end case;
      end if;
    when fetchd =>
      Tpacked <= unsigned(RAM_q);           -- non-cell RAM fetch needs alignment
      state <= fetchsm;
    when fetchsm =>                         -- Tpacked has byte or half data
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
        if RD_align(0) = '1' then
          error <= '1';  errorcode <= 22;
        end if;
      end if;
      state <= execute;  new_T <= '1';  T_src <= "1111";
    when userinit =>                        -- T is still indeterminate
      state <= user;  userFNsel <= immdata(3 downto 0);
    when user =>                            -- calculate userdata
      new_T <= '1';  T_src <= "1011";  userdata <= x"00000000";
      state <= stalled;                     -- default is single-cycle operation
      case userFNsel is
      when "0000" => pwrite <= T(16);       -- peripheral bus
        paddr <= std_logic_vector(T(25 downto 17));  psel <= '1';
        if T(16) = '1' then
          pwdata <= std_logic_vector(T(15 downto 0));
        end if;  state <= pwait;            -- bus cycle instead
        new_T <= '0';
      when "0001" => bye <= '1';
      when "0010" =>                        -- Counter
        userdata <= cycles(41 downto 10);
      when "0011" =>                        -- set divisor, get remainder
        if options(0) = '1' then
          divisor <= T;  userdata <= yo;
        end if;
      when "0100" =>
        if options(0) = '1' then
          if T >= divisor then              -- division overflow
            xo <= x"FFFFFFFF";  yo <= x"00000000";
          else
            state <= dividing;              -- start UM/MOD
            yo <= T;  xo <= N;  counter <= 31;  new_T <= '0';
          end if;
        end if;
      when "0101" =>
        if options(0) = '1' then
          state <= multiplying;             -- start UM*
          counter <= 5;  new_T <= '0';
          xom <= T(15 downto 0);  yom <= N(15 downto 0);
        end if;
      when others => null;
      end case;
    when multiplying =>                     -- with 16x16 hardware multiply
      product <= xom * yom;                 -- 16x16 multiplier with registered output
      case counter is
        when 5 =>                 xom <= T(31 downto 16);  yom <= N(31 downto 16);
        when 4 => xo <= product;  xom <= T(31 downto 16);  yom <= N(15 downto 0);
        when 3 => yo <= product;  xom <= T(15 downto 0);   yom <= N(31 downto 16);
        when 2 => yo <= mulsum(47 downto 16);  xo(31 downto 16) <= mulsum(15 downto 0);
        when others => yo <= mulsum(47 downto 16);
          userdata <= mulsum(15 downto 0) & xo(15 downto 0);
          new_T <= '1';  T_src <= "1011";  state <= stalled;
      end case;
      counter <= counter - 1;
    when dividing =>                        -- binary long division
      if (diffd(32)='0') or (yo(31)='1') then
        xo <= xo(30 downto 0) & '1';
        yo <= diffd(31 downto 0);
      else
        xo <= xo(30 downto 0) & '0';
        yo <= yo(30 downto 0) & xo(31);
      end if;
      if counter = 0 then
        state <= divdone;
      else
        counter <= counter - 1;
      end if;
    when divdone =>
      userdata <= xo;
      new_T <= '1';  T_src <= "1011";  state <= stalled;
    when pwait =>
      penable <= '1';
      if pready = '1' then                  -- DPB cycle is finished
        penable <= '1';  psel <= '0';
        new_T <= '1';  T_src <= "1011";  state <= stalled;
        userdata <= unsigned(x"0000" & prdata);
      end if;
    when execute =>
--------------------------------------------------------------------------------
-- decoding pipeline stage
      skip_if <= '0';  skip_nc <= '0';  skip_ge <= '0';
      rept_mi <= '0';  rept_nc <= '0';
      if held = '0' then -- waiting for RAM write to finish
-- strobes:
        RD_size <= "00";
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
          when 0      => immdata <= unsigned (x"03FFFFFF" and IR);
          when 1      => immdata <= unsigned (x"000FFFFF" and IR);
          when 2      => immdata <= unsigned (x"00003FFF" and IR);
          when 3      => immdata <= unsigned (x"000000FF" and IR);
          when others => immdata <= unsigned (x"00000003" and IR);
          end case;
        end if;
-- decode:
        carryin <= '0';

--|   | 0       | 1     | 2      | 3     | 4      | 5        | 6     | 7     |
--|:-:|:-------:|:-----:|:------:|:-----:|:------:|:--------:|:-----:|:-----:|
--| 0 | nop     | dup   | exit   | +     | 2\*    | port     |       |       |
--| 1 | *no:*   | 1+    | r>     |       | 2\*c   | **user** | c@+   | c!+   |
--| 2 |         | rp    | r@     | and   | 2/     | **jmp**  | w@+   | w!+   |
--| 3 |         | sp    |        | xor   | u2/    | **call** | w@    | >r    |
--| 4 | *reptc* | 4+    |        | c+    | 0=     | **litx** | @+    | !+    |
--| 5 | *-rept* | up    |        |       | 0<     | **@as**  | @     | rp!   |
--| 6 | *-if:*  |       |        |       | invert | **!as**  | c@    | sp!   |
--| 7 | *ifc:*  | over  | *ifz:* | drop  | swap   | **lit**  |       | up!   |

        case opcode is
        when o"01" => p_sdup;                                       -- dup
        when o"02" => p_rdrop;  state <= changed; PC_src <= PC_RAM; -- exit
        when o"03" => p_sdrop;  new_T <= '1';  T_src <= "0100";     -- +
        when o"04" => new_T <= '1';  T_src <= "1000";               -- 2*
        when o"05" => state <= stalled;                             -- unused
        when o"10" => state <= stalled;  -- no need to wait         -- no:
        when o"11" | o"41" => new_T <= '1';  T_src <= "0001";       -- 1+, 4+
                      immdata <= resize(unsigned(opcode(5 downto 3)), 32);
        when o"12" => p_rdrop;  p_sdup;                             -- r>
                      new_T <= '1'; T_src <= "0110";
        when o"14" => new_T <= '1'; T_src <= "1000"; carryin <= '1';-- 2*c
        when o"15" => state <= userinit;                            -- user
        when o"16" => p_sdup;  RD_size <= "01";  state <= fetch;    -- c@+
                      new_N <= '1';  N_src <= "11";  Noffset <= "001";
        when o"17" | o"27" | o"47" =>                       -- c!+, w!+, !+
            immdata <= resize(unsigned(opcode(5 downto 3)), 32);
            p_sdrop;                                        -- N = sdrop
            new_T <= '1';  T_src <= "0001";                 -- T = N+offset
            WR_src <= WR_N;  WR_size <= opcode(4 downto 3); -- mem[T] = N
            WR_dest <= WR_aT;
        when o"21" => new_T <= '1'; T_src <= "0001";                -- rp
                      immdata(RAMsize+1 downto 0) <= RP & "00";
                      immdata(31 downto RAMsize+2) <= (others => '1');
        when o"22" => p_rdrop;  p_sdup;  RPinc <= '0';              -- r@
                      new_T <= '1'; T_src <= "0110";
        when o"23" => p_sdrop;      T_src <= "0010";  new_T <= '1'; -- and
        when o"24" => new_T <= '1'; T_src <= "1010"; carryin <= '1';-- 2/
        when o"25" => state <= changed;  PC_src <= PC_imm;          -- jmp
        when o"26" => p_sdup;  RD_size <= "10";  state <= fetch;    -- w@+
                      new_N <= '1';  N_src <= "11";  Noffset <= "010";
        when o"31" => new_T <= '1'; T_src <= "0001";                -- sp
                      immdata(RAMsize+1 downto 0) <= SP & "00";
                      immdata(31 downto RAMsize+2) <= (others => '1');
        when o"33" => p_sdrop;      T_src <= "0011";  new_T <= '1'; -- xor
        when o"34" => new_T <= '1'; T_src <= "1010";                -- u2/
        when o"35" => state <= changed;  PC_src <= PC_imm;          -- call
                      p_rdup;   WR_src <= WR_PC;
        when o"36" => RD_size <= "10";  state <= fetch;             -- w@
        when o"37" => p_sdrop;  new_T <= '1';                       -- >r
                      p_rdup;   WR_src <= WR_T;
        when o"40" => rept_nc <= '1';  new_N <= '1'; N_src <= "10"; -- reptc
        when o"43" => p_sdrop;  new_T <= '1';  T_src <= "0100";     -- c+
                      carryin <= '1';
        when o"44" => new_T <= '1';  T_src <= "0101";               -- 0=
        when o"45" => new_T <= '1';  T_src <= "1100";               -- litx
                      state <= stalled;
        when o"46" => p_sdup;  RD_size <= "00";  state <= fetch;    -- @+
                      new_N <= '1';  N_src <= "11";  Noffset <= "100";
        when o"50" => rept_mi <= '1';  new_N <= '1';  N_src <= "10";-- -rept
        when o"51" => new_T <= '1';  T_src <= "0001";               -- up
                      immdata(RAMsize+1 downto 0) <= UP & "00";
                      immdata(31 downto RAMsize+2) <= (others => '1');
        when o"54" => new_T <= '1';  T_src <= "1001";               -- 0<
        when o"55" => state <= stalled;                             -- @as
        when o"56" => RD_size <= "00";  state <= fetch;             -- @
        when o"57" => p_sdrop;  RPload <= '1';                      -- rp!
        when o"60" => skip_ge <= '1';                               -- -if:
        when o"61" => new_T <= '1';  T_src <= "1101";               -- port
                      Port_src <= Port_T;
        when o"64" => new_T <= '1';  T_src <= "1110";               -- invert
        when o"65" => state <= stalled;                             -- !as
        when o"66" => RD_size <= "01";  state <= fetch;             -- c@
        when o"67" => p_sdrop;  SPload <= '1';                      -- sp!
        when o"70" => skip_nc <= '1';                               -- ifc:
        when o"71" => p_sdup;   new_T <= '1';                       -- over
        when o"72" => p_sdrop;  new_T <= '1';  skip_if <= '1';      -- ifz:
        when o"73" => p_sdrop;  new_T <= '1';                       -- drop
        when o"74" => new_N <= '1';  new_T <= '1';                  -- swap
        when o"75" => state <= stalled;  p_sdup;                    -- lit
                      new_T <= '1';  T_src <= "0111";
        when o"77" => p_sdrop;  UPload <= '1';                      -- up!
        when others => null;
        end case;
      end if;
      if repeating = '1' then -- note: NOP must follow -rept or reptc
        slot <= 0;  state <= execute;  opcode <= IR(31 downto 26);
      end if;
      if skipping = '1' then -- next opcode gets decoded but not executed
        state <= stalled;
      end if;
    end case;
  end if;
end process main;

process(skip_nc, skip_ge, skip_if, T, CARRY) is
begin
  if skip_if = '1' then
    if T = x"00000000" then
      skipping <= '0';
    else
      skipping <= '1';
    end if;
  elsif skip_ge = '1' then
    skipping <= not T(31);
  elsif skip_nc = '1' then
    skipping <= not CARRY;
  else
    skipping <= '0';
  end if;
end process;

process(rept_mi, rept_nc, N, CARRY) is
begin
  if rept_nc = '1' then
    repeating <= not CARRY;
  elsif rept_mi = '1' then
    repeating <= N(16);
  else
    repeating <= '0';
  end if;
end process;

zeroequal <= x"FFFFFFFF" when T=x"00000000" else x"00000000";
TNsum <= ('0' & T & (carryin and CARRY)) + ('0' & N & '1');

diffd <= ('0'&yo(30 downto 0)&xo(31)) - ('0'&divisor);  -- dividend difference
mulsum <= (yo & xo(31 downto 16)) + (x"0000" & product);

held <= opcode(1) and RAM_we when WR_src = WR_none
   else opcode(1);

spram: spram32
GENERIC MAP (Size => RAMsize)
PORT MAP (
  clk => clk,  reset => reset,
  en => RAM_en,  we => RAM_we,  addr => RAM_addr,
  data_i => RAM_d,  lanes => RAMlanes,  data_o => RAM_q
);

-- uncomment if you want to see the opcode name in the waveform viewer
-- process(opcode, state, held) begin
--     if (state = execute) and (held = '0') then
--       case opcode is
--       when "000000" => name <= ".    ";
--       when "000001" => name <= "dup  ";
--       when "000010" => name <= "exit ";
--       when "000011" => name <= "+    ";
--       when "000100" => name <= "2*   ";
--       when "001000" => name <= "no:  ";
--       when "001001" => name <= "1+   ";
--       when "001010" => name <= "r>   ";
--       when "001100" => name <= "2*c  ";
--       when "001101" => name <= "user ";
--       when "001110" => name <= "c@+  ";
--       when "001111" => name <= "c!+  ";
--       when "010001" => name <= "rp   ";
--       when "010010" => name <= "r@   ";
--       when "010011" => name <= "and  ";
--       when "010100" => name <= "2/   ";
--       when "010101" => name <= "jmp  ";
--       when "010110" => name <= "w@+  ";
--       when "010111" => name <= "w!+  ";
--       when "011001" => name <= "sp   ";
--       when "011011" => name <= "xor  ";
--       when "011100" => name <= "u2/  ";
--       when "011101" => name <= "call ";
--       when "011110" => name <= "w@   ";
--       when "011111" => name <= ">r   ";
--       when "100000" => name <= "reptc";
--       when "100001" => name <= "4+   ";
--       when "100011" => name <= "c+   ";
--       when "100100" => name <= "0=   ";
--       when "100101" => name <= "litx ";
--       when "100110" => name <= "@+   ";
--       when "100111" => name <= "!+   ";
--       when "101000" => name <= "-rept";
--       when "101001" => name <= "up   ";
--       when "101100" => name <= "0<   ";
--       when "101101" => name <= "@as  ";
--       when "101110" => name <= "@    ";
--       when "101111" => name <= "rp!  ";
--       when "110000" => name <= "-if: ";
--       when "110001" => name <= "port ";
--       when "110100" => name <= "com  ";
--       when "110101" => name <= "!as  ";
--       when "110110" => name <= "c@   ";
--       when "110111" => name <= "sp!  ";
--       when "111000" => name <= "ifc: ";
--       when "111001" => name <= "over ";
--       when "111010" => name <= "ifz: ";
--       when "111011" => name <= "drop ";
--       when "111100" => name <= "swap ";
--       when "111101" => name <= "lit  ";
--       when "111111" => name <= "up!  ";
--       when others   => name <= "?????";
--       end case;
--     else
--       name <= "-----";
--     end if;
-- end process;

END RTL;
