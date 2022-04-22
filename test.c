#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "yomu.h"

int test_file(char *filename) {
	yomu_t *yomu = yomu_f.parse("einstein.html");

	yomu_f.destroy(yomu);

	return 0;
}

int main() {
	yomu_f.init();

	test_file("test_data.html");

	yomu_f.close();

	return 0;
}