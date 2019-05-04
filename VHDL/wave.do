onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate -radix hexadecimal /m32_tb/cpu/clk
add wave -noupdate -radix hexadecimal /m32_tb/cpu/reset
add wave -noupdate -radix unsigned /m32_tb/cpu/cycles
add wave -noupdate -radix hexadecimal /m32_tb/cpu/slot
add wave -noupdate -radix hexadecimal /m32_tb/cpu/state
add wave -noupdate /m32_tb/bye
add wave -noupdate -color Magenta -radix octal /m32_tb/cpu/opcode
add wave -noupdate -color Magenta /m32_tb/cpu/name
add wave -noupdate -radix hexadecimal /m32_tb/cpu/IR
add wave -noupdate -radix hexadecimal /m32_tb/cpu/immdata
add wave -noupdate /m32_tb/cpu/skip
add wave -noupdate /m32_tb/cpu/rept
add wave -noupdate /m32_tb/cpu/held
add wave -noupdate /m32_tb/cpu/skipping
add wave -noupdate /m32_tb/cpu/repeating
add wave -noupdate -divider {ROM bus}
add wave -noupdate -color Orange -radix hexadecimal /m32_tb/cpu/caddr
add wave -noupdate -radix hexadecimal /m32_tb/cpu/cready
add wave -noupdate -radix hexadecimal /m32_tb/cpu/cdata
add wave -noupdate -divider {Block RAM}
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RAM_en
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RAM_we
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RAM_addr
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RAM_d
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RAMlanes
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RAM_q
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RAM_raddr
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RAM_waddr
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RAM_read
add wave -noupdate -radix hexadecimal /m32_tb/cpu/SPinc
add wave -noupdate -radix hexadecimal /m32_tb/cpu/SPdec
add wave -noupdate -radix hexadecimal /m32_tb/cpu/SP
add wave -noupdate -radix hexadecimal /m32_tb/cpu/SPnext
add wave -noupdate -divider Registers
add wave -noupdate -radix hexadecimal /m32_tb/cpu/T
add wave -noupdate -radix hexadecimal /m32_tb/cpu/N
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RP
add wave -noupdate -radix hexadecimal /m32_tb/cpu/UP
add wave -noupdate -radix hexadecimal /m32_tb/cpu/PC
add wave -noupdate -radix hexadecimal /m32_tb/cpu/DebugReg
add wave -noupdate -radix hexadecimal /m32_tb/cpu/CARRY
add wave -noupdate -divider State
add wave -noupdate -radix hexadecimal /m32_tb/cpu/new_T
add wave -noupdate -radix hexadecimal /m32_tb/cpu/T_src
add wave -noupdate -radix hexadecimal /m32_tb/cpu/carryin
add wave -noupdate -radix hexadecimal /m32_tb/cpu/userdata
add wave -noupdate -radix hexadecimal /m32_tb/cpu/new_N
add wave -noupdate -radix hexadecimal /m32_tb/cpu/zeroequals
add wave -noupdate -radix hexadecimal /m32_tb/cpu/TNsum
add wave -noupdate -radix hexadecimal /m32_tb/cpu/N_src
add wave -noupdate -radix hexadecimal /m32_tb/cpu/Noffset
add wave -noupdate -radix hexadecimal /m32_tb/cpu/PC_src
add wave -noupdate -radix hexadecimal /m32_tb/cpu/Port_src
add wave -noupdate -radix hexadecimal /m32_tb/cpu/WR_src
add wave -noupdate -radix hexadecimal /m32_tb/cpu/WR_addr
add wave -noupdate -radix hexadecimal /m32_tb/cpu/WR_size
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RD_size
add wave -noupdate -radix hexadecimal /m32_tb/cpu/WR_align
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RD_align
add wave -noupdate -radix hexadecimal /m32_tb/cpu/Tpacked
add wave -noupdate -divider {Stack pointer bumps}
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RPinc
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RPdec
add wave -noupdate -radix hexadecimal /m32_tb/cpu/RPload
add wave -noupdate -radix hexadecimal /m32_tb/cpu/SPload
add wave -noupdate -radix hexadecimal /m32_tb/cpu/UPload
add wave -noupdate -radix hexadecimal /m32_tb/cpu/userFNsel
add wave -noupdate -radix hexadecimal /m32_tb/cpu/ereq_i
add wave -noupdate -radix hexadecimal /m32_tb/cpu/bbout
add wave -noupdate -radix hexadecimal /m32_tb/cpu/bbin
add wave -noupdate -divider {KEY input stream}
add wave -noupdate -radix hexadecimal /m32_tb/cpu/kreq
add wave -noupdate -radix hexadecimal /m32_tb/cpu/kdata_i
add wave -noupdate -radix hexadecimal /m32_tb/cpu/kack
add wave -noupdate -divider {EMIT output stream}
add wave -noupdate -radix hexadecimal /m32_tb/cpu/ereq
add wave -noupdate -radix hexadecimal /m32_tb/cpu/edata_o
add wave -noupdate -radix hexadecimal /m32_tb/cpu/eack
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Cursor 1} {324275 ps} 0}
quietly wave cursor active 1
configure wave -namecolwidth 171
configure wave -valuecolwidth 100
configure wave -justifyvalue left
configure wave -signalnamewidth 0
configure wave -snapdistance 10
configure wave -datasetprefix 0
configure wave -rowmargin 4
configure wave -childrowmargin 2
configure wave -gridoffset 0
configure wave -gridperiod 1
configure wave -griddelta 40
configure wave -timeline 0
configure wave -timelineunits ps
update
WaveRestoreZoom {206020 ps} {410210 ps}
