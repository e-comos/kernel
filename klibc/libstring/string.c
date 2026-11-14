/**
 * Implementation of string and memory manipulation functions
 */

#include <klibc/stdarg.h>
#include <klibc/string.h>
/* ================================================================
 * Memory manipulation functions
 * ================================================================ */

void* memcpy(void* dest, const void* src, size_t n) {
	char* d = (char*)dest;
	const char* s = (const char*)src;

	for (size_t i = 0; i < n; i++) {
		d[i] = s[i];
	}
	return dest;
}

void* memset(void* s, int c, size_t n) {
	unsigned char* p = (unsigned char*)s;
	unsigned char uc = (unsigned char)c;

	for (size_t i = 0; i < n; i++) {
		p[i] = uc;
	}
	return s;
}

void* memmove(void* dest, const void* src, size_t n) {
	char* d = (char*)dest;
	const char* s = (const char*)src;

	if (d < s) {
		/* Copy forward */
		for (size_t i = 0; i < n; i++) {
			d[i] = s[i];
		}
	} else {
		/* Copy backward to handle overlap */
		for (size_t i = n; i > 0; i--) {
			d[i - 1] = s[i - 1];
		}
	}
	return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
	const unsigned char* p1 = (const unsigned char*)s1;
	const unsigned char* p2 = (const unsigned char*)s2;

	for (size_t i = 0; i < n; i++) {
		if (p1[i] != p2[i]) {
			return p1[i] - p2[i];
		}
	}
	return 0;
}

/* ================================================================
 * String manipulation functions
 * ================================================================ */

size_t strlen(const char* s) {
	size_t len = 0;
	while (s[len] != '\0') {
		len++;
	}
	return len;
}

char* strcpy(char* dest, const char* src) {
	char* d = dest;
	while (*src) {
		*d++ = *src++;
	}
	*d = '\0';
	return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
	char* d = dest;
	size_t i;

	for (i = 0; i < n && src[i] != '\0'; i++) {
		d[i] = src[i];
	}

	/* Pad with null bytes if n > strlen(src) */
	for (; i < n; i++) {
		d[i] = '\0';
	}
	return dest;
}

char* strcat(char* dest, const char* src) {
	char* d = dest;

	/* Find end of dest */
	while (*d != '\0') {
		d++;
	}

	/* Append src */
	while (*src != '\0') {
		*d++ = *src++;
	}
	*d = '\0';

	return dest;
}

char* strncat(char* dest, const char* src, size_t n) {
	char* d = dest;

	/* Find end of dest */
	while (*d != '\0') {
		d++;
	}

	/* Append at most n characters from src */
	size_t i;
	for (i = 0; i < n && src[i] != '\0'; i++) {
		d[i] = src[i];
	}
	d[i] = '\0';

	return dest;
}

int strcmp(const char* s1, const char* s2) {
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
	for (size_t i = 0; i < n; i++) {
		if (s1[i] != s2[i]) {
			return (unsigned char)s1[i] - (unsigned char)s2[i];
		}
		if (s1[i] == '\0') {
			return 0;
		}
	}
	return 0;
}

char* strchr(const char* s, int c) {
	while (*s != '\0') {
		if (*s == (char)c) {
			return (char*)s;
		}
		s++;
	}
	return NULL;
}

char* strrchr(const char* s, int c) {
	const char* last = NULL;
	while (*s != '\0') {
		if (*s == (char)c) {
			last = s;
		}
		s++;
	}
	return (char*)last;
}

char* strrev(char* s) {
	if (!s)
		return NULL;

	char* start = s;
	char* end = s;
	char temp;

	/* Find the end of the string */
	while (*end != '\0') {
		end++;
	}
	end--; /* Move back from null terminator */

	/* Reverse the string */
	while (start < end) {
		temp = *start;
		*start = *end;
		*end = temp;
		start++;
		end--;
	}

	return s;
}

/* ================================================================
 * Memory and string search functions
 * ================================================================ */

void* memchr(const void* s, int c, size_t n) {
	const unsigned char* p = (const unsigned char*)s;
	unsigned char uc = (unsigned char)c;

	for (size_t i = 0; i < n; i++) {
		if (p[i] == uc) {
			return (void*)(p + i);
		}
	}
	return NULL;
}

/* ================================================================
 * String to number conversion functions
 * ================================================================ */

int atoi(const char* s) {
	int result = 0;
	int sign = 1;

	/* Skip whitespace */
	while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' ||
		   *s == '\v') {
		s++;
	}

	/* Check for sign */
	if (*s == '-') {
		sign = -1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	/* Convert digits */
	while (*s >= '0' && *s <= '9') {
		result = result * 10 + (*s - '0');
		s++;
	}

	return sign * result;
}

long strtol(const char* s, char** endptr, int base) {
	long result = 0;
	int sign = 1;

	/* Skip whitespace */
	while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' ||
		   *s == '\v') {
		s++;
	}

	/* Check for sign */
	if (*s == '-') {
		sign = -1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	/* Determine base */
	if (base == 0) {
		if (*s == '0') {
			s++;
			if (*s == 'x' || *s == 'X') {
				base = 16;
				s++;
			} else {
				base = 8;
			}
		} else {
			base = 10;
		}
	}

	/* Convert digits */
	while (1) {
		int digit;
		if (*s >= '0' && *s <= '9') {
			digit = *s - '0';
		} else if (*s >= 'a' && *s <= 'z') {
			digit = *s - 'a' + 10;
		} else if (*s >= 'A' && *s <= 'Z') {
			digit = *s - 'A' + 10;
		} else {
			break;
		}

		if (digit >= base) {
			break;
		}

		result = result * base + digit;
		s++;
	}

	if (endptr) {
		*endptr = (char*)s;
	}

	return sign * result;
}

unsigned long strtoul(const char* s, char** endptr, int base) {
	unsigned long result = 0;

	/* Skip whitespace */
	while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' ||
		   *s == '\v') {
		s++;
	}

	/* Determine base */
	if (base == 0) {
		if (*s == '0') {
			s++;
			if (*s == 'x' || *s == 'X') {
				base = 16;
				s++;
			} else {
				base = 8;
			}
		} else {
			base = 10;
		}
	}

	/* Convert digits */
	while (1) {
		int digit;
		if (*s >= '0' && *s <= '9') {
			digit = *s - '0';
		} else if (*s >= 'a' && *s <= 'z') {
			digit = *s - 'a' + 10;
		} else if (*s >= 'A' && *s <= 'Z') {
			digit = *s - 'A' + 10;
		} else {
			break;
		}

		if (digit >= base) {
			break;
		}

		result = result * base + digit;
		s++;
	}

	if (endptr) {
		*endptr = (char*)s;
	}

	return result;
}

/* ================================================================
 * String formatting functions (simple implementation)
 * ================================================================ */

static void format_putc(char* buf, int* pos, int max_len, char c) {
	if (*pos < max_len - 1) {
		buf[*pos] = c;
		(*pos)++;
	}
}

/* ------------------------------------------------------------------
 * Shared formatting core used by sprintf() and snprintf().
 *
 * Supported conversions:
 *	 %d %i %u %x %X %p %s %c %%
 * Supported length modifiers:
 *	 l  ll  z
 * Supported flags / width:
 *	 '-'  left align
 *	 '0'  zero pad (numeric conversions only; applied after the sign/0x)
 *	 any decimal field width, e.g. %016llX
 * ------------------------------------------------------------------ */
static int fmt_ull(char* out, unsigned long long v, unsigned int base, int upper) {
	const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	char rev[24];
	int n = 0;
	int i;

	do {
		rev[n++] = digits[v % base];
		v /= base;
	} while (v);

	for (i = 0; i < n; i++)
		out[i] = rev[n - 1 - i];
	return n;
}

static int format_core(char* buf, int max_len, const char* format, va_list args) {
	int pos = 0;

	while (*format && pos < max_len - 1) {
		if (*format != '%') {
			format_putc(buf, &pos, max_len, *format);
			format++;
			continue;
		}

		format++; /* skip '%' */

		/* flags */
		int left_align = 0;
		int zero_pad = 0;
		for (;;) {
			if (*format == '-') {
				left_align = 1;
				format++;
			} else if (*format == '0') {
				zero_pad = 1;
				format++;
			} else if (*format == ' ' || *format == '+' || *format == '#') {
				format++;
			} else {
				break;
			}
		}

		/* field width */
		int width = 0;
		while (*format >= '0' && *format <= '9')
			width = width * 10 + (*format++ - '0');

		/* length modifier */
		int len_mod = 0; /* 0 = int, 1 = long, 2 = long long, 3 = size_t */
		if (*format == 'l') {
			format++;
			if (*format == 'l') {
				format++;
				len_mod = 2;
			} else {
				len_mod = 1;
			}
		} else if (*format == 'z') {
			format++;
			len_mod = 3;
		} else if (*format == 'h') {
			format++;
			if (*format == 'h')
				format++;
		}

		char conv = *format;
		if (!conv)
			break;
		format++;

		if (conv == '%') {
			format_putc(buf, &pos, max_len, '%');
			continue;
		}

		/* Strings are written straight out so they are never truncated
		 * by the fixed-size body buffer below. */
		if (conv == 's') {
			const char* s = va_arg(args, const char*);
			int slen = 0;
			int pad;

			if (!s)
				s = "(null)";
			while (s[slen])
				slen++;
			pad = (width > slen) ? width - slen : 0;

			if (!left_align)
				for (int i = 0; i < pad; i++)
					format_putc(buf, &pos, max_len, ' ');
			while (*s)
				format_putc(buf, &pos, max_len, *s++);
			if (left_align)
				for (int i = 0; i < pad; i++)
					format_putc(buf, &pos, max_len, ' ');
			continue;
		}

		char body[64];
		char prefix[4];
		int plen = 0;
		int blen = 0;
		int numeric = 1;

		switch (conv) {
		case 'd':
		case 'i': {
			long long v;
			unsigned long long uv;

			if (len_mod == 2)
				v = va_arg(args, long long);
			else if (len_mod == 1)
				v = va_arg(args, long);
			else
				v = va_arg(args, int);

			if (v < 0) {
				prefix[plen++] = '-';
				uv = (unsigned long long)(-(v + 1)) + 1ULL;
			} else {
				uv = (unsigned long long)v;
			}
			blen = fmt_ull(body, uv, 10, 0);
			break;
		}

		case 'u': {
			unsigned long long uv;

			if (len_mod == 2)
				uv = va_arg(args, unsigned long long);
			else if (len_mod == 1)
				uv = va_arg(args, unsigned long);
			else if (len_mod == 3)
				uv = (unsigned long long)va_arg(args, size_t);
			else
				uv = va_arg(args, unsigned int);

			blen = fmt_ull(body, uv, 10, 0);
			break;
		}

		case 'x':
		case 'X': {
			unsigned long long uv;

			if (len_mod == 2)
				uv = va_arg(args, unsigned long long);
			else if (len_mod == 1)
				uv = va_arg(args, unsigned long);
			else if (len_mod == 3)
				uv = (unsigned long long)va_arg(args, size_t);
			else
				uv = va_arg(args, unsigned int);

			blen = fmt_ull(body, uv, 16, conv == 'X');
			break;
		}

		case 'p': {
			unsigned long long uv =
				(unsigned long long)(uintptr_t)va_arg(args, void*);
			prefix[plen++] = '0';
			prefix[plen++] = 'x';
			blen = fmt_ull(body, uv, 16, 0);
			break;
		}

		case 'c': {
			numeric = 0;
			body[blen++] = (char)va_arg(args, int);
			break;
		}

		default:
			numeric = 0;
			prefix[plen++] = '%';
			body[blen++] = conv;
			break;
		}

		int total = plen + blen;
		int pad = (width > total) ? width - total : 0;
		int i;

		if (left_align || !zero_pad || !numeric) {
			/* space padding (left aligned pads on the right) */
			if (!left_align)
				for (i = 0; i < pad; i++)
					format_putc(buf, &pos, max_len, ' ');
			for (i = 0; i < plen; i++)
				format_putc(buf, &pos, max_len, prefix[i]);
			for (i = 0; i < blen; i++)
				format_putc(buf, &pos, max_len, body[i]);
			if (left_align)
				for (i = 0; i < pad; i++)
					format_putc(buf, &pos, max_len, ' ');
		} else {
			/* zero padding goes after the sign / 0x prefix */
			for (i = 0; i < plen; i++)
				format_putc(buf, &pos, max_len, prefix[i]);
			for (i = 0; i < pad; i++)
				format_putc(buf, &pos, max_len, '0');
			for (i = 0; i < blen; i++)
				format_putc(buf, &pos, max_len, body[i]);
		}
	}

	return pos;
}

int sprintf(char* buf, const char* format, ...) {
	va_list args;
	int pos;

	va_start(args, format);
	pos = format_core(buf, 1024, format, args);
	va_end(args);

	buf[pos] = '\0';
	return pos;
}

int snprintf(char* buf, size_t size, const char* format, ...) {
	va_list args;
	int max_len = (int)size;
	int pos;

	va_start(args, format);
	pos = format_core(buf, max_len, format, args);
	va_end(args);

	if (max_len > 0)
		buf[pos < max_len ? pos : max_len - 1] = '\0';
	return pos;
}

/* ================================================================
 * Character classification functions
 * ================================================================ */

int isdigit(int c) {
	return (c >= '0' && c <= '9');
}

int islower(int c) {
	return (c >= 'a' && c <= 'z');
}

int isupper(int c) {
	return (c >= 'A' && c <= 'Z');
}

int isalpha(int c) {
	return islower(c) || isupper(c);
}

int isalnum(int c) {
	return isalpha(c) || isdigit(c);
}

int isspace(int c) {
	return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
			c == '\v');
}

/* ================================================================
 * Character conversion functions
 * ================================================================ */

int toupper(int c) {
	if (islower(c)) {
		return c - ('a' - 'A');
	}
	return c;
}

int tolower(int c) {
	if (isupper(c)) {
		return c + ('a' - 'A');
	}
	return c;
}
