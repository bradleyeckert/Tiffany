library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

-- Serial Flash interface

entity sfif is
  port (clk:    in std_logic;
    reset:      in std_logic; -- reset is async
-- ROM wishbone interface
    rom_addr:   in std_logic_vector(23 downto 0);
    rom_data:  out std_logic_vector(7 downto 0);
    rom_stb:    in std_logic;
    rom_ack:   out std_logic;
-- I/O wishbone interface
    io_addr:    in std_logic_vector(1 downto 0);
    io_data_i:  in std_logic_vector(7 downto 0);
    io_data_o: out std_logic_vector(7 downto 0);
    io_we:      in std_logic;
    io_stb:     in std_logic;
    io_ack:    out std_logic;
-- 6-wire SPI flash connection
    SCLK:     out std_logic;                    -- clock                    6
    MOSI:   inout std_logic;                    -- to flash        / sio1   5
    MISO:   inout std_logic;                    -- data from flash / sio0   2
    NCS:      out std_logic;                    -- chip select              1
    NWP:    inout std_logic;                    -- write protect   / sio2   3
    NHOLD:  inout std_logic);                   -- hold            / sio3   7
end sfif;

architecture behavioral of sfif is
  signal float, first, last: std_logic;
  signal rate, rrate, speed, prescale: std_logic_vector(1 downto 0);
  signal data, data0, cache: std_logic_vector(7 downto 0);
  signal ticks, tick0: std_logic_vector(3 downto 0);
  signal nextaddr: std_logic_vector(23 downto 0);
  type statetype is (idle, command, highA, mediumA, lowA, zero, dummy, xfer);
  signal state: statetype;
  signal ioxfer: std_logic;                     -- io port transfer in progress

begin -------------------------------------------------------------------------

NHOLD <= 'Z' when (float='1') or (rate(1)='0') else data(7);
NWP   <= 'Z' when (float='1') or (rate(1)='0') else data(6);
MOSI  <= 'Z' when (float='1') else data(5) when rate(1)='1' else data(7);
MISO  <= 'Z' when (float='1') else data(6) when rate="01"
                              else data(4) when rate(1)='1' else 'Z';
io_data_o <= (rrate & rate & NHOLD & NWP & MOSI & MISO) when io_addr="11" else cache;
tick0 <= "1111" when rate="00" else "0111" when rate="01" else "0011";

data0 <= (data(3 downto 0) & NHOLD & NWP & MOSI & MISO) when rate(1)='1'
    else (data(5 downto 0) & MOSI & MISO) when rate(0)='1'
    else (data(6 downto 0) & MISO);

process (clk, reset) begin
  if reset='1' then
    rom_ack <= '0';
    io_ack <= '0';
    speed <= "00"; -- fastest
    ticks <= "0000";
    prescale <= "11";
    rate <= "00";
    rrate <= "00";
    first <= '0';
    last <= '0';
    float <= '0';
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
