typedef char Utf8;

size_t utf8_determinate(Utf8 u);
bool utf8_match(const Utf8 *pattern, const Utf8 *text);
size_t utf8_length(const Utf8 *utf8);
Utf8 *utf8_end(const Utf8 *utf8);
Utf8 *utf8_previous(const Utf8 *utf8);
size_t utf8_previouslength(const Utf8 *utf8);
Utf8 *utf8_next(const Utf8 *utf8);
size_t utf8_nextlength(const Utf8 *utf8);
Utf8 *utf8_lastcharacter(const Utf8 *utf8);
size_t utf8_lastindex(const Utf8 *utf8);
size_t utf8_widthfirst(const Utf8 *utf8);
size_t utf8_width(const Utf8 *utf8);

struct fitting {
	Utf8 *start;
	Utf8 *end;
	size_t width;
};

/*  0: The text fits.
 * -1: max is 0.
 *  1: The text does not fit.
 */
int utf8_getfitting(const Utf8 *utf8, size_t max, struct fitting *fitting);
