#!/usr/bin/gnuplot -persist

set terminal svg size 400, 250 enhanced dashed round font 'DejaVU Serif, 9'
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

set datafile separator ','
set output 'discovery.svg'
set xrange [2:50]
set xtics 8,8,50 add ('2' 2)
set ytics 0,1
plot \
     'discovery.log.nocache'   with lines ls 1 title 'Discovery without cache', \
     'discovery.log.nocache.2' with lines smooth unique ls 2 title 'Discovery without cache (2)',\
     'discovery.log.nocache.3' with lines smooth unique ls 3 title 'Discovery without cache (3)',\
     'discovery.log.cache'     with lines ls 4 title 'Discovery with cache', \

#reset
#set datafile separator ','
#plot \
#     '< paste -d, discovery.log.nocache discovery.log.cache' \
#	 using 1:($2/$4) with lines smooth csplines ls 1 title 'Speedup'
