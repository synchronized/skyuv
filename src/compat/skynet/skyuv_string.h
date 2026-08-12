#ifndef SKYUV_COMPAT_SKYNET_STRING_H
#define SKYUV_COMPAT_SKYNET_STRING_H

#include <string.h>

#if defined(_MSC_VER)
static char *
skyuv_compat_strsep(char **string, const char *delimiters) {
	char *token;
	char *current;

	if (string == NULL || *string == NULL) {
		return NULL;
	}
	token = *string;
	current = token;
	while (*current != '\0') {
		if (strchr(delimiters, *current) != NULL) {
			*current = '\0';
			*string = current + 1;
			return token;
		}
		++current;
	}
	*string = NULL;
	return token;
}

#define strsep skyuv_compat_strsep
#endif

#endif
