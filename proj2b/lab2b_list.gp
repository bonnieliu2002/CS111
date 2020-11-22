#! /usr/bin/gnuplot
#
# purpose: generate data reduction graphs for the multi-threaded list project
#
# input: lab2b_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
# 	4. # lists
#	5. # operations performed (threads * iterations * (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#
# output:
#	lab2b_list-1.png ...
#	lab2b_list-2.png ...
# 	lab2b_list-3.png ...
# 	lab2b_list-4.png ...
#
# Note:
#	Managing data is simplified by keeping all of the results in a single
#	file. But this means the individual graphing commands have to grep to
#	select only the data they want.
#

# general plot parameters
set terminal png
set datafile separator ","

# operations per second for each synchronization method
set title "List-1: Total Number of Operations per Second vs Threads"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:]
set ylabel "Total Number of Operations per Second"
set logscale y 10
set output 'lab2b-1.png'

plot \
	"< grep 'list-none-m,[0-9]*,1000,1,' lab2_list.csv" using ($2):(1000000000/($7)) \
		with linespoints lc rgb "red" title "Mutex, Iterations=1000", \
	"< grep 'list-none-s,[0-9]*,1000,1,' lab2_list.csv" using ($2):(1000000000/($7)) \
		with linespoints lc rgb "green" title "Spin-Lock, Iterations=1000"
#
# "no valid points" is possible if even a single iteration can't run
#

# average wait-for-lock
set title "List-2: Average Wait-for-Lock Time and Average Time Per Operation"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange[0.75:]
set ylabel "Time (ns)"
set logscale y 10
set output 'lab2b-2.png'

plot \
	"< grep 'list-none-m,[0-9]*,1000,1,' lab2_list.csv" using ($2):($7) \
		with linespoints lc rgb "red" title "Average Time Per Operation", \
	"< grep 'list-none-m,[0-9]*,1000,1,' lab2_list.csv" using ($2):($8) \
		with linespoints lc rgb "green" title "Average Wait-for-Lock Time"
