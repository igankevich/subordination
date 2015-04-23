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
set xrange [2:50]
set xtics 8,8,50 add ('2' 2)
set ytics 0,1
#plot 'discovery.log.tree'   with lines ls 1 title 'IP mapping'

plot \
     'discovery.log.nocache'   with lines ls 1 title 'Full scan', \
     'discovery.log.nocache.3' with lines smooth unique ls 3 title 'IP mapping',\

#     'discovery.log.cache'     with lines ls 4 title 'Using cached topology', \

#     'discovery.log.nocache.2' with lines smooth unique ls 2 title 'qq',\
#reset
#set datafile separator ','
#plot \
#     '< paste -d, discovery.log.nocache discovery.log.cache' \
#	 using 1:($2/$4) with lines smooth csplines ls 1 title 'Speedup'
