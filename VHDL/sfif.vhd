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
  busy:     out std_logic;
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
  signal divisor, divider: integer range 0 to 3;
  signal sclk_i: std_logic;
  signal TdatSR: std_logic_vector(31 downto 0);     -- transmit shift register
  signal TdatSRle: std_logic_vector(31 downto 0);   -- byte-reversed
  signal xdone:  std_logic;                         -- done flag
  signal xcount, RTcount: integer range 0 to 31;    -- symbol counter
  type t_state_t is (t_idle, t_transfer, t_finish);
  signal t_state: t_state_t;
  signal endcs, isxfer, hasmode:  std_logic;
  signal rate, xrate: std_logic_vector(1 downto 0);
  signal spirate: std_logic_vector(1 downto 0);     -- single / double / quad
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
  signal upperaddr, loweraddr: unsigned(15 downto 0);
    type c_state_t is (c_idle, c_addr24, c_addr32, c_mode, c_first, c_fill, c_end);
  signal c_state: c_state_t;
  signal ca: integer range 0 to 2**CacheSize-1;
  signal restart: std_logic;

    type f_state_t is (f_idle, f_start, f_fill, f_fillx, f_run);
  signal f_state: f_state_t;
  signal int_addr: unsigned(RAMsize-1 downto 0);    -- Internal "ROM" address
  signal ram_addr, ram_waddr: std_logic_vector(RAMsize-1 downto 0);
  signal ram_en, ram_we, ram_wen, internal: std_logic;
  signal ram_in, ram_out: std_logic_vector(31 downto 0); -- cache word is filled
  constant last_addr: unsigned(RAMsize-1 downto 0) := (others => '1');
  constant next_to_last: unsigned(RAMsize-1 downto 0) := last_addr - 1;

begin -------------------------------------------------------------------------

ca <= to_integer(unsigned(cache_addr(CacheSize-1 downto 0)));

restart <= (not internal) and -- external read, cache miss, fetch not pending
   (missed or not cache_pend (to_integer(unsigned(caddr(CacheSize-1 downto 0)))));

-- The ROM interface attempts to fetch from a small cache of 16 words.
-- The cache attempts to fill from memory. Any change in caddr[25:4] restarts it.
-- The bottom of memory is a synchronous-read RAM that shadows the bottom of flash.

prefetch: process (clk, reset) begin
  if reset='1' then
    cache_ok   <= (others=>'0');  xstart <= '0';  ram_wen <= '0';
    cache_pend <= (others=>'0');  xcount <= 0;    dummy <= 0;
    cache_addr <= (others=>'0');  xrate <= "00";  xdir <= '0';
    f_state <= f_idle;                          -- start out filling RAM with flash contents
    Tdata <= x"00000000";
    ram_in <= x"00000000";  busy <= '1';
    xcs <= "00";  int_addr <= (others=>'0');
    ram_waddr <= (others=>'0');
  elsif rising_edge(clk) then
    xstart <= '0';
    ram_wen <= '0';
    case f_state is
    when f_idle =>                              -- boot at reset:
      xcount <= 31;  Tdata <= x"0B" & std_logic_vector(BaseBlock) & x"0000";
      dummy <= 8;  xdir <= '1';  xrate <= "00";
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
        ram_wen <= '1';  ram_in <= TdatSRle;
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
      end if;
      if (xstart = '0') and (xdone = '1') then  -- FSM to fill cache from SPI flash
        case c_state is
        when c_idle =>                          -- NCS already at '1'
          if restart = '1' then
            cache_ok <= (others=>'0');
            cache_addr <= unsigned(caddr);
            cache_pend <= (others=>'0');        -- waiting to be filled = '1'
            cache_pend (2**CacheSize-1 downto to_integer(unsigned(caddr(CacheSize-1 downto 0)))) <= (others => '1');
            xrate <= "00";
            xcs <= "00";  xcount <= 7;  dummy <= 0;  xdir <= '1';
            if caddr(29 downto 22) = x"00" then
              case spirate is                   -- can use 24-bit address
              when "00" =>   Tdata(31 downto 24) <= x"0B";
              when "01" =>   Tdata(31 downto 24) <= x"BB";
              when others => Tdata(31 downto 24) <= x"EB";
              end case;
        	    c_state <= c_addr24;
            else
              case spirate is
              when "00" =>   Tdata(31 downto 24) <= x"0C";
              when "01" =>   Tdata(31 downto 24) <= x"BC";
              when others => Tdata(31 downto 24) <= x"EC";
              end case;
        	    c_state <= c_addr32;
            end if;
            xstart <= '1';
            busy <= '1';
          else
            busy <= '0';
          end if;
        when c_addr32 =>
          Tdata <= std_logic_vector(upperaddr & loweraddr);
          xrate <= spirate;
          case spirate is
          when "00"   => xcount <= 31;  dummy <= 8;
          when "01"   => xcount <= 15;  dummy <= qdummy;
          when others => xcount <= 7;   dummy <= qdummy;
          end case;
          xstart <= '1';
        	if hasmode = '1' then
        	  c_state <= c_mode;
        	  dummy <= 0;
        	else
        	  c_state <= c_first;
        	end if;
        when c_addr24 =>
          Tdata(31 downto 8) <= std_logic_vector(upperaddr(7 downto 0) & loweraddr);
          xrate <= spirate;
          case spirate is
          when "00"   => xcount <= 23;  dummy <= 8;
          when "01"   => xcount <= 11;  dummy <= qdummy;
          when others => xcount <= 5;   dummy <= qdummy;
          end case;
          xstart <= '1';
          if hasmode = '1' then
            c_state <= c_mode;
            dummy <= 0;
          else
            c_state <= c_first;
          end if;
        when c_mode =>
          Tdata <= x"FF000000";  dummy <= qdummy;
          case spirate is
          when "01" =>   xcount <= 3;
          when others => xcount <= 1;
          end case;
          xstart <= '1';  c_state <= c_first;
        when c_first =>                         -- request 1st word
          Tdata <= x"00000000";  dummy <= 0;  xdir <= '0';
          case spirate is
          when "00"   => xcount <= 31;
          when "01"   => xcount <= 15;
          when others => xcount <= 7;
          end case;
          xstart <= '1';  c_state <= c_fill;
        when c_fill =>                          -- prefetch flash into cache
          cache(ca) <= TdatSRle;
          cache_ok(ca) <= not restart;          -- tag as okay unless cache is restarting
          cache_pend(ca) <= '0';                -- clear pending either way
          if  not cache_addr(CacheSize-1 downto 0) = 0  then
            c_state <= c_end;  xcs <= "01";     -- last word in cache buffer
          else
            cache_addr <= cache_addr + 1;
          end if;
          case spirate is
          when "00"   => xcount <= 31;
          when "01"   => xcount <= 15;
          when others => xcount <= 7;
          end case;
          xstart <= '1';
          if restart = '1' then
            xcs <= "01";  xcount <= 0;          -- stop prefetch by deasserting CS
            c_state <= c_end;
          end if;
        when c_end =>                           -- cache has been wiped, idle will refill
          c_state <= c_idle;
        end case;
      end if;
    end case;
  end if;
end process prefetch;

missed <= '0' when cache_addr(29 downto CacheSize) = unsigned(caddr(29 downto CacheSize)) else '1';
cokay <= cache_ok(to_integer(unsigned(caddr(CacheSize-1 downto 0))));
upperaddr <= cache_addr(29 downto 14) + (x"00" & BaseBlock);
loweraddr <= cache_addr(13 downto 0) & "00";

-- convert to little endian format
TdatSRle <= TdatSR(7 downto 0) & TdatSR(15 downto 8) & TdatSR(23 downto 16) & TdatSR(31 downto 24);

-- mux Block RAM or cache data to output
RAM_decode: process (caddr, f_state, ram_wen, ram_waddr, cache, ram_out, cokay, missed, int_addr) begin
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
    cdata <= x"00000000";
    ram_en <= ram_wen;
    ram_we <= ram_wen;
    ram_addr <= ram_waddr;
  end if;
end process RAM_decode;

-- Block RAM
ram: SPRAMEZ
GENERIC MAP ( Wide => 32, Size => RAMsize )
PORT MAP ( clk => clk,  en => ram_en,  we => ram_we,
  addr => ram_addr,  data_i => ram_in,  data_o => ram_out
);

----------------------------------------------------------------------------------
-- SPI transfer shifts incoming data into TdatSR while shifting it out to the bus.
-- To send shorter data than 32-bit, left-justify it.
-- The xstart strobe triggers a transfer. xdone rises when it's finished.
-- TdatSR is the received data.

spirate <= config(7 downto 6);
hasmode <= config(5);
divisor <= to_integer(unsigned(config(4 downto 3)));
qdummy  <= to_integer(unsigned(config(2 downto 1)))*2 + 4; -- 4, 6, 8, 10
sclk <= sclk_i;

transfer: process (clk, reset) begin
  if reset='1' then
    RTcount <= 0;  divider <= 0;
    TdatSR <= x"00000000";  xdone <= '0';  sclk_i <= '0';  NCS <= '1';
    data_o <= "0000";  t_state <= t_idle;  endcs <= '0';
    xdata_o <= x"00";  xbusy <= '0';  isxfer <= '0';
    drive_o <= "0000";  dummycnt <= 0;  rate <= "00";
  elsif rising_edge(clk) then
    case t_state is
    when t_idle =>
      if xstart = '1' then                      -- controller requests a transfer
        RTcount <= xcount;  dummycnt <= dummy;
        isxfer <= '0';  rate <= xrate;
        divider <= divisor;  xdone <= '0';
        case xrate is
        when "00" =>
          drive_o <= "000" & xdir;
          data_o(0) <= Tdata(31);
          TdatSR <= Tdata(30 downto 0) & '0';
        when "01" =>
          drive_o <= "00" & xdir & xdir;
          data_o(1 downto 0) <= Tdata(31 downto 30);
          TdatSR <= Tdata(29 downto 0) & "00";
        when others =>
          drive_o <= (others => xdir);
          data_o <= Tdata(31 downto 28);
          TdatSR <= Tdata(27 downto 0) & x"0";
        end case;
        NCS <= xcs(1);  endcs <= xcs(0);
        t_state <= t_transfer;
      elsif xtrig = '1' then                    -- MCU requests a transfer
        RTcount <= 7;  dummycnt <= 0;
        divider <= divisor;  isxfer <= '1';
        rate <= "00";  drive_o <= "0001";
        TdatSR(31 downto 25) <= xdata_i(6 downto 0);
        data_o(0) <= xdata_i(7);                -- single rate, 8-bit
        xbusy <= '1';  t_state <= t_transfer;
        NCS <= xdata_i(9);  endcs <= xdata_i(8);
      end if;
    when t_transfer =>
      if (divider = 0) then
        divider <= divisor;
        sclk_i <= not sclk_i;
        if sclk_i = '1' then                    -- output to SPI on falling edge
          case rate is
          when "00" =>
            TdatSR <= TdatSR(30 downto 0) & data_i(1);
            data_o(0) <= TdatSR(31);            -- serial
          when "01" =>
            TdatSR <= TdatSR(29 downto 0) & data_i(1 downto 0);
            data_o(1 downto 0) <= TdatSR(31 downto 30); -- dual
          when others =>
            TdatSR <= TdatSR(27 downto 0) & data_i;
            data_o <= TdatSR(31 downto 28);     -- quad
          end case;
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
        drive_o <= "0000";                      -- float during dummy cycles
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
