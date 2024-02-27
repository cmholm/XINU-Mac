| Hardware definitions for KFPS

| (c) 1986, Kinetics, Inc.
| May be used but not sold without permission.

| $Header: /usr/fp/src/fps/ram.udp/RCS/kfps.h,v 1.13 86/02/21 20:40:31 tim Rel $

| CPU and speed definition
define(`clk10bus8')
define(`m4_hard_timer')

| for vaxen running 4.2 and possibly others, use flexnames
| not that the '#' should be deleted as well... it's an m4 comment delimiter
define(`flexnames')

| define the places to patch ram interrupt service
define(`m4_ieservice',	 `0x120')
define(`m4_dmtservice',	 `0x126')
define(`m4_tmoservice',	 `0x12c')
define(`m4_abservice',	 `0x132')
define(`m4_qhostservice',`0x138')
define(`m4_mhostservice',`0x13e')

| memory map
ram7	= 0x6e000
ram6	= 0x6c000
ram5	= 0x6a000
ram4	= 0x68000
ram3	= 0x66000
ram2	= 0x64000
port1	= 0x62000
port0	= 0x60000
z8530	= 0x40000
prom1	= 0x20000
prom0	= 0x00000
| scc offsets from z8530
bctl	= 0
bdata	= 1
actl	= 2
adata	= 3
sccdata	= 1		| offset to data from ctl
| chip sizes
ramsize = 0x2000
romsize = 0x2000
| stack allocation
stksiz	= 80			| initial stack size
scrlen	= 80			| scratch area for out packets
| address of table which contains 82586 initialization root to reset 82586
| and 68000 interrupt vector numbers whenever interrupts are enabled
roottab	= 0xfffffff0		| address of table 
| commands used by kfps - see "cmdidx.h" for C programs
kc_boot	= 15
kc_idle	= 30
