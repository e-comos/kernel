/**
 * Implementation of string and memory manipulation functions
 */

#include <klibc/string.h>
#include <klibc/stdarg.h>
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
            d[i-1] = s[i-1];
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
    if (!s) return NULL;
    
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
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') {
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
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') {
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
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') {
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

int sprintf(char* buf, const char* format, ...) {
    int pos = 0;
    int max_len = 1024; /* Simple limit */
    
    va_list args;
    va_start(args, format);
    
    while (*format && pos < max_len - 1) {
        if (*format != '%') {
            format_putc(buf, &pos, max_len, *format);
            format++;
            continue;
        }
        
        format++; /* Skip '%' */
        
        switch (*format) {
            case 'd': {
                int val = va_arg(args, int);
                char temp[16];
                int i = 0;
                int is_negative = 0;
                
                if (val < 0) {
                    is_negative = 1;
                    val = -val;
                }
                
                /* Convert to string */
                do {
                    temp[i++] = '0' + (val % 10);
                    val /= 10;
                } while (val > 0);
                
                if (is_negative) {
                    temp[i++] = '-';
                }
                
                /* Reverse and copy */
                for (int j = i - 1; j >= 0; j--) {
                    format_putc(buf, &pos, max_len, temp[j]);
                }
                break;
            }
            
            case 's': {
                char* str = va_arg(args, char*);
                while (*str) {
                    format_putc(buf, &pos, max_len, *str);
                    str++;
                }
                break;
            }
            
            case 'c': {
                char c = (char)va_arg(args, int);
                format_putc(buf, &pos, max_len, c);
                break;
            }
            
            case 'x': {
                unsigned int val = va_arg(args, unsigned int);
                char hex[] = "0123456789abcdef";
                char temp[9];
                int i = 0;
                
                do {
                    temp[i++] = hex[val & 0xF];
                    val >>= 4;
                } while (val > 0);
                
                /* Pad to 8 digits */
                while (i < 8) {
                    temp[i++] = '0';
                }
                
                /* Reverse and copy */
                for (int j = i - 1; j >= 0; j--) {
                    format_putc(buf, &pos, max_len, temp[j]);
                }
                break;
            }
            
            default:
                format_putc(buf, &pos, max_len, '%');
                format_putc(buf, &pos, max_len, *format);
                break;
        }
        
        format++;
    }
    
    buf[pos] = '\0';
    va_end(args);
    
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
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
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
