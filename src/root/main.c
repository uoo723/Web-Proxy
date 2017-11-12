#include <stdio.h>
#include <stdlib.h> // EXIT_FAILURE
#include <signal.h> // signal()

static void sigpipe_handler(int signum) { /* Do nothing */ }

static void error(const char *str) {
	perror(str);
	exit(EXIT_FAILURE);
}

int main() {
	printf("Hello World\n");
	signal(SIGPIPE, sigpipe_handler);
}
