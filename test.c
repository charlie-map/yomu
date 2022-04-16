#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "yomu.h"

int test_file(char *filename) {
	yomu_t *yomu = yomu_f.parse("test_data.html");

	char *yomu_data = yomu_f.read(yomu, 'd');
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