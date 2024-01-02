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
	bool escaped = false;
	size_t idxText, lastText, lenText;
	size_t lastPattern;
	size_t idxPattern;
	size_t lenPattern;

	lenText = strlen(text);
repeat:
	idxText = 0;
	idxPattern = 0;
	lenPattern = 0;
	lastPattern = SIZE_MAX;
	while (pattern[lenPattern] != '\0' &&
			(pattern[lenPattern] != '|' || escaped)) {
		if (pattern[lenPattern] == '\\')
			escaped = !escaped;
		else
			escaped = false;
		lenPattern++;
	}

	while (idxPattern != lenPattern || idxText != lenText) {
		escaped = false;
		if (pattern[idxPattern] == '\\') {
			escaped = true;
			idxPattern++;
		}
		if (pattern[idxPattern] == '*' && !escaped) {
			while (pattern[idxPattern] == '*')
				idxPattern++;
			lastPattern = idxPattern;
			lastText = idxText;
		} else if (pattern[idxPattern] == text[idxText] && idxPattern != lenPattern) {
			idxPattern++;
			idxText++;
		} else if (lastPattern != SIZE_MAX && idxText != lenText) {
			idxPattern = lastPattern;
			idxText = ++lastText;
		} else {
			if (pattern[lenPattern] == '|') {
				pattern += lenPattern + 1;
				goto repeat;
			}
			return false;
		}
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

int utf8_getnfitting(const Utf8 *utf8, size_t n, struct fitting *fit)
{
	size_t totalWidth, width;
	size_t count;

	fit->start = (Utf8*) utf8;
	totalWidth = 0;
	while (n > 0) {
		width = utf8_widthfirst(utf8);
		totalWidth += width;
		count = utf8_determinate(*utf8);
		if (count > n)
			return -1;
		n -= count;
		utf8 += count;
	}
	fit->width = totalWidth;
	fit->end = (Utf8*) utf8;
	return 0;
}
