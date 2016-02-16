#!/usr/bin/gnuplot -persist

set terminal svg dynamic enhanced size 1920/2,900/2
set output 'world-2d.svg'

set style data lines

set style line 1 lt 1 lw 1 lc rgb '#00a0f0'
set style line 2 lt 1 lw 3 lc rgb '#00a0f0'
set style line 8 lt 1 lw 1 lc rgb '#808080'
set style line 9 lt 1 lw 1 lc rgb '#c04000' pointtype 7 pointsize 0.25

set style arrow 1 head back filled linestyle 9 size graph 0.005,15.000,45.000

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
'../data-2/world_110m.txt' with lines ls 8 notitle, \
'../data-2/backbone-ru.dat' with lines ls 2 title 'Internet backbone', \
'../data-2/terrestrial-ru.dat' with lines ls 1 title 'Terrestrial network', \
'graph.dat' with lines linestyle 9 title 'Computed topology', \
'graph.dat' with points linestyle 9 notitle, \
'vectors.dat' with vectors arrowstyle 1 notitle
