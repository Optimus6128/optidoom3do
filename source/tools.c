#include "Doom.h"
#include "string.h"

#include "tools.h"

#define MAX_NUM_CHARS 256

bool isCharNumeric(char c)
{
	return (c >= '0' && c <= '9');
}

bool isCharLowercaseLetter(char c)
{
	return (c >= 'a' && c <= 'z');
}

const char *getUpperCaseStr(const char *name, int numChars)
{
	static char upperStr[MAX_NUM_CHARS];
	char c;
	int i = 0;
	
	if (numChars > MAX_NUM_CHARS) numChars = MAX_NUM_CHARS;

	do {
		c = name[i];
		if (isCharLowercaseLetter(c)) {
			c -= 32;
		}
		upperStr[i++] = c;
	} while(c!=0 && i < numChars);

	return upperStr;
}



Word readDiskStreamEndianSwap32(Stream *strm)
{
	Word value;

	ReadDiskStream(strm, (char*)&value, 4);

	return READ_ENDIAN_32(value);
}
