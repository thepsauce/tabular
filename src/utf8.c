#include "tabular.h"

size_t utf8_determinate(Utf8 u)
{
	size_t det = 0;

	if (!(u & 0x80))
		return 1;
	for (det = 0; u & 0x80; det++)
		u <<= 1;
	return det;
}

bool utf8_match(const Utf8 *pattern, const Utf8 *text)
{
	while (*pattern != '\0' || *text != '\0') {
		if (*pattern != *text) {
			if (*pattern == '*') {
				do
					pattern++;
				while (*pattern == '*');
				if (*pattern == '\0')
					return true;
				while (*text != '\0') {
					if (utf8_match(pattern, text))
						return true;
					text++;
				}
			}
			return false;
		}
		pattern++;
		text++;
	}
	return true;
}

size_t utf8_length(const Utf8 *utf8)
{
	return strlen((const char*) utf8);
}

Utf8 *utf8_end(const Utf8 *utf8)
{
	return (Utf8*) utf8 + utf8_length(utf8);
}

Utf8 *utf8_previous(const Utf8 *utf8)
{
	while (utf8--, (*utf8 & 0xc0) == 0x80);
	return (Utf8*) utf8;
}

size_t utf8_previouslength(const Utf8 *utf8)
{
	size_t length;

	for (length = 1; (*(utf8 - length) & 0xc0) == 0x80; length++);
	return length;
}

Utf8 *utf8_next(const Utf8 *utf8)
{
	if (*utf8 == '\0')
		return NULL;
	while (utf8++, (*utf8 & 0xc0) == 0x80);
	return (Utf8*) utf8;
}

size_t utf8_nextlength(const Utf8 *utf8)
{
	size_t length;

	if (*utf8 == '\0')
		return 0;
	for (length = 1; (utf8[length] & 0xc0) == 0x80; length++);
	return length;
}

Utf8 *utf8_lastcharacter(const Utf8 *utf8)
{
	if (*utf8 == '\0')
		return NULL;
	for (; *utf8 != '\0'; utf8++);
	while (utf8--, (*utf8 & 0xc0) == 0x80);
	return (Utf8*) utf8;
}

size_t utf8_lastindex(const Utf8 *utf8)
{
	size_t index;

	if (*utf8 == '\0')
		return 0;
	for (index = 0; utf8[index] != '\0'; index++);
	while (index--, (utf8[index] & 0xc0) == 0x80);
	return index;
}

size_t utf8_widthfirst(const Utf8 *utf8)
{
	wchar_t wch;

	mbstowcs(&wch, utf8, 1);
	return wcwidth(wch);
}

size_t utf8_width(const Utf8 *utf8)
{
	size_t width = 0;

	while (*utf8 != '\0') {
		width += utf8_widthfirst(utf8);
		utf8 = utf8_next(utf8);
	}
	return width;
}

int utf8_getfitting(const Utf8 *utf8, size_t max, struct fitting *fit)
{
	size_t width;
	int code = 0;

	if (max == 0)
		return -1;
	const size_t originalMax = max;
	fit->start = (Utf8*) utf8;
	while (*utf8 != '\0') {
		width = utf8_widthfirst(utf8);
		if (width > max) {
			code = 1;
			break;
		}
		max -= width;
		utf8 = utf8_next(utf8);
	}
	fit->width = originalMax - max;
	fit->end = (Utf8*) utf8;
	return code;
}
