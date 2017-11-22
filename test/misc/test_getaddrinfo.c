#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <stdio.h>

void test_getaddrinfo(void **state) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	struct sockaddr_in *sockaddr;
	char ip[INET_ADDRSTRLEN] = {0};

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int s = getaddrinfo("www.google.com", "80", &hints, &result);
	if (s) {
		fail_msg("getaddrinfo: %s\n", gai_strerror(s));
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		memset(ip, 0, INET_ADDRSTRLEN);
		sockaddr = (struct sockaddr_in *) rp->ai_addr;
		inet_ntop(AF_INET, &sockaddr->sin_addr, ip, INET_ADDRSTRLEN);
		// printf("ip: %s\n", ip);
		// printf("port: %d\n", ntohs(sockaddr->sin_port));
	}

	freeaddrinfo(result);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_getaddrinfo),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
