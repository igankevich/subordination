#!/usr/bin/gnuplot -persist

set terminal svg size 400, 250 enhanced dashed round font 'DejaVU Serif, 11'
set style fill solid 0.77 noborder
set style line 1 lt 1 lw 2 lc rgb '#404040'
set style line 2 lt 1 lw 2 lc rgb '#404040'
set boxwidth 1 relative
set border 1+2
unset grid
#set key outside center top 
unset key
set xtics nomirror out
set ytics nomirror out
set xlabel 'Time [s]'
set ylabel 'No. of objects'

set output 'buffer-1.svg'
set xrange [0:*]
plot for [pid=7333:7342-9] '< grep ^' . pid . ' buffer-discovery.log' \
	using ($2*10**-9):6 with steps ls 1 title 'Discovery phase', \

set output 'buffer-2.svg'
set xrange [0:1.4]
plot 'buffer-socket.log' using ($2*10**-9):6 with steps ls 2 title 'Two-node pipeline'
