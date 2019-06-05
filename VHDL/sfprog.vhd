library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- SPI flash xport to UART interface
-- Intercepts UART escape sequences:
-- 10h 00h-03h translates to 10h-13h.
-- 10h 0Fh 12h puts the MCU in reset and starts accepting commands from the UART.
-- 1Bh (ESC) releases the MCU and allows UART to feed through.
-- 3Fh (?) returns a 4-byte product ID.
-- 40h to 7Fh is 5-bit data to send out xdata_o. Bit5 tags the last symbol.

ENTITY sfprog IS
generic (
  PID: std_logic_vector(31 downto 0) := x"87654321"  -- Product ID
);
port (
  clk	  : in	std_logic;			            -- System clock
  reset	  : in	std_logic;			            -- Asynchronous reset
  hold    : out	std_logic;                      -- resets the CPU, makes SFIF trigger from xtrig
  busy	  : in	std_logic;			            -- flash is busy
  -- SPI flash
  xdata_o : out std_logic_vector(9 downto 0);
  xdata_i : in  std_logic_vector(7 downto 0);
  xtrig   : out std_logic;
  xbusy   : in  std_logic;
  -- UART register interface: connects to the UART
  ready_u : in std_logic;                    	-- Ready for next byte to send
  wstb_u  : out std_logic;                    	-- Send strobe
  wdata_u : out std_logic_vector(7 downto 0); 	-- Data to send
  rxfull_u: in  std_logic;                    	-- RX buffer is full, okay for host to accept
  rstb_u  : out std_logic;                    	-- Accept RX byte
  rdata_u : in  std_logic_vector(7 downto 0);	-- Received data
  -- UART register interface: connects to the MCU
  ready   : out std_logic;                    	-- Ready for next byte to send
  wstb    : in  std_logic;                    	-- Send strobe
  wdata   : in  std_logic_vector(7 downto 0); 	-- Data to send
  rxfull  : out std_logic;                    	-- RX buffer is full, okay to accept
  rstb    : in  std_logic;                    	-- Accept RX byte
  rdata   : out std_logic_vector(7 downto 0) 	-- Received data
);
END sfprog;


ARCHITECTURE RTL OF sfprog IS

  type c_state_t is (c_idle, c_rx, c_tx, c_tx1, c_xfer);
  signal c_state: c_state_t;
  signal rxstate:  integer range 0 to 15;
  signal txstate:  integer range 0 to 15;
  signal parm:  std_logic_vector(9 downto 0);
  signal xtrig_i, hold_i:  std_logic;
  signal wstb_i:  std_logic;
  signal wdata_i: std_logic_vector(7 downto 0);

---------------------------------------------------------------------------------
BEGIN

xdata_o <= parm;
xtrig <= xtrig_i;
hold <= hold_i;
ready <= ready_u and (not hold_i);
wstb_u <= wstb_i or wstb;                       -- trigger UART TX from sfcon or MCU
wdata_u <= wdata_i when hold_i = '1' else wdata;

receiver: process (clk, reset) is
begin
  if reset = '1' then
    hold_i <= '0';  c_state <= c_idle;
    wdata_i <= x"00";
    wstb_i <= '0';  txstate <= 0;
    rstb_u <= '0';  rxstate <= 0;
	rxfull <= '0';  rdata <= x"00";
	xtrig_i <= '0'; parm <= "0000000000";
  elsif rising_edge(clk) then
    rstb_u <= '0';  wstb_i <= '0';	xtrig_i <= '0';
	case c_state is
	when c_idle =>
	  if rxfull_u = '1' then
	    rstb_u <= '1';
        c_state <= c_rx;
        case rxstate is                         -- what to do with it:
        when 0 =>
          if rdata_u = x"10" then
            rxstate <= 1;
          else
            rdata <= rdata_u;                   -- pass through non-escape
            rxfull <= '1';
          end if;
        when 1 =>
          rxstate <= 0;
          if rdata_u(7 downto 2) = "000000" then  -- 10 00-03 --> 10-13
            rdata <= "000100" & rdata_u(1 downto 0);
            rxfull <= '1';
          elsif rdata_u = x"0F" then
            rxstate <= 3;
          end if;
        when 2 =>                               -- logged in
          case rdata_u is
          when x"1B" =>                         -- log out
            rxstate <= 0;
            hold_i <= '0';
          when x"2F" =>                         -- ping
            wdata_i <= "0011110" & busy;
            c_state <= c_tx;
          when x"3F" =>                         -- request PID
            txstate <= 3;
            wdata_i <= PID(31 downto 24);
            c_state <= c_tx;
          when others =>
            if rdata_u(6) = '1' then            -- upper nybl = {4,5,6,7}
              parm <= parm(4 downto 0) & rdata_u(4 downto 0); -- 40h to 7Fh
              if rdata_u(5) = '1' then
                xtrig_i <= '1';
                c_state <= c_xfer;
              else
                c_state <= c_rx;
              end if;
            else
              c_state <= c_tx;
		      wdata_i <= rdata_u;			    -- UART TX
            end if;
          end case;
        when others =>                          -- expecting key
          if rdata_u = x"12" then               -- just use 0x12
            rxstate <= 2;
            hold_i <= '1';
          end if;
        end case;
	  end if;
	when c_rx =>
	  c_state <= c_idle;
	when c_tx =>                                -- sending wdata_i
	  if ready_u = '1' then
	    wstb_i <= '1';
        if txstate = 0 then
	      c_state <= c_idle;
        else
          txstate <= txstate - 1;
          c_state <= c_tx1;
        end if;
	  end if;
    when c_tx1 =>                               -- give ready_u time to respond
      if txstate = 14 then
        txstate <= 0;
        wdata_i <= "0101" & xdata_i(3 downto 0);
      else
        wdata_i <= PID(8*txstate+7 downto 8*txstate);
      end if;
	  c_state <= c_tx;
    when c_xfer =>
      if (xtrig_i = '0') and (xbusy = '0') then -- wait for transfer to finish
        wdata_i <= "0101" & xdata_i(7 downto 4);
        c_state <= c_tx;
        txstate <= 15;
      end if;
	end case;
    if rstb = '1' then
      rxfull <= '0';
    end if;
  end if;
end process receiver;

END RTL;
