#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>
#include <http/http_log.h>

typedef struct {
	http_request_t *request;
	http_response_t *response;
} state_t;

static int setup(void **state) {
	state_t *s = malloc(sizeof(state_t));
	http_request_t *request = malloc(sizeof(http_request_t));
	http_response_t *response = malloc(sizeof(http_response_t));

	if (s == NULL || request == NULL || response == NULL) {
		return -1;
	}

	strcpy(request->ip, "192.168.0.1");
	strcpy(request->schema, "http");
	strcpy(request->host, "www.naver.com");

	response->content_length = 128;

	s->request = request;
	s->response = response;

	*state = s;

    return 0;
}

static int teardown(void **state) {
	state_t *s = (state_t *) *state;
	free(s->request);
	free(s->response);
	free(s);
    return 0;
}

static void test_http_log(void **state) {
	state_t *s = (state_t *) *state;
	http_request_t *req = s->request;
	http_response_t *res = s->response;

	assert_true(http_log_set_file("proxy.log"));
	log_http_request(req, res);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_http_log),
    };
    return cmocka_run_group_tests(tests, setup, teardown);
}
