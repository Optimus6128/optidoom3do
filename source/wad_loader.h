#ifndef WAD_LOADER_H
#define WAD_LOADER_H

// lump order in a map wad
enum {
	ML_THINGS,ML_LINEDEFS,ML_SIDEDEFS,ML_VERTEXES,ML_SEGS,
	ML_SSECTORS,ML_SECTORS,ML_NODES,ML_REJECT,ML_BLOCKMAP,
	ML_TOTAL
};

enum {
	CONV_INT,
	CONV_UINT,
	CONV_FIXED,
	CONV_ANGLE
};

typedef struct {
	Word start;
	Word size;
	char name[8];
} LumpInfo;

typedef struct {
	LumpInfo info;
	char *data;
	char *convertedData;
} LumpData;

typedef struct {
	LumpData *ldata;
	int lumpsNum;
} MapLumpData;

void resetMapLumpData(void);
bool isPwad(char *filepath);
void loadSelectedWadLumpInfo(char *filepath);
bool isMapReplaced(int mapNum);
void *loadLumpData(int mapNumber, const char *lumpName);
void releaseLumpData(int mapNumber, const char *lumpName);
void releasePCto3DOresourceIndexMaps(void);

extern const char* mapLumpNames[ML_TOTAL];
extern int blockListSizeFinal;

#endif
