#!/usr/bin/gnuplot -persist

set terminal svg size 400, 250 enhanced dashed round font 'DejaVU Serif, 9'
set style fill solid 0.77 noborder
set style line 1 lt 1 lw 3 lc rgb '#000000'
set style line 2 lt 1 lw 4 lc rgb '#000000'
set boxwidth 1 relative
set border 1+2
unset grid
#set key outside center top 
unset key
set xtics nomirror out
set ytics nomirror out
set xlabel 'Delay [ms]'
set ylabel 'No. of objects in buffer'

set output 'buffer.svg'
set datafile separator ','
plot 'buffer.log.save' with lines ls 1 smooth csplines
