#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "yomu.h"

int test_file(char *filename) {
	FILE *fp = fopen("skip_test.html", "r");

	int *page_len_max = malloc(sizeof(int)), page_index = 0;
	*page_len_max = 8;
	char *full_page = malloc(sizeof(char) * 8);
	full_page[0] = '\0';

	size_t buffer_page_size = sizeof(char);
	char *buffer_page = malloc(sizeof(char));
	int line_len = 0;
	while ((line_len = getline(&buffer_page, &buffer_page_size, fp)) != -1) {
		//printf("%d %s\n", line_len, buffer_page);
		while ((page_index + line_len) >= *page_len_max) {
			*page_len_max *= 2;
			full_page = realloc(full_page, sizeof(char) * *page_len_max);
		}

		page_index += line_len;
		strcat(full_page, buffer_page);
	}

	//printf("%s\n", full_page);

	free(buffer_page);
	fclose(fp);

	free(page_len_max);

	yomu_t *yomu_test = yomu_f.parse(full_page);

	//printf("%s\n", yomu_f.read(yomu_test, ""));

	yomu_f.destroy(yomu_test);

	free(full_page);

	return 0;
}

int fp_test() {
	yomu_t *yomu_test = yomu_f.parse("skip_test.html");

	yomu_f.destroy(yomu_test);

	return 0;
}

int main() {
	yomu_f.init();

	fp_test();
	//test_file("test_data.html");

	yomu_f.close();

	return 0;
}