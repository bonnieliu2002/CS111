# NAME: Bonnie Liu
# EMAIL: bonnieliu2002@g.ucla.edu
# ID: 005300989

default:
	gcc -Wall -Wextra -lmraa -o lab4b lab4b.c -lm
clean:
	rm -f *.o lab4b *.tar.gz
dist:
	tar -czf lab4b-005300989.tar.gz lab4b.c Makefile README
check:
	./lab4b --foo; \
	if [ $$? -eq 0 ]; then \
		echo "bad argument: code should be nonzero"; \
	fi;
