#!/usr/bin/gnuplot -persist

set terminal svg size 400, 250 enhanced dashed round font 'DejaVU Serif, 9'
set style fill solid 0.77 noborder
set style line 1 lt 2 lw 3 lc rgb '#000000'
set style line 2 lt 1 lw 4 lc rgb '#000000'
set boxwidth 1 relative
set border 1+2
unset grid
#set key outside center top 
unset key
set xtics nomirror out
set ytics nomirror out
set xlabel 'Log line no.'
set ylabel 'Time [s]'

set output 'trans.svg'
set datafile separator ','
set yrange [0:*]
plot 'trans.csv' using 1:6       with lines ls 1 smooth csplines title 'Ideal time', \
     'trans.csv' using 1:($7+$8) with lines ls 2 smooth csplines title 'Replay time'
