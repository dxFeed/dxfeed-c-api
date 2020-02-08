//
// Created by Mikhail on 07.02.2020.
//
#include <stdlib.h>
#include <string.h>

#include "../../src/PrimitiveTypes.h"

char *elements = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
//char *elements = "0123456789abcdef";
int find_char(char* string, char c) {
	char *e = strchr(string, c);
	if (e == NULL) {
		return -1;
	}
	return (int)(e - string);
}
char* strgen(char *sourceString) {
	char *result;
	if (NULL == sourceString || strlen(sourceString) < 1) {
		result = malloc(sizeof(char));
		result[0] = '0';
	} else {
		bool startNew = true;
		for (int i = 0; i < strlen(sourceString); i++) {
			if (sourceString[i] != elements[strlen(elements) - 1]) {
				startNew = false;
				break;
			}
		}
		if (startNew) {
			size_t size = (strlen(sourceString) + 1) * sizeof(char);
			result = malloc(size);
			for (int i = 0; i < size; i++) {
				result[i] = '0';
			}
			result[strlen(sourceString)+1] = 0;
		} else {
			result = malloc(strlen(sourceString) * sizeof(char));
			bool shift = true;
			for (int i = strlen(sourceString) - 1; i >= 0; i--) {
				if (shift) {
					int oldIdx = find_char(elements, sourceString[i]);
					int newIdx = oldIdx + 1;
					if (newIdx >= strlen(elements)) {
						newIdx = 0;
					} else {
						shift = false;
					}
					result[i] = elements[newIdx];
				} else {
					result[i] = sourceString[i];
				}
			}
			result[strlen(sourceString)] = 0;
		}
	}
	return result;
}
