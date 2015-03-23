#!/usr/bin/gnuplot -persist

set terminal svg size 400, 250 enhanced dashed round font 'DejaVU Serif, 9'
set style line 1 lt 1 lw 4 lc rgb '#000000'
set style line 2 lt 2 lw 4 lc rgb '#000000'
set border 1+2
unset grid
set key inside left top Left reverse
set xtics nomirror out
set ytics nomirror out

set xlabel 'No. of nodes'
set ylabel 'Time [s]'

set datafile separator ','
set output 'discovery.svg'
set xrange [2:32]
set xtics 4,4,32 add ('2' 2)
set ytics 0,0.3
plot \
     'discovery.log.nocache' with lines smooth csplines ls 1 title 'Discovery without cache', \
     'discovery.log.cache'   with lines smooth csplines ls 2 title 'Discovery with cache', \

#reset
#set datafile separator ','
#plot \
#     '< paste -d, discovery.log.nocache discovery.log.cache' \
#	 using 1:($2/$4) with lines smooth csplines ls 1 title 'Speedup'
