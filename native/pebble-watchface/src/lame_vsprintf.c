#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

/* Because pebble SDK doesn't implement it. Damn it, honestly... */

static inline char hex_nibble(int val)
{
	if(val <= 9)
		return '0' + val;
	else
		return 'A' + (val - 10);
}

static char *write_hex(char *str, uintptr_t val)
{
	int nibbles = (sizeof(uintptr_t) * 2); 

	for(int i = nibbles - 1; i >= 0; i--) {
		str[i] = hex_nibble(val & 0xf);
		val >>= 4;
	}

	return str + nibbles;
}

static char *write_dec(char *str, uintptr_t val)
{
	if(val == 0) {
		*str++ = '0';
		return str;
	}

	char *start = str;

	while(val) {
		*str++ = '0' + (val % 10);
		val /= 10;
	}

	char *end = str--;

	while(start < str) {
		char tmp = *start;
		*start++ = *str;
		*str-- = tmp;
	}

	return end;
}

static char *write_ptr(char *str, void *val)
{
	return write_hex(str, (uintptr_t)val);
}

static char *write_str(char *str, const char *src)
{
	while((*str++ = *src++))
		;

	return str;
}

int lame_vsprintf(char *str, const char *format, va_list ap)
{
	bool in_control_code = false;
	char *current = str;

	for(; *format; format++) {
		if(!in_control_code) {
			if(*format == '%')
				in_control_code = true;
			else
				*current++ = *format;
		} else {
			switch(*format) {
				case 'x':
				case 'X': {
					unsigned val = va_arg(ap, unsigned);
					current = write_hex(current, val);
					break;
				}
				case 'd': {
					unsigned val = va_arg(ap, unsigned);
					current = write_dec(current, val);
					break;
				}
				case 'p': {
					void *val = va_arg(ap, void *);
					current = write_ptr(current, val);
					break;
				}
				case 's': {
					const char *val = va_arg(ap, const char *);
					current = write_str(current, val);
					break;
				}
				default:
					*current++ = *format;
					break;
			}
			in_control_code = false;
		}
	}
	*current++ = '\0';

	return current - str;
}

