default:
	gcc -g -Wall -Wextra --debug lab1b-client.c -lz -o lab1b-client
	gcc -g -Wall -Wextra --debug lab1b-server.c -lz -o lab1b-server
client:
	gcc -g -Wall -Wextra --debug lab1b-client.c -lz -o lab1b-client
server:
	gcc -g -Wall -Wextra --debug lab1b-server.c -lz -o lab1b-server
clean:
	rm -f *.o lab1b-client lab1b-server *.tar.gz
dist:
	tar -czf lab1b-005300989.tar.gz lab1b-client.c lab1b-server.c Makefile README
