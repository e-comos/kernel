/**
 * String and memory manipulation functions
 * 
 * This header provides standard C library string and memory functions
 * for kernel use. Since the kernel cannot use libc, we implement
 * our own versions of these functions.
 */

#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>

/* ================================================================
 * Memory manipulation functions
 * ================================================================ */

/**
 * Copy n bytes from src to dest
 * 
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return Pointer to dest
 */
void* memcpy(void* dest, const void* src, size_t n);

/**
 * Fill memory with a constant byte
 * 
 * @param s Pointer to memory to fill
 * @param c Byte value to fill with
 * @param n Number of bytes to fill
 * @return Pointer to s
 */
void* memset(void* s, int c, size_t n);

/**
 * Copy n bytes from src to dest, handling overlapping memory
 * 
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return Pointer to dest
 */
void* memmove(void* dest, const void* src, size_t n);

/**
 * Compare two memory areas
 * 
 * @param s1 First memory area
 * @param s2 Second memory area
 * @param n Number of bytes to compare
 * @return 0 if equal, <0 if s1 < s2, >0 if s1 > s2
 */
int memcmp(const void* s1, const void* s2, size_t n);

/* ================================================================
 * String manipulation functions
 * ================================================================ */

/**
 * Calculate the length of a string
 * 
 * @param s Null-terminated string
 * @return Length of the string (excluding null terminator)
 */
size_t strlen(const char* s);

/**
 * Copy a string
 * 
 * @param dest Destination buffer
 * @param src Source string
 * @return Pointer to dest
 */
char* strcpy(char* dest, const char* src);

/**
 * Copy up to n characters from src to dest
 * 
 * @param dest Destination buffer
 * @param src Source string
 * @param n Maximum number of characters to copy
 * @return Pointer to dest
 */
char* strncpy(char* dest, const char* src, size_t n);

/**
 * Concatenate two strings
 * 
 * @param dest Destination buffer (must be large enough)
 * @param src Source string to append
 * @return Pointer to dest
 */
char* strcat(char* dest, const char* src);

/**
 * Concatenate up to n characters from src to dest
 * 
 * @param dest Destination buffer
 * @param src Source string
 * @param n Maximum number of characters to concatenate
 * @return Pointer to dest
 */
char* strncat(char* dest, const char* src, size_t n);

/**
 * Compare two strings
 * 
 * @param s1 First string
 * @param s2 Second string
 * @return 0 if equal, <0 if s1 < s2, >0 if s1 > s2
 */
int strcmp(const char* s1, const char* s2);

/**
 * Compare up to n characters of two strings
 * 
 * @param s1 First string
 * @param s2 Second string
 * @param n Maximum number of characters to compare
 * @return 0 if equal, <0 if s1 < s2, >0 if s1 > s2
 */
int strncmp(const char* s1, const char* s2, size_t n);

/**
 * Find the first occurrence of a character in a string
 * 
 * @param s String to search
 * @param c Character to find
 * @return Pointer to first occurrence, or NULL if not found
 */
char* strchr(const char* s, int c);

/**
 * Find the last occurrence of a character in a string
 * 
 * @param s String to search
 * @param c Character to find
 * @return Pointer to last occurrence, or NULL if not found
 */
char* strrchr(const char* s, int c);

/**
 * Reverse a string in place
 * 
 * @param s String to reverse
 * @return Pointer to the reversed string
 */
char* strrev(char* s);

/* ================================================================
 * Memory and string search functions
 * ================================================================ */

/**
 * Find a byte in a memory block
 * 
 * @param s Memory block to search
 * @param c Byte to search for
 * @param n Size of the memory block
 * @return Pointer to the first occurrence, or NULL if not found
 */
void* memchr(const void* s, int c, size_t n);

/* ================================================================
 * String to number conversion functions
 * ================================================================ */

/**
 * Convert a string to an integer
 * 
 * @param s String to convert
 * @return Converted integer
 */
int atoi(const char* s);

/**
 * Convert a string to a long integer
 * 
 * @param s String to convert
 * @param endptr Pointer to the first invalid character
 * @param base Base to use for conversion (2-36, 0 for auto)
 * @return Converted long integer
 */
long strtol(const char* s, char** endptr, int base);

/**
 * Convert a string to an unsigned long integer
 * 
 * @param s String to convert
 * @param endptr Pointer to the first invalid character
 * @param base Base to use for conversion (2-36, 0 for auto)
 * @return Converted unsigned long integer
 */
unsigned long strtoul(const char* s, char** endptr, int base);

/**
 * String formatting functions
 * ================================================================ */

/**
 * Format a string (simple implementation)
 * 
 * @param buf Buffer to store the formatted string
 * @param format Format string
 * @param ... Variable arguments
 * @return Number of characters written
 */
int sprintf(char* buf, const char* format, ...);

/**
 * Format a string with size limit
 * 
 * @param buf Buffer to store the formatted string
 * @param size Maximum buffer size
 * @param format Format string
 * @param ... Variable arguments
 * @return Number of characters written (excluding null terminator)
 */
int snprintf(char* buf, size_t size, const char* format, ...);

/* ================================================================
 * Character classification functions
 * ================================================================ */

/**
 * Check if a character is a digit
 * 
 * @param c Character to check
 * @return Non-zero if digit, 0 otherwise
 */
int isdigit(int c);

/**
 * Check if a character is a lowercase letter
 * 
 * @param c Character to check
 * @return Non-zero if lowercase, 0 otherwise
 */
int islower(int c);

/**
 * Check if a character is an uppercase letter
 * 
 * @param c Character to check
 * @return Non-zero if uppercase, 0 otherwise
 */
int isupper(int c);

/**
 * Check if a character is a letter
 * 
 * @param c Character to check
 * @return Non-zero if letter, 0 otherwise
 */
int isalpha(int c);

/**
 * Check if a character is alphanumeric
 * 
 * @param c Character to check
 * @return Non-zero if alphanumeric, 0 otherwise
 */
int isalnum(int c);

/**
 * Check if a character is a space
 * 
 * @param c Character to check
 * @return Non-zero if space, 0 otherwise
 */
int isspace(int c);

/* ================================================================
 * Character conversion functions
 * ================================================================ */

/**
 * Convert a character to uppercase
 * 
 * @param c Character to convert
 * @return Uppercase equivalent, or unchanged if not a letter
 */
int toupper(int c);

/**
 * Convert a character to lowercase
 * 
 * @param c Character to convert
 * @return Lowercase equivalent, or unchanged if not a letter
 */
int tolower(int c);

#endif /* STRING_H */
