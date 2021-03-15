#ifndef TOOLS_H
#define TOOLS_H

#include "filestream.h"
#include "filestreamfunctions.h"

#define IN_RANGE(a,min,max) (a >= min && a <= max)
#define READ_ENDIAN_16(a) ((a >> 8) | ((a << 8) & 0xFF00))
#define READ_ENDIAN_32(a) ((a >> 24) | ((a >> 8) & 0x0000FF00) | ((a << 8) & 0x00FF0000) | (a << 24))

bool isCharNumeric(char c);
bool isCharLowercaseLetter(char c);
const char *getUpperCaseStr(const char *name, int numChars);

Word readDiskStreamEndianSwap32(Stream *strm);

#endif
