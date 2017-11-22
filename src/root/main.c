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
#include <netdb.h>
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

static void sigpipe_handler(int signum) { /* Do nothing */ }

static void error(const char *str) {
	perror(str);
	exit(EXIT_FAILURE);
}

static size_t get_sock_buf_size(int sockfd, int optname) {
    size_t ret = 0;
    socklen_t opt_size = sizeof(ret);
    if (getsockopt(sockfd, SOL_SOCKET, optname, &ret, &opt_size)) {
        perror("getsockopt() failed");
        return 0;
    }

    return ret;
}

// get addrinfo struct from hostname. result must be free'd using freeaddrinfo() after use.
static bool get_addrinfo(const char *hostname, const char *port,
        struct addrinfo **result) {
    struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

	int s = getaddrinfo(hostname, port, &hints, result);
	if (s) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        *result = NULL;
        return false;
	}

    return true;
}

// Called from thread. receive request from client.
// The server side of proxy.
static bool rcv_request(int sockfd, http_request_t *request) {
    http_parser *parser;
    http_parser_settings settings;
    int nparsed, recved;
    size_t sock_rcv_buf_size;
    char *buf;

    sock_rcv_buf_size = get_sock_buf_size(sockfd, SO_RCVBUF);
    if (sock_rcv_buf_size == 0) {
        fprintf(stderr, "sock_rcv_buf_size is 0\n");
        return false;
    }

    parser = malloc(sizeof(http_parser));
    if (!parser) {
        perror("parser cannot be initialized");
        return false;
    }

    buf = malloc(sock_rcv_buf_size);
    if (!buf) {
        perror("buf cannot be initialized");
        free(parser);
        return false;
    }

    http_parser_settings_init(&settings);
    settings.on_url = request_on_url_cb;
    settings.on_header_field = request_on_header_field_cb;
    settings.on_header_value = request_on_header_value_cb;
    settings.on_body = request_on_body_cb;
    settings.on_message_complete = request_on_message_complete_cb;

    http_parser_init(parser, HTTP_REQUEST);
    parser->data = request;

    while (!request->on_message_completed) {
        if ((recved = recv(sockfd, buf, sock_rcv_buf_size, 0)) == -1) {
            if (errno == EPIPE) {
                break;
            } else if (errno == EPROTOTYPE) {
                continue;
            } else {
                perror("recv failed");
                free(parser);
                free(buf);
                return false;
            }
        }

        nparsed = http_parser_execute(parser, &settings, buf, recved);
        if (parser->upgrade) {
            fprintf(stderr, "upgrade is not implemented\n");
            free(parser);
            free(buf);
            return false;
        }

        if (nparsed != recved) {
            fprintf(stderr, "nparsed != recved\n");
            free(parser);
            free(buf);
            return false;
        }
    }

    free(parser);
    free(buf);
    return true;
}

// Called from thread. send request to origin server. and receive its response.
// The client side of proxy.
static bool send_request(http_request_t *request, http_response_t *response) {
    int sockfd;
    struct addrinfo *addrinfo, *rp;
    size_t sock_snd_buf_size;
    size_t sock_rcv_buf_size;
    size_t remaining, offset, sent;
    size_t recved, nparsed;
    size_t req_size;
    char *buf, *req_str;
    http_parser *parser;
    http_parser_settings settings;

    if (request->host == NULL || strcmp(request->host, "") == 0) {
        fprintf(stderr, "hostname is not specified\n");
        return false;
    }

    if (!get_addrinfo(request->host, request->port, &addrinfo)) {
        fprintf(stderr, "get_addrinfo() failed\n");
        return false;
    }

    for (rp = addrinfo; rp != NULL; rp = rp->ai_next) {
        if ((sockfd = socket(rp->ai_family, rp->ai_socktype,
            rp->ai_protocol)) == -1)
            continue;

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;

        close(sockfd);
    }

    if (rp == NULL) {
        fprintf(stderr, "Could not connect\n");
        return false;
    }

    freeaddrinfo(addrinfo);

    sock_snd_buf_size = get_sock_buf_size(sockfd, SO_SNDBUF);
    sock_rcv_buf_size = get_sock_buf_size(sockfd, SO_RCVBUF);

    if (sock_snd_buf_size == 0 || sock_rcv_buf_size == 0) {
        fprintf(stderr, "sock_snd_buf_size is 0\n");
        close(sockfd);
        return false;
    }

    buf = malloc(sock_snd_buf_size);
    if (!buf) {
        perror("buf cannot be initialized");
        close(sockfd);
        return false;
    }

    if (!make_request_string(request, &req_str, &req_size)) {
        fprintf(stderr, "make_request_string() failed\n");
        free(buf);
        close(sockfd);
        return false;
    }

    remaining = req_size;
    offset = 0;

    while (remaining != 0) {
        size_t buf_size;

        if (remaining <= sock_snd_buf_size) {
            buf_size = remaining;
        } else {    // remaining > sock_snd_buf_size
            buf_size = sock_snd_buf_size;
        }

        memcpy(buf, req_str + offset, buf_size);
        if ((sent = send(sockfd, buf, buf_size, 0)) == -1) {
            perror("send() failed");
            free(buf);
            free(req_str);
            close(sockfd);
            return false;
        }

        remaining -= sent;
        offset += sent;
    }

    free(req_str);

// =========================================================================

    // rcv
    buf = realloc(buf, sock_rcv_buf_size);

    if (!buf) {
        perror("buf cannot be initialized");
        close(sockfd);
        return false;
    }

    parser = malloc(sizeof(http_parser));

    if (!parser) {
        perror("parser cannot be initialized");
        free(buf);
        close(sockfd);
        return false;
    }

    http_parser_settings_init(&settings);
    settings.on_header_field = response_on_header_field_cb;
    settings.on_header_value = response_on_header_value_cb;
    settings.on_body = response_on_body_cb;
    settings.on_message_complete = response_on_message_complete_cb;

    http_parser_init(parser, HTTP_RESPONSE);
    parser->data = response;

    while (!response->on_message_completed) {
        if ((recved = recv(sockfd, buf, sock_rcv_buf_size, 0)) == -1) {
            if (errno == EPIPE) {
                break;
            } else if (errno == EPROTOTYPE) {
                continue;
            } else {
                perror("recv failed");
                free(parser);
                free(buf);
                close(sockfd);
                return false;
            }
        }

        nparsed = http_parser_execute(parser, &settings, buf, recved);

        if (nparsed != recved) {
            fprintf(stderr, "nparsed != recved\n");
            free(parser);
            free(buf);
            close(sockfd);
            return false;
        }
    }

    free(buf);
    close(sockfd);
    return true;
}

// Called from thread. send response received by origin server to client.
static bool send_response(int sockfd, http_response_t *response) {
    size_t sock_snd_buf_size;
    size_t sent, remaining, offset;
    size_t res_size;
    char *buf;
    char *res_str;

    sock_snd_buf_size = get_sock_buf_size(sockfd, SO_SNDBUF);
    if (sock_snd_buf_size == 0) {
        fprintf(stderr, "sock_snd_buf_size is 0\n");
        return false;
    }

    buf = malloc(sock_snd_buf_size);
    if (!buf) {
        perror("buf cannot be initialized");
        return false;
    }

    if (!make_response_string(response, &res_str, &res_size)) {
        fprintf(stderr, "make_response_string() failed\n");
        free(buf);
        return false;
    }

    remaining = res_size;
    offset = 0;

    while (remaining != 0) {
        size_t buf_size;

        if (remaining <= sock_snd_buf_size) {
            buf_size = remaining;
        } else {
            buf_size = sock_snd_buf_size;
        }

        memcpy(buf, res_str + offset, buf_size);
        if ((sent = send(sockfd, buf, buf_size, 0)) == -1) {
            perror("send() failed");
            free(buf);
            free(res_str);
            return false;
        }

        remaining -= sent;
        offset += sent;
    }

    free(buf);
    free(res_str);
    return true;
}

static void thread_main(void *data) {
    args_t *args = (args_t *) data;
    http_request_t *request = malloc(sizeof(http_request_t));
    http_response_t *response = malloc(sizeof(http_response_t));

    if (!request) {
        perror("request cannot be initialized");
        return;
    }

    if (!response) {
        perror("response cannot be initialized");
        free(request);
        return;
    }

    memset(request, 0, sizeof(http_request_t));
    memset(response, 0, sizeof(http_response_t));

    strcpy(request->ip, args->ip);

    if (!rcv_request(args->sockfd, request)) {
        fprintf(stderr, "rcv_request() failed\n");
    } else if (!send_request(request, response)) {
        fprintf(stderr, "send_request() failed\n");
    } else if (!send_response(args->sockfd, response)){
        fprintf(stderr, "send_response() failed\n");
    } else {
        log_http_request(request, response);
    }

    if (request->content_length != 0) {
        free(request->content);
    }

    if (response->content_length != 0) {
        free(response->content);
    }

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
