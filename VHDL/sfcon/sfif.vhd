library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- Serial NOR Flash interface

-- Supports 2^30 32-bit words, or 2^32 (4G) bits. BTW, 1G parts are under $10.
-- Parts beyond 128Mb use 4-byte read operations for the upper address range.

-- Upon POR, the RAM is loaded from flash.
-- RAMsize is log2 of the number of 32-bit RAM words.

-- BaseAddr is the physical address of the first 64KB sector. It allows the
-- bottom of flash to contain the FPGA bitstream.

entity sfif is
generic (
  RAMsize:   integer := 10;                     -- log2 (RAM cells)
  BaseBlock: integer := 0
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

architecture behavioral of sfif is
  signal rate, divisor, prescale: integer range 0 to 3;


  signal float, first, last: std_logic;
  signal data, data0, cache: std_logic_vector(7 downto 0);
  signal ticks, tick0: std_logic_vector(3 downto 0);
  signal nextaddr: std_logic_vector(29 downto 0);
  type statetype is (idle, command, highA, mediumA, lowA, zero, dummy, xfer);
  signal state: statetype;
  signal ioxfer: std_logic;                     -- io port transfer in progress

  type cache_t is array (0 to 15) of std_logic_vector(31 downto 0);
  signal cache_ok: std_logic_vector(31 downto 0);-- cache word is filled
  signal cache_addr: unsigned(29 downto 0);     -- current cache address
  signal cache: cache_t;                        -- prefetch cache
  signal missed: std_logic;                     -- cache miss

component SPRAMEZ
generic (
  Wide:  integer := 32;                         -- width of word
  Size:  integer := 10                          -- log2 (words)
);
port (
  clk:    in  std_logic;                        -- System clock
  reset:  in  std_logic;                        -- async reset
  en:     in  std_logic;                        -- Memory Enable
  we:     in  std_logic;                        -- Write Enable (0=read, 1=write)
  addr:   in  std_logic_vector(Size-1 downto 0);
  data_i: in  std_logic_vector(Wide-1 downto 0);-- write data
  data_o: out std_logic_vector(Wide-1 downto 0) -- read data
);
END component;

  type f_state_t is (f_start, f_fill, f_run);
  signal f_state: f_state_t;

  -- SPI transfer
  signal Rdata:  std_logic_vector(31 downto 0); -- receive shift register
  signal TdatSR: std_logic_vector(31 downto 0); -- transmit shift register
  signal xstart: std_logic;                     -- start strobe
  signal xdone:  std_logic;                     -- done flag
  signal xcount, RTcount: integer range 0 to 31;-- symbol counter

begin -------------------------------------------------------------------------

-- Transfer shifts incoming data into Rdata while shifting Tdata out to the bus.
-- Rdata and Tdata are 32-bit words. To send shorter data, left-justify it.
-- The xstart strobe triggers a transfer. xdone rises when it's finished.

with rate select data_o <= -- {io3 io2 io1 io0}
  TdatSR(31 downto 29) & TdatSR(31) when 0,             -- single
  TdatSR(31 downto 30) & TdatSR(31 downto 30) when 1,   -- dual
  TdatSR(31 downto 28) when others;                     -- quad

with config(5 downto 4) select divisor <=
  2 when "00",     -- Fclk/8
  1 when "01",     -- Fclk/4
  0 when others;   -- Fclk/2

transfer: process (clk, reset) begin
  if reset='1' then
    Rdata  <= x"00000000";  xcount <= 0;
    TdatSR <= x"00000000";  xdone <= '0';
  elsif rising_edge(clk) then
    if RTcount = 0 then
      if xstart = '1' then
        RTcount <= xcount;
        rate <= to_integer(unsigned(config(7 downto 6)));
        TdatSR <= Tdata;
        xdone <= '0';
      else
        xdone <= '1';
      end if;
    elsif divider = 0 then
      RTcount <= RTcount - 1;
      divider <= divisor;
    else
      divider <= divider - 1;
    end if;
  end if;
end process transfer;

missed <= '0' when cache_addr(29 downto 4) = caddr(29 downto 4) else '1';
cready <= '0' when missed = '1' else cache_ok(to_integer(unsigned(caddr(3 downto 0))));
cdata <= cache(to_integer(unsigned(caddr(3 downto 0))));

-- The ROM interface attempts to fetch from a small cache of 16 words.
-- The cache attempts to fill from memory. Any change in caddr[25:4] restarts it.

prefetch: process (clk, reset) begin
  if reset='1' then
    cache_ok <= (others=>'0');
    cache_addr <= (others=>'0');
    data <= x"0B";  count <= 8;
    f_state <= f_start;                         -- start out filling RAM with flash contents
  elsif rising_edge(clk) then
    if missed = '1' then
      cache_ok <= (others=>'0');
      cache_addr <= caddr;
      state <=
    else
    end if;
  end if;
end process prefetch;





process (clk, reset) begin
  if reset='1' then
    prescale <= 0;
    nextaddr <= (others=>'1');

    data <= "00000000";
    state <= idle;
    SCLK <= '0';
    NCS <= '1';
    ioxfer <= '0';
  elsif rising_edge(clk) then
-- SPI state machine -----------------------------------------------------------
    if (prescale="00") then                     -- tick once every 1, 2, 3 or 4 clocks
      prescale <= speed;
      if (ticks/="0000") then                   -- process half-bits, if any
        ticks <= ticks - 1;
        if (first='0') then
          SCLK <= ticks(0);                     -- generate SCLK
          if (ticks(0)='0') then
            data <= data0;                      -- falling SCLK -> sample and shift
          end if;
          if (ticks="0001") then
            cache <= data0;                     -- received data for sequential reads
          end if;
        else
          NCS <= '1';
          if (ticks="0001") then                -- insert a long inactive CS
            first <= '0';                       -- before the SPI transfer
            ticks <= tick0;                     -- then restart the bit timer
            NCS <= '0';
          end if;
        end if;
      else
        if (state=xfer) then                    -- end of random access
          rom_ack <= rom_stb after 2 ns;
        end if;
        SCLK <= '0';
      end if;
    else
      prescale <= prescale - 1;
    end if;
-- I/O port --------------------------------------------------------------------
    if (io_stb='1') then
      if (io_we='1') then
        if (io_addr="11") then
          speed <= io_data_i(1 downto 0);       -- set new SPI speed
          rate  <= io_data_i(5 downto 4);       -- set new immediate data rate
          rrate <= io_data_i(7 downto 6);       -- set new ROM data rate
          io_ack <= '1' after 2 ns;
        else
          if ioxfer='0' then
            float <= '0';
            first <= io_addr(0);                -- set up to deassert CS before transfer
            last  <= io_addr(1);                -- set up to deassert CS after transfer
            data <= io_data_i;                  -- start a transfer
            nextaddr <= (others=>'1');          -- pending ROM byte is now invalid
            ticks <= tick0;
            ioxfer <= '1';
          elsif (ticks="0000") then             -- transfer finished
            NCS <= last;
            float <= last;
            io_ack <= '1' after 2 ns;
          end if;
        end if;
      else
        io_ack <= '1' after 2 ns;               -- reads are trivial
      end if;
    else
      ioxfer <= '0';
      io_ack <= '0' after 2 ns;
    end if;
-- ROM port --------------------------------------------------------------------
    if (ticks="0000") then
      case state is
        when idle =>
          if (rom_stb='1') then                 -- host is requesting ROM data
            if (rom_addr = nextaddr) then
              rom_data <= cache;                -- sequential data is already at the SPI output
              ticks <= tick0;
              rom_ack <= '1' after 2 ns;
            else
              state <= command;
              float <= '0';
              if (rate = "11") then
                rate <= rrate;
              else
                rate <= "00";
              end if;
              case rate is
                when "01" =>   data <= x"3B";   -- double
                when "10" =>   data <= x"EB";   -- quad
                when others => data <= x"0B";   -- single or SSTquad
              end case;
              first <= '1';
              ticks <= tick0;
              rom_ack <= '0' after 2 ns;
            end if;
            nextaddr <= rom_addr + 1;
          end if;
        when command =>                         -- process "fast read" command
          if (rate(1)='1') then
            rate <= rrate;                      -- quad rate address
          end if;
          data <= rom_addr(23 downto 16);       -- hi
          ticks <= tick0;  state <= highA;
        when highA =>
          data <= rom_addr(15 downto 8);        -- med
          ticks <= tick0;  state <= mediumA;
        when mediumA =>
          data <= rom_addr(7 downto 0);         -- lo
          ticks <= tick0;  state <= lowA;
        when lowA =>
          data <= "00000000";                   -- dummy
          ticks <= tick0;
          if (rate="10") then
            rate <= rrate;                      -- quad rate data (0xEB)
            state <= zero;                      -- 2-cycle "00" after address
          else
            rate <= "00";                       -- normal rate
            float <= '1';  state <= dummy;
          end if;
        when zero =>
          float <= '1';                         -- 0xEB command uses 4-cycle dummy
          data <= "00000000";
          rate <= "01";
          ticks <= tick0;  state <= dummy;
        when dummy =>
          data <= "00000000";                   -- xfer
          rate <= rrate;
          ticks <= tick0;  state <= xfer;
        when xfer =>
          data <= "00000000";                   -- prefetch
          rom_data <= data0;
          ticks <= tick0;  state <= idle;
      end case;
    elsif (state=idle) and (rom_stb='0') then
      rom_ack <= '0' after 2 ns;
    end if;
  end if;
end process;
end behavioral;
