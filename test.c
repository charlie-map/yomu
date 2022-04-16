#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "yomu.h"

int test_file(char *filename) {
	yomu_t *yomu = yomu_f.parse("test_data.html");

	int *div_len = malloc(sizeof(int));
	yomu_t **div = yomu_f.find(yomu, "div p", div_len);

	yomu_t *div_merge = yomu_f.merge(*div_len, div);
	free(div_len);

	printf("read -- ");
	char *yomu_data = yomu_f.read(div_merge, "");
	printf("yomu: %s\n", yomu_data);

	free(yomu_data);
	yomu_f.destroy(yomu);

	return 0;
}

int main() {
	yomu_f.init();

	test_file("test_data.html");

	yomu_f.close();

	return 0;
}