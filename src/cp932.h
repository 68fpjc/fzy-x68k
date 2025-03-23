#ifndef CP932_H
#define CP932_H

#include <stddef.h>

/**
 * @brief  Returns true if the character is a lead byte in CP932
 * @param    c The character to check if it is a lead byte
 */
int is_cp932_lead_byte(const char c);

/**
 * @brief  Returns true if the character is a printable character in CP932
 * @param wc  The character to check if it is printable
 */
int is_print_cp932(const short wc);

/**
 * @brief  Returns the length of a string in CP932 characters
 * @param s  The string to count
 * @return   The number of CP932 characters in the string
 */
size_t cp932_strlen(const char *s);

#endif // CP932_H
