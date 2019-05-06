LIBRARY ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
USE ieee.numeric_std.ALL;

ENTITY uart_tb IS
END uart_tb;

ARCHITECTURE behavior OF uart_tb IS

COMPONENT uart
  port(
    clk:       in std_logic;                    	-- CPU clock
    reset:     in std_logic;                    	-- sync reset
    ready:    out std_logic;                    	-- Ready for next byte to send
    wstb:      in std_logic;                    	-- Send strobe
    wdata:     in std_logic_vector(7 downto 0); 	-- Data to send
    rxfull:   out std_logic;                    	-- RX buffer is full, okay to accept
    rstb:      in std_logic;                    	-- Accept RX byte
    rdata:    out std_logic_vector(7 downto 0); 	-- Received data
    bitperiod: in std_logic_vector(15 downto 0);	-- Clocks per serial bit
    rxd:       in std_logic;
    txd:      out std_logic);
END COMPONENT;


   --Inputs
   signal clk : std_logic := '0';
   signal reset : std_logic := '0';
   signal x_wstb : std_logic := '0';
   signal x_rstb : std_logic := '0';
   signal x_wdata : std_logic_vector(7 downto 0) := (others => '0');
   signal rxd : std_logic := '1';

   --Outputs
   signal x_ready : std_logic;
   signal x_rxfull : std_logic;
   signal x_rdata : std_logic_vector(7 downto 0);
   signal txd : std_logic;

   -- Clock period definitions
   constant clk_period : time := 100 ns;      -- 10 MHz
   constant baud_period : time := 8680.55 ns; -- 115200 bps
   constant bitperiod : std_logic_vector(15 downto 0) := conv_std_logic_vector(10000000/115200, 16);

BEGIN

   -- Instantiate the Unit Under Test (UUT)
   uut: uart
        PORT MAP (
          clk => clk,
          reset => reset,
          ready => x_ready,
          wstb => x_wstb,
          wdata => x_wdata,
          rxfull => x_rxfull,
          rstb => x_rstb,
          rdata => x_rdata,
          bitperiod => bitperiod,
          rxd => rxd,
          txd => txd
        );

   -- Occasional incoming characters
   rx_process: process is
   procedure sendchar (char: in std_logic_vector(7 downto 0)) is
   begin
      rxd <= '0';      wait for baud_period;
      rxd <= char(0);  wait for baud_period;
      rxd <= char(1);  wait for baud_period;
      rxd <= char(2);  wait for baud_period;
      rxd <= char(3);  wait for baud_period;
      rxd <= char(4);  wait for baud_period;
      rxd <= char(5);  wait for baud_period;
      rxd <= char(6);  wait for baud_period;
      rxd <= char(7);  wait for baud_period;
      rxd <= '1';      wait for baud_period;
   end procedure;
   begin
      wait for baud_period*2;
      sendchar(x"5A");
      sendchar(x"12");
      sendchar(x"FF");
      sendchar(x"00");
   end process;

   -- System clock
   clk_process :process is
   begin
      clk <= '0'; wait for clk_period/2;
      clk <= '1'; wait for clk_period/2;
   end process;

   -- Stimulus process
   stim_proc: process is
   procedure sendbyte (char: in std_logic_vector(7 downto 0)) is
   begin
      loop
         wait until rising_edge(clk);
         if x_ready='1' then
            x_wdata <= char;
            x_wstb <= '1' after 1 ns;
            wait until rising_edge(clk);
            x_wstb <= '0' after 1 ns;
            exit;
         end if;
      end loop;
   end procedure;
   begin
      reset <= '1';  wait for clk_period*2.2;
      reset <= '0';
      sendbyte(x"06");
      sendbyte(x"00");
      sendbyte(x"00");
      sendbyte(x"04");
      sendbyte(x"98");
      wait;
   end process;

END;


