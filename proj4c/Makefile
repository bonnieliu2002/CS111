default:
	gcc -Wall -Wextra -lmraa -o lab4c_tcp lab4c_tcp.c -lm
	gcc -Wall -Wextra -lmraa -lssl -lcrypto -o lab4c_tls lab4c_tls.c -lm
clean:
	rm -f *.o lab4c_tcp lab4c_tls *.tar.gz
dist:
	tar -czf lab4c-005300989.tar.gz lab4c_tcp.c lab4c_tls.c README Makefile
