#include "skyuv_runtime.h"

#include <stdio.h>

int main(int argc, char *argv[]) {
	FILE *current;
	FILE *replacement;
	if (argc != 4) {
		return 2;
	}
	current = skyuv_logger_reopen(NULL, argv[1]);
	if (current == NULL) {
		return 1;
	}
	fputs("before failure\n", current);
	fflush(current);
	replacement = skyuv_logger_reopen(current, argv[2]);
	if (replacement != current) {
		return 1;
	}
	if (fputs("after failure\n", replacement) < 0 || fflush(replacement) != 0) {
		return 1;
	}
	replacement = skyuv_logger_reopen(current, argv[3]);
	if (replacement == NULL || replacement == current) {
		fclose(current);
		return 1;
	}
	if (fputs("after recovery\n", replacement) < 0) {
		fclose(replacement);
		return 1;
	}
	return fclose(replacement) == 0 ? 0 : 1;
}
