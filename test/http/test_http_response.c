#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

#include <http/http_response.h>

typedef struct {
	http_parser *parser;
	http_parser_settings *settings;
	http_response_t *response;
	char buf[1024];
} state_t;

static int setup(void **state) {
	state_t *s;
	http_parser *parser;
	http_parser_settings *settings;
	http_response_t *response;

	if ((s = malloc(sizeof(state_t))) == NULL) {
		return -1;
	}

	if ((parser = malloc(sizeof(http_parser))) == NULL) {
		free(s);
		return -1;
	}

	if ((settings = malloc(sizeof(http_parser_settings))) == NULL) {
		free(s);
		free(parser);
		return -1;
	}

	if ((response = init_http_response(0)) == NULL) {
		free(s);
		free(parser);
		free(settings);
		return -1;
	}

	http_parser_settings_init(settings);
	settings->on_header_field = response_on_header_field_cb;
	settings->on_header_value = response_on_header_value_cb;
	settings->on_body = response_on_body_cb;
	settings->on_message_complete = response_on_message_complete_cb;

	http_parser_init(parser, HTTP_RESPONSE);

	parser->data = response;

	char buf[1024] =
		"HTTP/1.1 200 OK\r\n"
		"Date: Mon, 27 Jul 2009 12:28:53 GMT\r\n"
		"Server: Apache/2.2.14 (Win32)\r\n"
		"Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT\r\n"
		"Content-Length: 56\r\n"
		"Content-Type: text/html\r\n"
		"Connection: Closed\r\n\r\n"
		"<html>\n"
		"\t<body>\n"
   		"\t\t<h1>Hello, World!</h1>\n"
		"\t</body>\n"
		"</html>";

	strcpy(s->buf, buf);

	s->parser = parser;
	s->settings = settings;
	s->response = response;

	*state = s;

	return 0;
}

static int teardown(void **state) {
	state_t *s = (state_t *) *state;
	free(s->parser);
	free(s->settings);
	free_http_response(s->response);
	free(s);

	return 0;
}

static void test_response_parse(void **state) {
	state_t *s = (state_t *) *state;
	http_parser *parser = s->parser;
	http_parser_settings *settings = s->settings;
	http_response_t *response = s->response;

	size_t recved, nparsed;
	recved = strlen(s->buf);

	nparsed = http_parser_execute(parser, settings, s->buf, recved);

	assert_true(nparsed == recved);
	assert_true(parser->http_errno == HPE_OK);
	assert_true(response->status == HTTP_STATUS_OK);
	assert_true(parser->http_major == 1);
	assert_true(parser->http_minor == 1);

	assert_string_equal(find_header_value(response->headers, "Date"),
		"Mon, 27 Jul 2009 12:28:53 GMT");
	assert_string_equal(find_header_value(response->headers, "Server"),
		"Apache/2.2.14 (Win32)");
	assert_string_equal(find_header_value(response->headers, "Last-Modified"),
		"Wed, 22 Jul 2009 19:15:56 GMT");

	assert_true(response->content_length == 56);

	char body[64] =
		"<html>\n"
		"\t<body>\n"
		"\t\t<h1>Hello, World!</h1>\n"
		"\t</body>\n"
		"</html>";
}

int main() {
	const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_response_parse),
    };
    return cmocka_run_group_tests(tests, setup, teardown);
}
