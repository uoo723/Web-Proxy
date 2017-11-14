#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

#include <http/http_request.h>

static int setup(void **state) {
	return 0;
}

static int teardown(void **state) {
	return 0;
}

static void test_http_request(void **state) {
	http_parser *parser = malloc(sizeof(http_parser));
	http_parser_settings settings;
	http_request_t *request = malloc(sizeof(http_request_t));

	http_parser_settings_init(&settings);
	settings.on_url = on_url_cb;
	settings.on_header_field = on_header_field_cb;
	settings.on_header_value = on_header_value_cb;
	settings.on_body = on_body_cb;
	settings.on_message_complete = on_message_complete_cb;

	http_parser_init(parser, HTTP_REQUEST);
	parser->data = request;

	memset(request, 0, sizeof(http_request_t));

	char buf[1024] =
		"GET / HTTP/1.1\r\n"
		"User-Agent: Mozilla/4.0 (compatible; MSIE5.01; Windows NT)\r\n"
		"Host: www.xxx.com\r\n"
		"Accept-Language: en-us\r\n"
		"Accept-Encoding: gzip, deflate\r\n"
		"Connection: Keep-Alive\r\n"
		"Content-Length: 11\r\n\r\n"
		"sample body";

	size_t nparsed, recved;
	recved = strlen(buf);
	nparsed = http_parser_execute(parser, &settings, buf, recved);

	assert_int_equal(request->method, HTTP_GET);
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

int main() {
	const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_http_request, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
