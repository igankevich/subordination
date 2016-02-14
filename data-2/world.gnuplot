#!/usr/bin/gnuplot -persist

set terminal png transparent nocrop enhanced size 1920,1080 font "arial,8" 
set output 'world.png'
#set terminal svg dynamic enhanced
#set output 'world.svg'
unset border
set dummy u, v
set angles degrees
set parametric
set view 60, 150, 1.22, 1.26
set samples 64, 64
set isosamples 13, 13
set mapping spherical
set style data lines
set style line 1 lt 1 lw 1 lc rgb '#000000'
set style line 2 lt 1 lw 2 lc rgb '#000000'
set style line 3 lt 1 lw 1 lc rgb '#c00000'
unset xtics
unset ytics
unset ztics
set urange [ -90.0000 : 90.0000 ] noreverse nowriteback
set vrange [ 0.00000 : 360.000 ] noreverse nowriteback
#set cblabel "GeV" 
#set cbrange [ 0.00000 : 8.00000 ] noreverse nowriteback
#set colorbox user
#set colorbox vertical origin screen 0.9, 0.2, 0 size screen 0.02, 0.75, 0 front bdefault
u = 0.0
## Last datafile plotted: "srl.dat"
splot \
cos(u)*cos(v),cos(u)*sin(v),sin(u) notitle with lines ls 1, \
'../data-2/world_110m.txt' with lines ls 2, \
'graph.txt' with lines ls 3

#'world.dat' notitle with lines lt 2,\
#'srl.dat' using 3:2:(1):1:4 with labels notitle point pt 6 lw .1 left offset 1,0 font "Helvetica,7" tc pal

