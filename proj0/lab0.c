#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <signal.h>

void signal_handler() {
	fprintf(stderr, "segmentation fault caught\n");
	exit(4);
}

int main(int argc, char* argv[]) {
	static int inputFlag;
	static int outputFlag;
	static int segmentFlag;
	static int catchFlag;

	char* inputFile = NULL;
	char* outputFile = NULL;

	// define option struct
	// if flag is null, will return val (in this case, val equals either 1, 2, 3, or 4)
	// else will return 0
	static struct option long_options[] = {
                {"input", required_argument, &inputFlag, 1},
                {"output", required_argument, &outputFlag, 2},
                {"segfault", no_argument, &segmentFlag, 3},
                {"catch", no_argument, &catchFlag, 4},
                {0, 0, 0, 0}
        };

	// repeatedly read in arguments
	while (1) {
		// will be updated depending on argument that is passed in
		int option_index = 0;

		// use getopt_long for argument handling
		int c = getopt_long(argc, argv, "", long_options, &option_index);

		// no more options
		if (c == -1)
			break;

		// unrecognized argument
		if (c != 0) {
			printf("error: unrecognized argument; correct usage: --input=filename, --output=filename, --segfault, --catch\n");
			exit(1);
		}

		// read in input file
		if (option_index == 0)
			inputFile = optarg;
		// read in output file
		else if (option_index == 1)
			outputFile = optarg;	
	}

	// if inputFile is not NULL, that means an argument was passed in, and we want to do input redirection
	if (inputFile) {
		int ifd = open(inputFile, O_RDONLY);
		if (ifd >= 0) {
			close(0);
			dup(ifd);
			close(ifd);
		}
		else { // problem opening inputFile
			fprintf(stderr, "error opening file %s as --input argument: no such file exists\n", inputFile);
			exit(2);
		}
	}

	// if outputFile is not NULL, that means an argument was passed in, and we want to do output redirection
	if (outputFile) {
		int ofd = creat(outputFile, 0666); // read-write permissions for owner, group, and other
		if (ofd >= 0) {
			close(1);
			dup(ofd);
			close(ofd);
		}
		else { // problem creating output file
			fprintf(stderr, "error creating file %s as --output argument\n", outputFile);
			exit(3);
		}
	}

	// if --catch is passed in as argument, use signal(2) to register SIGSEGV handler
	if (catchFlag) {
                signal(SIGSEGV, signal_handler);
        }

	// if --segfault is passed in as argument, force a segmentation fault by dereferencing NULL
	if (segmentFlag) {
                char* segFault = NULL;
                *segFault = 1;
	}

	// read from stdin and write to stdout one character at a time
	char* buffer = (char*) malloc(sizeof(char));
	while (1) {
		int nread = read(0, buffer, sizeof(char));
		if (nread == 0) // EOF
			break;
		else if (nread < 0) { // error encountered
			fprintf(stderr, "error reading stdin: %s\n", strerror(errno));
			exit(errno);
		}
		else {
			int nwrite = write(1, buffer, sizeof(char));
			if (nwrite < 0) { // error encountered
				fprintf(stderr, "error writing to file %s: %s\n", outputFile, strerror(errno));
			exit(errno);
			}
		}
	}

	// everything worked properly!
	exit(0);
}
