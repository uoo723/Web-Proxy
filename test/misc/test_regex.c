#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <sys/types.h>
#include <regex.h>

static char *test_str[5] = {
	"192.168.0.13", 		// ok
	"www.naver.com",		// fail
	"192.adf.aa",			// fail
	"192.1122.0.3",			// fail
	"255.255.0.0",			// ok
};

static void test_regex(void **state) {
	regex_t regex;
	int ret;

	if (regcomp(&regex, "^([0-9]{1,3}\\.){3}[0-9]{1,3}$", REG_EXTENDED|REG_NOSUB)) {
		fail_msg("regcomp() failed");
	}

	ret = regexec(&regex, test_str[0], 0, NULL, 0);
	assert_true(ret == 0);

	ret = regexec(&regex, test_str[1], 0, NULL, 0);
	assert_true(ret == REG_NOMATCH);

	ret = regexec(&regex, test_str[2], 0, NULL, 0);
	assert_true(ret == REG_NOMATCH);

	ret = regexec(&regex, test_str[3], 0, NULL, 0);
	assert_true(ret == REG_NOMATCH);

	ret = regexec(&regex, test_str[4], 0, NULL, 0);
	assert_true(ret == 0);

	regfree(&regex);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_regex),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
