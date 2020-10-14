#NAME: Bonnie Liu
#EMAIL: bonnieliu2002@g.ucla.edu
#UID: 005300989

default:
	gcc -g -Wall -Wextra lab0.c -o lab0

clean:
	rm -f *.o lab0 *.tar.gz *.txt

dist:
	tar -czf lab0-005300989.tar.gz lab0.c Makefile backtrace.png breakpoint.png README

check:
	./lab0 --segfault; \
	if [[ $$? -ne 139 ]]; then \
		echo "segfault error: code should be 139"; \
	fi;
	./lab0 --catch --segfault; \
	if [[ $$? -ne 4 ]]; then \
		echo "catch error: code should be 4"; \
	fi;
	echo "We hold these truths to be self evident" > in.txt; \
	./lab0 --input in.txt; \
	if [[ $$? -ne 0 ]]; then \
		echo "input error: code should be 0"; \
	fi;
	./lab0 --input in.txt --output out.txt; \
	if [[ $$? -ne 0 ]]; then \
		echo "output error: code should be 0"; \
	fi;
