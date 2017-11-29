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

// #include <thpool/thpool.h>
#include <cache/lru.h>
#include <http/http_common.h>
#include <http/http_request.h>
#include <http/http_response.h>
#include <http/http_log.h>

#define BACKLOG 10
// #define THREAD_NUM 8
#define BUFFER_SIZE (80 * 1024)
#define CACHE_SIZE (5 * 1024 * 1024)  // 5MB
#define OBJECT_SIZE (512 * 1024)        // 512KB

// key: <request->host>:<requset->port>/<request->path>
// value: entire response
static lru_cache_t *cache;

typedef struct {
    int sockfd;
    char ip[INET_ADDRSTRLEN];
} args_t;

static void error(const char *str) {
	perror(str);
	exit(EXIT_FAILURE);
}

static void print_cache_status() {
    size_t total_memory = cache->total_memory;
    size_t free_memory = cache->free_memory;
    size_t in_use_memory = total_memory - free_memory;

    total_memory /= 1024 * 1024;
    in_use_memory /= 1024;

    printf("%luKB/%luMB\n", in_use_memory, total_memory);
    fflush(stdout);
}

// get addrinfo struct from hostname. result must be free'd using
// freeaddrinfo() after use.
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
// If cache is existed, hit is set to true. And cache is sent directly to client.
// When client request connect method upgrade is set to true.
static bool rcv_request(int sockfd, http_request_t *request,
        http_response_t *response, bool *upgrade, bool *hit) {
    http_parser *parser;
    http_parser_settings settings;
    int nparsed, recved;
    char buf[BUFFER_SIZE] = {0};
    char *key, *value;
    size_t key_len, value_len;
    size_t sent, remaining, offset;
    lru_cache_error err;

    parser = malloc(sizeof(http_parser));
    if (!parser) {
        perror("parser cannot be initialized");
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
    *upgrade = false;

    while (!request->on_message_completed) {
        if ((recved = recv(sockfd, buf, BUFFER_SIZE, 0)) == -1) {
            perror("recv() failed");
            free(parser);
            return false;
        }

        nparsed = http_parser_execute(parser, &settings, buf, recved);
        if (parser->upgrade) {
            // fprintf(stderr, "HTTP tunnel is not implemented\n");
            free(parser);
            *upgrade = true;
            return true;
        }

        if (nparsed != recved) {
            fprintf(stderr, "nparsed != recved\n");
            free(parser);
            return false;
        }
    }

    key_len = strlen(request->host) + strlen(request->port)
        + 1 /* ":" */ + strlen(request->path) + 1;
    key = malloc(key_len);

    if (!key) {
        perror("key cannot be initialized");
        free(parser);
        return false;
    }

    strcpy(key, request->host);
    strcat(key, ":");
    strcat(key, request->port);
    strcat(key, request->path);

    err = lru_cache_get(cache, key, key_len, (void **) &value, &value_len);
    if (err != LRU_CACHE_NO_ERROR) {
        fprintf(stderr, "lru_cache_get() failed\n");
        free(parser);
        free(key);
        return false;
    }

    free(key);

    *hit = value ? true : false;
    if (*hit) {    // Hit.
        remaining = value_len;
        offset = 0;

        while (remaining != 0) {
            size_t buf_size;

            if (remaining <= BUFFER_SIZE) {
                buf_size = remaining;
            } else {
                buf_size = BUFFER_SIZE;
            }

            memcpy(buf, value + offset, buf_size);
            if ((sent = send(sockfd, buf, buf_size, 0)) == -1) {
                perror("send() failed");
                free(parser);
                return false;
            }

            remaining -= sent;
            offset += sent;
        }

        http_parser_settings_init(&settings);
        settings.on_header_field = response_on_header_field_cb;
        settings.on_header_value = response_on_header_value_cb;
        settings.on_body = response_on_body_cb;
        settings.on_message_complete = response_on_message_complete_cb;

        http_parser_init(parser, HTTP_RESPONSE);
        parser->data = response;
        http_parser_execute(parser, &settings, value, value_len);
    }

    free(parser);
    return true;
}

// Called from thread. send request to origin server. and receive its response.
// The client side of proxy.
// sockfd will be used in rcv_and_send_response().
static bool send_request(http_request_t *request, http_response_t *response,
    int *sockfd) {
    struct addrinfo *addrinfo, *rp;
    size_t remaining, offset, sent;
    size_t req_size;
    char *req_str;
    char buf[BUFFER_SIZE] = {0};

    if (request->host == NULL || strcmp(request->host, "") == 0) {
        fprintf(stderr, "hostname is not specified\n");
        return false;
    }

    if (!get_addrinfo(request->host, request->port, &addrinfo)) {
        fprintf(stderr, "get_addrinfo() failed\n");
        return false;
    }

    for (rp = addrinfo; rp != NULL; rp = rp->ai_next) {
        if ((*sockfd = socket(rp->ai_family, rp->ai_socktype,
            rp->ai_protocol)) == -1)
            continue;

        if (connect(*sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;

        close(*sockfd);
    }

    if (rp == NULL) {
        fprintf(stderr, "Could not connect\n");
        return false;
    }

    freeaddrinfo(addrinfo);

    if (!make_request_string(request, &req_str, &req_size)) {
        fprintf(stderr, "make_request_string() failed\n");
        close(*sockfd);
        return false;
    }

    remaining = req_size;
    offset = 0;

    while (remaining != 0) {
        size_t buf_size;

        if (remaining <= BUFFER_SIZE) {
            buf_size = remaining;
        } else {    // remaining > sock_snd_buf_size
            buf_size = BUFFER_SIZE;
        }

        memcpy(buf, req_str + offset, buf_size);
        if ((sent = send(*sockfd, buf, buf_size, 0)) == -1) {
            perror("send() failed");
            free(req_str);
            close(*sockfd);
            return false;
        }

        remaining -= sent;
        offset += sent;
    }

    free(req_str);
    return true;
}

// Called from thread.
// receive response from origin server and forward to client.
// server_sockfd was created in send_request.
// We do not need to close client_sockfd, becuase it will be closed end of
// the thread_main().
// Store response in cache.
static bool rcv_and_send_response(int server_sockfd, int client_sockfd,
    http_request_t *request, http_response_t *response) {
    http_parser *parser;
    http_parser_settings settings;
    size_t recved, nparsed, sent;
    char buf[BUFFER_SIZE] = {0};
    char *key, *value;
    size_t key_len, value_len, offset;
    lru_cache_error err;

    parser = malloc(sizeof(http_parser));
    if (!parser) {
        perror("parser cannot be initialized");
        close(server_sockfd);
        return false;
    }

    http_parser_settings_init(&settings);
    settings.on_header_field = response_on_header_field_cb;
    settings.on_header_value = response_on_header_value_cb;
    settings.on_body = response_on_body_cb;
    settings.on_message_complete = response_on_message_complete_cb;

    http_parser_init(parser, HTTP_RESPONSE);
    parser->data = response;
    value = NULL;
    value_len = 0;
    offset = 0;

    while (!response->on_message_completed) {
        if ((recved = recv(server_sockfd, buf, BUFFER_SIZE, 0)) == -1) {
            perror("recv() failed");
            free(parser);
            close(server_sockfd);
            return false;
        }

        nparsed = http_parser_execute(parser, &settings, buf, recved);

        if (nparsed != recved) {
            fprintf(stderr, "nparsed != recved\n");
            free(parser);
            close(server_sockfd);
            return false;
        }

        value_len += recved;

        if (!value) {
            value = malloc(value_len);
        } else {
            value = realloc(value, value_len);
        }

        if (!value) {
            perror("value cannot be initialized");
            fprintf(stderr, "nparsed != recved\n");
            free(parser);
            close(server_sockfd);
            return false;
        }

        memcpy(value + offset, buf, recved);

        offset += recved;

        if ((sent = send(client_sockfd, buf, recved, 0)) == -1) {
            perror("send() failed");
            free(parser);
            free(value);
            close(server_sockfd);
            return false;
        }
    }

    free(parser);

    if (value_len <= OBJECT_SIZE) {
        key_len = strlen(request->host) + strlen(request->port)
            + 1 /* ":" */ + strlen(request->path) + 1;
        key = malloc(key_len);
        if (!key) {
            perror("key cannot be initialized");
            free(value);
            close(server_sockfd);
            return false;
        }

        strcpy(key, request->host);
        strcat(key, ":");
        strcat(key, request->port);
        strcat(key, request->path);

        // Note: Do not free value if err == LRU_CACHE_NO_ERROR.
        err = lru_cache_set(cache, key, key_len, value, value_len);
        if (err != LRU_CACHE_NO_ERROR) {
            fprintf(stderr, "lru_cache_set() failed\n");
            free(key);
            free(value);
            close(server_sockfd);
            return false;
        }

        printf("caching - key: %s, ", key);
        print_cache_status();

        free(key);
    }

    close(server_sockfd);
    return true;
}

// This function is called for HTTPS connection.
// The proxy sever just forward to client's tcp stream to origin server
// and vice versa unless tcp connection is closed.
static void http_tunnel(int client_sockfd, http_request_t *request) {
    struct addrinfo *addrinfo, *rp;
    http_response_t *response;
    char *res_str;
    char buf[BUFFER_SIZE] = {0};
    size_t res_len, remaining, offset, bytes /* using at recv and send */;
    int server_sockfd;

    if (request->host == NULL || strcmp(request->host, "") == 0) {
        fprintf(stderr, "hostname is not specified\n");
        close(client_sockfd);
        return;
    }

    if (!get_addrinfo(request->host, request->port, &addrinfo)) {
        fprintf(stderr, "get_addrinfo() failed\n");
        close(client_sockfd);
        return;
    }

    for (rp = addrinfo; rp != NULL; rp = rp->ai_next) {
        if ((server_sockfd = socket(rp->ai_family, rp->ai_socktype,
            rp->ai_protocol)) == -1)
            continue;

        if (connect(server_sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;

        close(server_sockfd);
    }

    if (rp == NULL) {
        fprintf(stderr, "Could not connect\n");
        close(client_sockfd);
        return;
    }

    freeaddrinfo(addrinfo);

    if ((response = init_http_response(0)) == NULL) {
        perror("response cannot be initialized");
        close(client_sockfd);
        close(server_sockfd);
        return;
    }

    response->http_major = request->http_major;
    response->http_minor = request->http_minor;
    response->status = HTTP_STATUS_OK;

    if (!make_response_string(response, &res_str, &res_len)) {
        fprintf(stderr, "make_response_string() failed\n");
        free(response);
        close(client_sockfd);
        close(server_sockfd);
        return;
    }
    free(response);

    remaining = res_len;
    offset = 0;

    while (remaining != 0) {
        size_t buf_size;

        if (remaining <= BUFFER_SIZE) {
            buf_size = remaining;
        } else {
            buf_size = BUFFER_SIZE;
        }

        memcpy(buf, res_str + offset, buf_size);
        if ((bytes = send(client_sockfd, buf, buf_size, 0)) == -1) {
            perror("send() failed");
            free(res_str);
            close(client_sockfd);
            close(server_sockfd);
            return;
        }

        remaining -= bytes;
        offset += bytes;
    }
    free(res_str);

    while (1) {
        while (1) {
            if ((bytes = recv(client_sockfd, buf, BUFFER_SIZE,
                    MSG_DONTWAIT | MSG_NOSIGNAL)) == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    if (errno == EPIPE) {
                        close(server_sockfd);
                    } else {
                        perror("recv(client_sockfd) failed");
                        close(server_sockfd);
                        close(client_sockfd);
                    }
                    return;
                }
            }

            if ((bytes = send(server_sockfd, buf, bytes, MSG_NOSIGNAL)) == -1) {
                if (errno == EPIPE) {
                    close(client_sockfd);
                } else {
                    perror("send(server_sockfd) failed");
                    close(server_sockfd);
                    close(client_sockfd);
                }
                return;
            }
        }

        while (1) {
            if ((bytes = recv(server_sockfd, buf, BUFFER_SIZE,
                    MSG_DONTWAIT | MSG_NOSIGNAL)) == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    if (errno == EPIPE) {
                        close(client_sockfd);
                    } else {
                        perror("recv(server_sockfd) failed");
                        close(server_sockfd);
                        close(client_sockfd);
                    }
                    return;
                }
            }

            if ((bytes = send(client_sockfd, buf, bytes, MSG_NOSIGNAL)) == -1) {
                if (errno == EPIPE) {
                    close(server_sockfd);
                } else {
                    perror("send(client_sockfd) failed");
                    close(server_sockfd);
                    close(client_sockfd);
                }
                return;
            }
        }
    }
}

// thread entry point.
static void thread_main(void *data) {
    args_t *args = (args_t *) data;
    http_request_t *request;
    http_response_t *response;
    int server_sockfd;
    bool hit, upgrade;

    if ((request = init_http_request(0)) == NULL) {
        perror("request cannot be initialized");
        return;
    }

    if ((response = init_http_response(0)) == NULL) {
        perror("response cannot be initialized");
        free(request);
        return;
    }

    strcpy(request->ip, args->ip);

    if (!rcv_request(args->sockfd, request, response, &upgrade, &hit)) {
        fprintf(stderr, "rcv_request() failed\n");
    } else if (upgrade) {       // HTTP tunneling
        // char str[128] = {0};
        // strcpy(str, request->host);
        // strcat(str, ":");
        // strcat(str, request->port);
        // printf("\nreceived HTTP tunnel (CONNECT Method) request from client\n");
        // printf("Unable to cache\n");
        // printf("request: %s\n", str);
        // fflush(stdout);
        http_tunnel(args->sockfd, request);
        free(request);
        free(response);
        free(args);
        return;
    } else if (hit) {
        printf("hit ");
        print_cache_status();
        log_http_request(request, response);
    } else if (!send_request(request, response, &server_sockfd)) {
        fprintf(stderr, "send_request() failed\n");
    } else if (!rcv_and_send_response(server_sockfd, args->sockfd,
            request, response)){
        fprintf(stderr, "send_response() failed\n");
    } else {
        log_http_request(request, response);
    }

    free_http_request(request);
    free_http_response(response);
    close(args->sockfd);
    free(args);
}

int main(int argc, char *argv[]) {
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
        pthread_t pid;
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

        pthread_create(&pid, NULL, (void *) thread_main, (void *) args);
    }

    close(sockfd);

    return 0;
}
