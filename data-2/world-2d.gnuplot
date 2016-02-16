#!/usr/bin/gnuplot -persist

set terminal svg dynamic enhanced size 1920/2,900/2
set output 'world-2d.svg'

set style data lines
set style line 1 lt 1 lw 1 lc rgb '#000000'
set style line 2 lt 1 lw 2 lc rgb '#00a0f0'
set style line 3 lt 1 lw 1 lc rgb '#40c000'

#unset border
#unset xtics
#unset ytics

# World
#set xrange [ -180 : 180 ]
#set yrange [  -90 : 90  ]

# Russia
set xrange [   18 : 180 ]
set yrange [   30 : 90  ]

plot \
'../data-2/world_110m.txt' with lines ls 1 notitle, \
'../data-2/rostelecom.dat' with lines ls 2 notitle, \
'graph.txt' with lines ls 3 title 'Computed topology'
