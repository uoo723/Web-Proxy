#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>

#include <thpool/thpool.h>
#include <cache/lru.h>
#include <http/http_common.h>
#include <http/http_request.h>
#include <http/http_response.h>
#include <http/http_log.h>

#define BACKLOG 10
#define THREAD_NUM 8
#define CACHE_SIZE (512 * 1024 * 1024)  // 5MB
#define OBJECT_SIZE (512 * 1024)        // 5KB

static lru_cache_t *cache;

typedef struct {
    int sockfd;
    char ip[INET_ADDRSTRLEN];
} args_t;

typedef threadpool threadpool_t;

static __thread size_t sock_snd_buf_size;
static __thread size_t sock_rcv_buf_size;

static void sigpipe_handler(int signum) { /* Do nothing */ }

static void error(const char *str) {
	perror(str);
	exit(EXIT_FAILURE);
}

// Called from thread.
static bool rcv_request(int sockfd, http_parser *parser,
    http_parser_settings *settings, http_response_t *response) {
    http_request_t *request;
    int nparsed, recved;
    char *buf = malloc(sock_rcv_buf_size);
    if (!buf) {
        perror("buf cannot be initialized");
        return false;
    }

    while (1) {
        if ((recved = recv(sockfd, buf, sock_rcv_buf_size, 0)) < 0) {
            if (errno == EPIPE) {
                break;
            } else if (errno == EPROTOTYPE) {
                continue;
            } else {
                perror("recv failed");
                free(buf);
                return false;
            }
        }

        nparsed = http_parser_execute(parser, settings, buf, recved);
        if (parser->upgrade) {
            fprintf(stderr, "upgrade is not implemented\n");
        }

        if (nparsed != recved) {
            fprintf(stderr, "nparsed != recved\n");
            free(buf);
            return false;
        }

        request = (http_request_t *) parser->data;
        if (request->on_message_completed) break;
    }

    free(buf);
    return true;
}

static void thread_main(void *data) {
    args_t *args = (args_t *) data;
    http_parser *parser = malloc(sizeof(http_parser));
    http_request_t *request = malloc(sizeof(http_request_t));
    http_response_t *response = malloc(sizeof(http_response_t));
    http_parser_settings settings;

    if (!parser) {
        perror("parser cannot be initialized");
        return;
    }

    if (!request) {
        perror("request cannot be initialized");
        free(parser);
        return;
    }

    if (!response) {
        perror("response cannot be initialized");
        free(parser);
        free(request);
        return;
    }

    http_parser_settings_init(&settings);
    settings.on_url = request_on_url_cb;
    settings.on_header_field = request_on_header_field_cb;
    settings.on_header_value = request_on_header_value_cb;
    settings.on_body = request_on_body_cb;
    settings.on_message_complete = request_on_message_complete_cb;

    http_parser_init(parser, HTTP_REQUEST);
    parser->data = request;

    memset(request, 0, sizeof(http_request_t));
    memset(response, 0, sizeof(http_response_t));

    strcpy(request->ip, args->ip);

    if (sock_snd_buf_size == 0) {
        socklen_t opt_size = sizeof(sock_snd_buf_size);
        if (getsockopt(args->sockfd, SOL_SOCKET, SO_SNDBUF, &sock_snd_buf_size,
            &opt_size)) {
            perror("getsockopt failed");
            goto release;
        }

        if (sock_snd_buf_size == 0) {
            fprintf(stderr, "sock_snd_buf_size is 0. starnge!\n");
            goto release;
        }
    }

    if (sock_rcv_buf_size == 0) {
        socklen_t opt_size = sizeof(sock_rcv_buf_size);
        if (getsockopt(args->sockfd, SOL_SOCKET, SO_RCVBUF, &sock_rcv_buf_size,
            &opt_size)) {
            perror("getsockopt failed");
            goto release;
        }

        if (sock_rcv_buf_size == 0) {
            fprintf(stderr, "sock_rcv_buf_size is 0 starnge!\n");
            goto release;
        }
    }

    if (!rcv_request(args->sockfd, parser, &settings, response)) {
        fprintf(stderr, "rcv_request() failed\n");
    } else {
        log_http_request(request, response);
    }

release:
    free(parser);
    free(request);
    free(response);
    close(args->sockfd);
    free(args);
}

int main(int argc, char *argv[]) {
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigpipe_handler;
    sigaction(SIGPIPE, &act, NULL);     // Install handler for SIGPIPE

	threadpool_t thpool;
	int sockfd;
	int opt;
	int port;
	socklen_t opt_size = sizeof(opt);

	if (argc != 2) {
		fprintf(stderr, "Usage %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[1]);

	if (!(port >= 1024 && port <= 65535)) {
		fprintf(stderr, "port number must be in (1024 <= port <= 65535)\n");
		exit(EXIT_FAILURE);
	}

	// Create thread pool
	if ((thpool = thpool_init(THREAD_NUM)) == NULL) {
		error("thpool initialization failed");
	}

    // Create lru cache object
    if ((cache = lru_cache_init(CACHE_SIZE, OBJECT_SIZE)) == NULL) {
        error("cache initialization failed");
    }

	// Create ipv4 TCP socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        error("socket failed");
    }

	opt = 1;
    // Set socket option to be reuse address to avoid error "Address already in use"
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, opt_size) < 0) {
        error("setsockopt(SO_REUSEADDR) failed");
    }

	// Required on Linux >= 3.9
    #ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, opt_size) < 0) {
        error("setsockopt(SO_REUSEPORT) failed");
    }
    #endif

	struct sockaddr_in server_addr;
    // Set IP socket address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the address to the socket
    if (bind(sockfd, (struct sockaddr *) &server_addr,
        sizeof(server_addr)) < 0) {
        error("bind failed");
    }

    // Listen for connections on the socket
    if (listen(sockfd, BACKLOG) < 0) {
        error("listen failed");
    }

    char ip_str[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &server_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    printf("running on %s:%d\n", ip_str, port);
    fflush(stdout);

    if (!http_log_set_file("proxy.log")) {      // Set log file
        error("http_log_set_file() failed");
    }

    while (1) {
        args_t *args = malloc(sizeof(args_t));
        struct sockaddr_in client_addr;
        socklen_t client_addrlen = sizeof(struct sockaddr_in);

        if (!args) {
            error("args cannot be initiailized");
        }

        // Accept a connection on the socket
        if ((args->sockfd = accept(sockfd, (struct sockaddr *) &client_addr,
            &client_addrlen)) < 0) {
            error("accept failed");
        }

        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        strcpy(args->ip, ip_str);

        thpool_add_work(thpool, (void *) thread_main, (void *) args);
    }

    close(sockfd);
    thpool_destroy(thpool);

    return 0;
}
