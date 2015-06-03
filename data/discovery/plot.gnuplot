#!/usr/bin/gnuplot -persist

set terminal svg size 400, 250 enhanced dashed round font 'DejaVU Serif, 10'
set style line 1 lt 1 lw 3 lc rgb '#000000'
set style line 2 lt 2 lw 3 lc rgb '#000000'
set style line 3 lt 3 lw 3 lc rgb '#000000'
set style line 4 lt 4 lw 3 lc rgb '#000000'
set border 1+2
unset grid
set key inside left top Left reverse
set xtics nomirror out
set ytics nomirror out

set xlabel 'No. of nodes'
set ylabel 'Time [s]'

#set xlabel 'Кол-во узлов'
#set ylabel 'Время [с]'

set datafile separator ','
set output 'discovery.svg'
set xrange [100:400]
set yrange [0:4]
set xtics 100,100
set ytics 0,1

plot 'scalability.csv' using 1:($2*10**-9) with lines ls 1 notitle
