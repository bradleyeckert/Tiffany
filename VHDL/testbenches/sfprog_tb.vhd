library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use IEEE.std_logic_textio.all;
use STD.textio.all;

ENTITY sfprog_tb IS
END sfprog_tb;

ARCHITECTURE behavior OF sfprog_tb IS

COMPONENT sfprog
generic (
  key: unsigned(31 downto 0) := x"87654321"     -- Product ID
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
END COMPONENT;

  signal clk:     std_logic := '1';
  signal reset:   std_logic := '1';
  signal hold:    std_logic;
  signal busy:    std_logic := '1';

  signal xdata_i: std_logic_vector(7 downto 0) := "01101001";
  signal xdata_o: std_logic_vector(9 downto 0);
  signal xtrig:   std_logic;
  signal xbusy:   std_logic := '0';

  -- UART register interface: connects to the UART
  signal ready_u : std_logic := '0';
  signal wstb_u  : std_logic;
  signal wdata_u : std_logic_vector(7 downto 0);
  signal rxfull_u: std_logic := '0';
  signal rstb_u  : std_logic;
  signal rdata_u : std_logic_vector(7 downto 0) := x"00";
  -- UART register interface: connects to the MCU
  signal ready   : std_logic;
  signal wstb    : std_logic := '0';
  signal wdata   : std_logic_vector(7 downto 0) := x"00";
  signal rxfull  : std_logic;
  signal rstb    : std_logic := '0';
  signal rdata   : std_logic_vector(7 downto 0);

  constant clk_period : time := 10 ns;          -- 100 MHz

BEGIN

  rx_proc: process (clk) is                     -- receive UART data from MCU port
  file outfile: text;
  variable f_status: FILE_OPEN_STATUS;
  variable buf_out: LINE; -- EMIT fills, LF dumps
  variable char: std_logic_vector(7 downto 0);
  begin
    if rising_edge(clk) then
      rstb <= '0';
      if rxfull = '1' then                      -- receive full
        rstb <= '1';                            -- do something with rdata
        write (buf_out, char(7 downto 0));
  	    if char(7 downto 5) /= "000" then       -- BL to FFh
          write (buf_out, character'val(to_integer(unsigned(char(7 downto 0)))));
  	    end if;
        writeline (output, buf_out);
      end if;
    end if;
  end process;

  -- Instantiate the Unit Under Test (UUT)
  uut: sfprog
    PORT MAP (
      clk => clk,  reset => reset,  hold => hold,  busy => busy,
      xdata_i => xdata_i,  xdata_o => xdata_o,  xtrig => xtrig,  xbusy => xbusy,
      ready_u => ready_u,  wstb_u => wstb_u,  wdata_u => wdata_u,
      rxfull_u => rxfull_u,  rstb_u => rstb_u,  rdata_u => rdata_u,
      ready => ready,  wstb => wstb,  wdata => wdata,
      rxfull => rxfull,  rstb => rstb,  rdata => rdata
    );

  -- System clock
  clk_process :process is
  begin
     wait for clk_period/2;  clk <= '0';
     wait for clk_period/2;  clk <= '1';
  end process;

  busy_proc: process is                         -- busy for a while after startup
  begin
     wait for clk_period*50;  busy <= '0';
     wait;
  end process;

  -- Stimulus process
  stim_proc: process is
  procedure sendchar (char: in std_logic_vector(7 downto 0)) is
  begin
    rxfull_u <= '1';  rdata_u <= char;          -- simulate UART RX character going to sfprog
    wait until rstb_u = '1';
    wait for clk_period*2;
  end procedure;
  begin
     wait for clk_period*8.2;  reset <= '0';
     sendchar(x"0D");
     sendchar(x"0A");
     wait;
  end process;

END;
