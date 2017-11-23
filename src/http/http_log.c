#include <pthread.h>
#include <time.h>
#include <string.h>
#include "http_log.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static FILE *file;

static char *get_current_time() {
	static char time_str[32] = {0};

	memset(time_str, 0, 32);
	time_t now = time(NULL);
	strftime(time_str, 32, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));

	return time_str;
}

bool http_log_set_file(const char *path) {
	if (path == NULL) {

		pthread_mutex_lock(&lock);
		if (file != NULL) {
			fclose(file);
			file = NULL;
		}
		pthread_mutex_unlock(&lock);
		return true;
	}

	pthread_mutex_lock(&lock);
	if (file != NULL) {
		fclose(file);
	}

	file = fopen(path, "a");
	pthread_mutex_unlock(&lock);

	if (file == NULL) {
		return false;
	}

	return true;
}

void log_http_request(http_request_t *request, http_response_t *response) {
	pthread_mutex_lock(&lock);
	FILE *stream = file != NULL ? file : stdout;
	char url[256] = {0};
	if (strcmp(request->schema, "") != 0) {
		strcpy(url, request->schema);
		strcat(url, "://");
	}
	strcat(url, request->host);
	strcat(url, request->path);
	
	fprintf(stream, "Date: %s: %s %s %d\n", get_current_time(), request->ip,
		url, response->content_length);
	fflush(stream);
	pthread_mutex_unlock(&lock);
}
