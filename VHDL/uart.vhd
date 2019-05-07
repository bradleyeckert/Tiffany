library IEEE;
use IEEE.std_logic_1164.ALL;
use IEEE.std_logic_ARITH.ALL;
use IEEE.std_logic_UNSIGNED.ALL;

-- Unbuffered UART with FIFO-compatible interface.
-- Baud rate is set by bitperiod.

entity UART is
  port(
    clk:       in std_logic;                    	-- CPU clock
    reset:     in std_logic;                    	-- async reset
    ready:    out std_logic;                    	-- Ready for next byte to send
    wstb:      in std_logic;                    	-- Send strobe
    wdata:     in std_logic_vector(7 downto 0); 	-- Data to send
    rxfull:   out std_logic;                    	-- RX buffer is full, okay to accept
    rstb:      in std_logic;                    	-- Accept RX byte
    rdata:    out std_logic_vector(7 downto 0); 	-- Received data
    bitperiod: in std_logic_vector(15 downto 0);	-- Clocks per serial bit
    rxd:       in std_logic;
    txd:      out std_logic);
end UART;

architecture RTL of UART is

  signal baudint,  baudint0:  std_logic_vector(11 downto 0);
  signal baudfrac, baudfrac0: std_logic_vector(3 downto 0);
  signal txstate, rxstate:    std_logic_vector(7 downto 0);
  signal txreg, rxbuf, rxreg: std_logic_vector(7 downto 0);
  signal tnext, error:  std_logic;
  signal rxdi, rxda:  std_logic;

begin ------------------------------------------------------------------------

baudint0 <= bitperiod(15 downto 4);
baudfrac0 <= bitperiod(3 downto 0);
rdata <= rxbuf;

process (clk, reset) begin
  if reset = '1' then
    baudint  <= (others=>'0');
    baudfrac <= (others=>'0');
    txd <= '1';  error <= '0';
    rxfull <= '0';  tnext <= '1';
    txstate <= (others=>'0');
    rxstate <= (others=>'0');
    rxreg   <= (others=>'0');
    txreg   <= (others=>'0');
    rxbuf   <= (others=>'0');
    rxdi <= '1';  rxda <= '1';
    ready <= '1';
  elsif (rising_edge(clk)) then
    rxdi <= rxda;                               -- synchronize incoming RXD
    rxda <= rxd;
    if baudint = 0 then
      if (baudfrac0 > baudfrac) then            -- fractional divider:
           baudint <= baudint0;                 -- stretch by Frac/16 clocks
      else baudint <= baudint0 - 1;
      end if;
      baudfrac <= (baudfrac+1);                 -- 16 ticks/bit:
      txd <= tnext;
      if txstate=0 then
        ready <= '1';
      else
        if txstate(3 downto 0) = 0 then
          tnext <= txreg(0);                    -- TX
          txreg <= '1' & txreg(7 downto 1);
        end if;
        txstate <= txstate - 1;
      end if;
      if rxstate = 0 then
        error <= error and (not rxdi);          -- stop or mark ('1') clears error
        if (rxdi='0') and (error='0') then      -- start bit detected (maybe)
          rxstate <= "10101000";                -- will be sampled mid-bit
        end if;
      else
        rxstate <= rxstate - 1;
        if (rxstate(7 downto 4) /= "0000") then
          if (rxstate(3 downto 0) = "0000") then
            if (rxstate(7 downto 4) = "1010") then -- possible start bit
              if (rxdi='1') then
                rxstate <= "00000000";          -- false start, abort
              end if;
            else
              if (rxstate(7 downto 4) = "0001") then
                if rxdi='1' then                -- stop bit expected
                  rxbuf <= rxreg;               -- ignore BREAK and framing errors
                  rxfull <= '1';
                else
                  error <= '1';
                end if;
                rxstate <= "00000000";
              else 							    -- RX
                rxreg <= rxdi & rxreg(7 downto 1);
              end if;
            end if;
          end if;
        end if;
      end if;
    else
      baudint <= baudint - 1;
    end if;
    if rstb = '1' then
      rxfull <= '0';                            -- reading clears rxfull
    end if;
    if wstb = '1' then                          -- retrigger transmission
      txreg <= wdata;  tnext <= '0';
      txstate <= "10111111";                    -- n,8,2
      ready <= '0';
    end if;
  end if;
end process;

end RTL;
