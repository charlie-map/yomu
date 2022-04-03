#include <stdio.h>
#include <stdarg.h>

#include "yomu.h"

int test_file(char *filename) {
	yomu_t *yomu = yomu_f.parse("test_data.html");

	char *yomu_data = yomu_f.read(yomu, 'd');
	printf("yomu: %s\n", yomu_data);

	return 0;
}

int main() {
	test_file("test_data.html");

	return 0;
}