#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "yomu.h"

int test_file(char *filename) {
	yomu_t *yomu = yomu_f.parse("test_data.html");

	int *div_len = malloc(sizeof(int));
	yomu_t **div = yomu_f.find(yomu, "img", div_len);

	for (int i = 0; i < *div_len; i++) {
		printf("%s\n", yomu_f.attr.get(div[i], "src"));
	}

	free(div_len);
	free(div);

	yomu_f.destroy(yomu);

	return 0;
}

int main() {
	yomu_f.init();

	test_file("test_data.html");

	yomu_f.close();

	return 0;
}