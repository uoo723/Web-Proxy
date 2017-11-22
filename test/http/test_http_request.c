#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

#include <http/http_request.h>

typedef struct {
	http_parser *parser;
	http_parser_settings *settings;
	http_request_t *request;
} state_t;

static char *messages[1024] = {
	"GET / HTTP/1.1\r\n"
	"User-Agent: Mozilla/4.0 (compatible; MSIE5.01; Windows NT)\r\n"
	"Host: www.xxx.com\r\n"
	"Accept-Language: en-us\r\n"
	"Accept-Encoding: gzip, deflate\r\n"
	"Connection: Keep-Alive\r\n"
	"Content-Length: 11\r\n\r\n"
	"sample body",

	"GET http://www.xxx.com HTTP/1.1\r\n"
	"User-Agent: Mozilla/4.0 (compatible; MSIE5.01; Windows NT)\r\n"
	"Host: www.xxx.com\r\n"
	"Accept-Language: en-us\r\n"
	"Accept-Encoding: gzip, deflate\r\n"
	"Connection: Keep-Alive\r\n"
	"Content-Length: 11\r\n\r\n"
	"sample body",

	"GET https://www.xxx.com/test HTTP/1.1\r\n"
	"User-Agent: Mozilla/4.0 (compatible; MSIE5.01; Windows NT)\r\n"
	"Host: www.xxx.com\r\n"
	"Accept-Language: en-us\r\n"
	"Accept-Encoding: gzip, deflate\r\n"
	"Connection: Keep-Alive\r\n"
	"Content-Length: 11\r\n\r\n"
	"sample body",

	"GET https://www.xxx.com:443/test/ HTTP/1.1\r\n"
	"User-Agent: Mozilla/4.0 (compatible; MSIE5.01; Windows NT)\r\n"
	"Host: www.xxx.com\r\n"
	"Accept-Language: en-us\r\n"
	"Accept-Encoding: gzip, deflate\r\n"
	"Connection: Keep-Alive\r\n"
	"Content-Length: 11\r\n\r\n"
	"sample body",

	"GET http://192.168.56.101 HTTP/1.1\r\n"
	"User-Agent: Mozilla/4.0 (compatible; MSIE5.01; Windows NT)\r\n"
	"Host: 192.168.56.101"
	"Accept-Language: en-us\r\n"
	"Accept-Encoding: gzip, deflate\r\n"
	"Connection: Keep-Alive\r\n"
	"Content-Length: 11\r\n\r\n"
	"sample body"
};

static int setup(void **state) {
	state_t *s = malloc(sizeof(state_t));
	http_parser *parser = malloc(sizeof(http_parser));
	http_parser_settings *settings = malloc(sizeof(http_parser_settings));
	http_request_t *request = malloc(sizeof(http_request_t));

	if (s == NULL || parser == NULL || settings == NULL || request == NULL) {
		return -1;
	}

	http_parser_settings_init(settings);
	settings->on_url = request_on_url_cb;
	settings->on_header_field = request_on_header_field_cb;
	settings->on_header_value = request_on_header_value_cb;
	settings->on_body = request_on_body_cb;
	settings->on_message_complete = request_on_message_complete_cb;

	memset(parser, 0, sizeof(http_parser));
	memset(request, 0, sizeof(http_request_t));

	s->parser = parser;
	s->settings = settings;
	s->request = request;

	*state = s;

	return 0;
}

static int teardown(void **state) {
	state_t *s = (state_t *) *state;
	free(s->parser);
	free(s->settings);
	free(s->request);
	free(s);

	return 0;
}

static void test_http_request(void **state) {
	state_t *s = (state_t *) *state;

	http_parser *parser = s->parser;
	http_parser_settings *settings = s->settings;
	http_request_t *request = s->request;

	size_t nparsed, recved;
	recved = strlen(messages[0]);

	parser->data = request;

	http_parser_init(parser, HTTP_REQUEST);

	nparsed = http_parser_execute(parser, settings, messages[0], recved);

	assert_true(recved == nparsed);

	assert_int_equal(request->method, HTTP_GET);
	assert_int_equal(parser->http_errno, HPE_OK);
	assert_string_equal(find_header_value(&request->headers, "User-Agent"),
		"Mozilla/4.0 (compatible; MSIE5.01; Windows NT)");
	assert_string_equal(find_header_value(&request->headers, "Host"),
		"www.xxx.com");
	assert_string_equal(find_header_value(&request->headers, "Accept-Language"),
		"en-us");
	assert_string_equal(find_header_value(&request->headers, "Accept-Encoding"),
		"gzip, deflate");
	assert_string_equal(find_header_value(&request->headers, "Connection"),
		"Keep-Alive");
	assert_string_equal(find_header_value(&request->headers, "Content-Length"),
		"11");
	assert_string_equal(request->body, "sample body");
}

static void test_failed_http_request(void **state) {
	state_t *s = (state_t *) *state;

	http_parser *parser = s->parser;
	http_parser_settings *settings = s->settings;
	http_request_t *request = s->request;

	size_t nparsed, recved;
	recved = strlen(messages[0]);

	http_parser_init(parser, HTTP_REQUEST);
	nparsed = http_parser_execute(parser, settings, messages[0], recved);

	assert_null(parser->data);
	assert_false(nparsed == recved);
	assert_true(parser->http_errno == HPE_CB_url);
}

static void test_parse_url(void **state) {
	state_t *s = (state_t *) *state;
	http_parser *parser = s->parser;
	http_parser_settings *settings = s->settings;
	http_request_t *request = s->request;

	size_t nparsed, recved;
	recved = strlen(messages[0]);

	parser->data = request;

	http_parser_init(parser, HTTP_REQUEST);

	nparsed = http_parser_execute(parser, settings, messages[0], recved);

	assert_true(parser->http_errno == HPE_OK);
	assert_true(nparsed == recved);

	assert_true(strcmp(request->host, "www.xxx.com") == 0);
	assert_true(strcmp(request->schema, "http") == 0);
	assert_true(strcmp(request->path, "/") == 0);

	memset(request, 0, sizeof(http_request_t));
	http_parser_init(parser, HTTP_REQUEST);

	recved = strlen(messages[1]);

	nparsed = http_parser_execute(parser, settings, messages[1], recved);

	assert_true(parser->http_errno == HPE_OK);
	assert_true(nparsed == recved);

	assert_true(strcmp(request->host, "www.xxx.com") == 0);
	assert_true(strcmp(request->schema, "http") == 0);
	assert_true(strcmp(request->path, "/") == 0);

	memset(request, 0, sizeof(http_request_t));
	http_parser_init(parser, HTTP_REQUEST);

	recved = strlen(messages[2]);

	nparsed = http_parser_execute(parser, settings, messages[2], recved);

	assert_true(parser->http_errno == HPE_OK);
	assert_true(nparsed == recved);

	assert_true(strcmp(request->host, "www.xxx.com") == 0);
	assert_true(strcmp(request->schema, "https") == 0);
	assert_true(strcmp(request->path, "/test") == 0);
	assert_true(strcmp(request->port, "") == 0);

	memset(request, 0, sizeof(http_request_t));
	http_parser_init(parser, HTTP_REQUEST);

	recved = strlen(messages[3]);

	nparsed = http_parser_execute(parser, settings, messages[3], recved);

	assert_true(parser->http_errno == HPE_OK);
	assert_true(nparsed == recved);

	assert_true(strcmp(request->host, "www.xxx.com") == 0);
	assert_true(strcmp(request->schema, "https") == 0);
	assert_true(strcmp(request->path, "/test") == 0);
	assert_true(strcmp(request->port, "443") == 0);

	memset(request, 0, sizeof(http_request_t));
	http_parser_init(parser, HTTP_REQUEST);

	assert_true(strcmp(request->host, "") == 0);
	recved = strlen(messages[4]);

	nparsed = http_parser_execute(parser, settings, messages[4], recved);

	assert_true(parser->http_errno == HPE_OK);
	assert_true(nparsed == recved);

	assert_true(strcmp(request->host, "192.168.56.101") == 0);
	assert_true(strcmp(request->schema, "http") == 0);
}

int main() {
	const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_http_request, setup, teardown),
        cmocka_unit_test_setup_teardown(test_failed_http_request, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
