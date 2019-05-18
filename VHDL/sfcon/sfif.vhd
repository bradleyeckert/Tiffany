library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- Serial NOR Flash interface
-- QDR not tested

-- Supports 2^30 32-bit words, or 2^32 (4G) bits. BTW, 1G parts are under $10.
-- Parts beyond 128Mb use 4-byte read operations for the upper address range.
-- Supports single-rate and quad-rate.

-- Upon POR, the RAM is loaded from flash.
-- RAMsize is log2 of the number of 32-bit RAM words.

-- BaseAddr is the physical address of the first 64KB sector. It allows the
-- bottom of flash to contain the FPGA bitstream.

entity sfif is
generic (
  RAMsize:   integer := 10;                     -- log2 (RAM cells)
  CacheSize: integer := 4;
  BaseBlock: unsigned(7 downto 0) := x"00"
);
port (clk:   in std_logic;
  reset:     in std_logic;                      -- reset is async
-- ROM interface
  caddr:     in std_logic_vector(29 downto 0);
  cdata:    out std_logic_vector(31 downto 0);
  cready:   out std_logic;
-- Configuration
  config:    in std_logic_vector(7 downto 0);   -- HW configuration
-- SPIxfer
  xdata_i:   in std_logic_vector(9 downto 0);
  xdata_o:  out std_logic_vector(7 downto 0);
  xtrig:     in std_logic;
  xbusy:    out std_logic;
-- 6-wire SPI flash connection
  NCS:      out std_logic;                      -- chip select
  SCLK:     out std_logic;                      -- clock
  data_o:   out std_logic_vector(3 downto 0);
  drive_o:  out std_logic_vector(3 downto 0);
  data_i:    in std_logic_vector(3 downto 0)
);
end sfif;

architecture RTL of sfif is

  -- transfer process signals
  signal Tdata:  std_logic_vector(31 downto 0);     -- transmit data for SPI
  signal xstart: std_logic;                         -- start strobe
  signal divisor, divider, prescale: integer range 0 to 3;
  signal sclk_i: std_logic;
  signal TdatSR: std_logic_vector(31 downto 0);     -- transmit shift register
  signal xdone:  std_logic;                         -- done flag
  signal xcount, RTcount: integer range 0 to 31;    -- symbol counter
  type t_state_t is (t_idle, t_transfer, t_finish);
  signal t_state: t_state_t;
  signal endcs, isxfer:  std_logic;
  signal rate, xrate, quadrate: std_logic;          -- single / quad
  signal xdir: std_logic;                           -- direction: in / out
  signal xcs: std_logic_vector(1 downto 0);         -- transfer NCS
  signal dummy, dummycnt: integer range 0 to 15;    -- dummy counts after transfer
  signal qdummy: integer range 0 to 15;             -- dummy counts after quad xfer

component SPRAMEZ
generic (
  Wide:  integer := 32;                             -- width of word
  Size:  integer := 10                              -- log2 (words)
);
port (
  clk:    in  std_logic;                            -- System clock
  reset:  in  std_logic;                            -- async reset
  en:     in  std_logic;                            -- Memory Enable
  we:     in  std_logic;                            -- Write Enable (0=read, 1=write)
  addr:   in  std_logic_vector(Size-1 downto 0);
  data_i: in  std_logic_vector(Wide-1 downto 0);    -- write data
  data_o: out std_logic_vector(Wide-1 downto 0)     -- read data
);
END component;

  type cache_t is array (0 to 2**CacheSize-1) of std_logic_vector(31 downto 0);
  signal cache_ok:   std_logic_vector(2**CacheSize-1 downto 0); -- cache word is filled
  signal cache_pend: std_logic_vector(2**CacheSize-1 downto 0); -- cache word is pending
  signal cache_addr: unsigned(29 downto 0);         -- current cache address
  signal cache: cache_t;                            -- prefetch cache
  signal missed, cokay: std_logic;                  -- cache miss, ready
  signal cache_cnt: integer range 0 to 2**CacheSize;-- amount of cache to fill
  signal upperaddr, loweraddr: unsigned(15 downto 0);
    type c_state_t is (c_idle, c_addr24, c_addr32, c_first, c_fill, c_end);
  signal c_state: c_state_t;
  signal cache_wipe: std_logic;                     -- tell prefetch cache was wiped
  signal ca: integer range 0 to 2**CacheSize-1;

    type f_state_t is (f_idle, f_start, f_fill, f_fillx, f_run);
  signal f_state: f_state_t;
  signal int_addr: unsigned(RAMsize-1 downto 0);    -- Internal "ROM" address
  signal ram_addr, ram_waddr: std_logic_vector(RAMsize-1 downto 0);
  signal ram_en, ram_we, ram_wen, internal: std_logic;
  signal ram_in, ram_out: std_logic_vector(31 downto 0); -- cache word is filled
  constant last_addr: unsigned(RAMsize-1 downto 0) := (others => '1');
  constant next_to_last: unsigned(RAMsize-1 downto 0) := last_addr - 1;
  signal int_a0: unsigned(RAMsize-1 downto 0);      -- internal addr

begin -------------------------------------------------------------------------

ca <= to_integer(unsigned(cache_addr(CacheSize-1 downto 0)));

-- The ROM interface attempts to fetch from a small cache of 16 words.
-- The cache attempts to fill from memory. Any change in caddr[25:4] restarts it.
-- The bottom of memory is a synchronous-read RAM that shadows the bottom of flash.

prefetch: process (clk, reset) begin
  if reset='1' then
    cache_ok   <= (others=>'0');  xstart <= '0';  ram_wen <= '0';
    cache_pend <= (others=>'0');  xcount <= 0;    dummy <= 0;
    cache_addr <= (others=>'0');  xrate <= '0';   xdir <= '0';
    f_state <= f_idle;                          -- start out filling RAM with flash contents
    Tdata <= x"00000000";  cache_cnt <= 0;
    ram_in <= x"00000000";  cache_wipe <= '1';
    xcs <= "00";  int_addr <= (others=>'0');
    ram_waddr <= (others=>'0');
  elsif rising_edge(clk) then
    xstart <= '0';
    ram_wen <= '0';
    case f_state is
    when f_idle =>                              -- boot at reset:
      xcount <= 31;  Tdata <= x"0B" & std_logic_vector(BaseBlock) & x"0000";
      dummy <= 8;  xdir <= '1';  xrate <= '0';
      xstart <= '1';  f_state <= f_start;       -- start a fast read from BaseBlock address
    when f_start =>
      if (xstart = '0') and (xdone = '1') then  -- transfer done
        int_addr <= (others=>'0');  Tdata <= x"00000000";
        xcount <= 31;  dummy <= 0;  xdir <= '0';
        xstart <= '1';  f_state <= f_fill;      -- fetch first 32-bit word
      end if;
    when f_fill =>
      if (xstart = '0') and (xdone = '1') then
        ram_waddr <= std_logic_vector(int_addr);-- write to RAM, used as "internal ROM"
        ram_wen <= '1';  ram_in <= TdatSR;
        if int_addr = last_addr then
          f_state <= f_fillx;                   -- finished booting
        else
          int_addr <= int_addr + 1;
          if int_addr = next_to_last then
            xcs <= "01";
          end if;
          xcount <= 31;  dummy <= 0;
          xstart <= '1';
        end if;
      end if;
    when f_fillx =>                             -- allow time to write
      f_state <= f_run;
    when f_run =>
      if internal = '1' then                    -- reading from internal block RAM
        int_addr <= unsigned(caddr(RAMsize-1 downto 0));
      else                                      -- filling cache
        if (missed = '1') or (cache_pend (to_integer(unsigned(not caddr(CacheSize-1 downto 0)))) = '0') then
        -- cache miss or hit on a part that isn't waiting to be filled
          cache_ok <= (others=>'0');
          cache_addr <= unsigned(caddr);
          cache_pend <= (others=>'0');          -- waiting to be filled = '1'
          cache_pend (to_integer(unsigned(not caddr(CacheSize-1 downto 0))) downto 0) <= (others => '1');
          cache_cnt <= 2**CacheSize - to_integer(unsigned(caddr(CacheSize-1 downto 0)));
          cache_wipe <= '1';                    -- end current prefetch and restart it
        end if;
        case c_state is
        when c_idle =>                          -- NCS already at '1'
          if cache_wipe = '1' then
            cache_wipe <= '0';
            xcs <= "00";  xcount <= 7;  dummy <= 0;  xdir <= '1';
            if cache_addr(29 downto 22) = x"00" then
              Tdata(31 downto 24) <= x"0B";  c_state <= c_addr24;
            else
              Tdata(31 downto 24) <= x"0C";  c_state <= c_addr32;
            end if;
            xstart <= '1';
          end if;
        when c_addr32 =>
          if (xstart = '0') and (xdone = '1') then
            Tdata <= std_logic_vector(upperaddr & loweraddr);
            xrate <= quadrate;
            if quadrate = '1' then
              xcount <= 7;
              dummy <= qdummy;
            else
              xcount <= 31;
              dummy <= 8;
            end if;
            xstart <= '1';  c_state <= c_first;
          end if;
        when c_addr24 =>
          if (xstart = '0') and (xdone = '1') then
            Tdata(31 downto 8) <= std_logic_vector(upperaddr(7 downto 0) & loweraddr);
            xrate <= quadrate;
            if quadrate = '1' then
              xcount <= 5;
              dummy <= qdummy;
            else
              xcount <= 23;
              dummy <= 8;
            end if;
            xstart <= '1';  c_state <= c_first;
          end if;
        when c_first =>                         -- request 1st word
          if (xstart = '0') and (xdone = '1') then
            Tdata <= x"00000000";  dummy <= 0;  xdir <= '0';
            if quadrate = '1' then
              xcount <= 7;
            else
              xcount <= 31;
            end if;
            xstart <= '1';  c_state <= c_fill;
          end if;
        when c_fill =>                          -- prefetch flash into cache
          if (xstart = '0') and (xdone = '1') then
            if cache_ok(ca) = '0' then
               cache(ca) <= TdatSR;             -- save if not already not prefetched
            end if;
            cache_ok(ca) <= not cache_wipe;
            if cache_cnt = 0 then
              c_state <= c_idle;
            else
              if quadrate = '1' then
                xcount <= 7;
              else
                xcount <= 31;
              end if;
              xstart <= '1';
              if cache_wipe = '1' then
                xcs <= "01";  xcount <= 1;      -- stop prefetch by deasserting CS
                c_state <= c_end;
              else
                if cache_cnt = 1 then           -- this is the last word
                  xcs <= "01";
                else
                  cache_addr <= cache_addr + 1;
                end if;
                cache_cnt <= cache_cnt - 1;
              end if;
            end if;
          end if;
        when c_end =>                           -- cache has been wiped, idle will refill
          if (xstart = '0') and (xdone = '1') then
            c_state <= c_idle;
          end if;
        end case;
      end if;
    end case;
  end if;
end process prefetch;

missed <= '0' when cache_addr(29 downto CacheSize) = unsigned(caddr(29 downto CacheSize)) else '1';
cokay <= cache_ok(to_integer(unsigned(caddr(CacheSize-1 downto 0))));
upperaddr <= cache_addr(29 downto 14) + (x"00" & BaseBlock);
loweraddr <= cache_addr(13 downto 0) & "00";

RAM_decode: process (caddr, f_state, ram_wen, ram_waddr, cache, cache_ok, ram_out, cokay, missed) begin
  internal <= '0';
  if f_state = f_run then
    ram_we <= '0';  ram_en <= '0';
    ram_addr <= caddr(RAMsize-1 downto 0);
    if unsigned(caddr(29 downto RAMsize)) = 0 then -- internal RAM
      ram_en <= '1';  internal <= '1';
      cdata <= ram_out;
      if unsigned(caddr(RAMsize-1 downto 0)) = int_addr then
        cready <= '1';
      else
        cready <= '0';
      end if;
    else -- external flash (cached)
      cdata  <= cache (to_integer(unsigned(caddr(CacheSize-1 downto 0))));
      cready <= cokay and (not missed);
    end if;
  else -- not running yet
    cready <= '0';
    ram_en <= ram_wen;
    ram_we <= ram_wen;
    ram_addr <= ram_waddr;
  end if;
end process RAM_decode;

ram: SPRAMEZ
GENERIC MAP ( Wide => 32, Size => RAMsize )
PORT MAP (
  clk => clk,  reset => reset,  en => ram_en,  we => ram_we,
  addr => ram_addr,  data_i => ram_in,  data_o => ram_out
);

----------------------------------------------------------------------------------
-- SPI transfer shifts incoming data into TdatSR while shifting it out to the bus.
-- To send shorter data than 32-bit, left-justify it.
-- The xstart strobe triggers a transfer. xdone rises when it's finished.
-- TdatSR is the received data.

quadrate <= config(7);
divisor <= to_integer(unsigned(config(5 downto 4)));
qdummy  <= to_integer(unsigned(config(3 downto 2)))*2 + 4;
sclk <= sclk_i;

transfer: process (clk, reset) begin
  if reset='1' then
    RTcount <= 0;  divider <= 0;
    TdatSR <= x"00000000";  xdone <= '0';  sclk_i <= '0';  NCS <= '1';
    data_o <= "0000";  t_state <= t_idle;  endcs <= '0';
    xdata_o <= x"00";  xbusy <= '0';  isxfer <= '0';
    drive_o <= "0000";  dummycnt <= 0;  rate <= '0';
  elsif rising_edge(clk) then
    case t_state is
    when t_idle =>
      if xstart = '1' then                      -- controller requests a transfer
        RTcount <= xcount;  dummycnt <= dummy;
        isxfer <= '0';  rate <= xrate;
        divider <= divisor;  xdone <= '0';
        if xrate = '0' then
          data_o(0) <= Tdata(31);
          TdatSR <= Tdata(30 downto 0) & '0';
        else
          data_o <= Tdata(31 downto 28);
          TdatSR <= Tdata(27 downto 0) & x"0";
        end if;
        NCS <= xcs(1);  endcs <= xcs(0);
        if rate = '1' then
          drive_o <= (others => xdir);
        else
          drive_o <= "000" & xdir;
        end if;
        t_state <= t_transfer;
      elsif xtrig = '1' then                    -- MCU requests a transfer
        RTcount <= 7;  dummycnt <= 0;
        divider <= divisor;  isxfer <= '1';
        rate <= '0';  TdatSR(31 downto 25) <= xdata_i(6 downto 0);
        data_o(0) <= xdata_i(7);                -- single rate, 8-bit
        xbusy <= '1';  t_state <= t_transfer;
        NCS <= xdata_i(9);  endcs <= xdata_i(8);
      end if;
    when t_transfer =>
      if (divider = 0) then
        divider <= divisor;
        sclk_i <= not sclk_i;
        if sclk_i = '1' then                    -- output to SPI on falling edge
          if rate = '0' then
            TdatSR <= TdatSR(30 downto 0) & data_i(1);
            data_o(0) <= TdatSR(31);            -- SDR
          else
            TdatSR <= TdatSR(27 downto 0) & data_i;
            data_o <= TdatSR(31 downto 28);     -- QDR
          end if;
          if RTcount = 0 then
            t_state <= t_finish;
            drive_o <= "0000";                  -- float the bus
          else
            RTcount <= RTcount - 1;
          end if;
        end if;
      else
        divider <= divider - 1;
      end if;
    when t_finish =>
      if dummycnt = 0 then
        if (divider = 0) then                   -- wait for last bit to finish
          sclk_i <= '0';  xbusy <= '0';
          NCS <= endcs;
          if isxfer = '1' then
            xdata_o <= TdatSR(7 downto 0);
          else
            xdone <= '1';
          end if;
          t_state <= t_idle;
        else
          divider <= divider - 1;
        end if;
      elsif (divider = 0) then
        sclk_i <= not sclk_i;
        if sclk_i = '1' then                    -- output to SPI on falling edge
          if dummycnt /= 0 then
            dummycnt <= dummycnt - 1;
          end if;
        end if;
      else
        divider <= divider - 1;
      end if;
    end case;
  end if;
end process transfer;

end RTL;
