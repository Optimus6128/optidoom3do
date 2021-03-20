#include "Doom.h"
#include "string.h"
#include "stdio.h"

#include "wad_loader.h"
#include "tools.h"

#include "filestream.h"
#include "filestreamfunctions.h"

#define MAX_MAPS_NUM 32
#define MAX_BLOCKMAP_SIZE 65536


int loadingFix;

const char* mapLumpNames[ML_TOTAL] = {
	"THINGS", 
	"LINEDEFS", 
	"SIDEDEFS", 
	"VERTEXES", 
	"SEGS", 
	"SSECTORS", 
	"SECTORS", 
	"NODES", 
	"REJECT", 
	"BLOCKMAP"
};

#define TEXTURE_NAMES_NUM 74
#define FLAT_NAMES_NUM 43

#define MISSING_TEX_REPLACE 28
#define MISSING_FLAT_REPLACE 1

static int mapExMxToMAPxx[3][9] = {	1,2,3,4,5,6,7,8,24,
									9,10,11,12,13,14,15,0,23,
									16,17,18,19,20,21,22,0,0};

static int lumpEntrySizeIn[ML_TOTAL] = { 10, 14, 30, 4, 12, 4, 26, 28, -1, -1 };
static int lumpEntrySizeOut[ML_TOTAL] = { 20, 28, 24, 8, 24, 8, 28, 56, -1, -1 };

static MapLumpData mapLumpData[MAX_MAPS_NUM];


static const char textureNames[TEXTURE_NAMES_NUM][9] = {
	"BIGDOOR2", "BIGDOOR6", "BRNPOIS", "BROWNGRN", "BROWN1", "COMPSPAN", "COMPTALL", "CRATE1", "CRATELIT", "CRATINY", "DOOR1", "DOOR3", "DOORBLU",
	"DOORRED", "DOORSTOP", "DOORTRAK", "DOORYEL", "EXITDOOR", "EXITSIGN", "GRAY5", "GSTSATYR", "LITE5", "MARBFAC3", "METAL", "METAL1", "NUKE24",
	"PIPE2", "PLAT1", "SHAWN2", "SKINEDGE", "SKY1", "SKY2", "SKY3", "SLADWALL", "SP_DUDE4", "SP_HOT1", "STEP6", "SUPPORT2",
	"SUPPORT3", "SW1BRN1", "SW1GARG", "SW1GSTON", "SW1HOT", "SW1WOOD", "SW2BRN1", "SW2GARG", "SW2GSTON", "SW2HOT", "SW2WOOD", "BRICK01", "BRICK02",
	"BRICK03", "DFACE01", "MARBLE01", "MARBLE02", "MARBLE03", "MARBLE04", "WOOD01", "ASH01", "CEMENT01", "CBLUE01", "TECH01", "TECH02", "TECH03",
	"TECH04", "SKIN01", "SKIN02", "SKIN03", "SKULLS01", "STWAR01", "STWAR02", "COMTAL02", "SW1STAR", "SW2STAR"
};

static const char flatNames[FLAT_NAMES_NUM][9] = {
	"FLAT14", "FLAT23", "FLAT5_2", "FLAT5_4", "NUKAGE1", "NUKAGE2", "NUKAGE3", "FLOOR0_1", "FLOOR0_3", "FLOOR0_6", "FLOOR3_3", "FLOOR4_6", "FLOOR4_8", "FLOOR5_4",
	"STEP1", "STEP2", "FLOOR6_1", "FLOOR6_2", "TLITE6_4", "TLITE6_6", "FLOOR7_1", "FLOOR7_2", "MFLR8_1", "MFLR8_4", "CEIL3_2", "CEIL3_4", "CEIL5_1", "CRATOP1",
	"CRATOP2", "FLAT4", "FLAT8", "GATE3", "GATE4", "FWATER1", "FWATER2", "FWATER3", "FWATER4", "LAVA1", "LAVA2", "LAVA3", "LAVA4",
	"GRASS", "ROCKS"
};

int additionalTexturesNum;
int additionalFlatsNum;

bool hasLoadedAdditionalTextureIndexMaps = false;
bool hasLoadedAdditionalFlatIndexMaps = false;

char **textureNamesAdditional;
int *textureNamesAdditionalIndexMap;
char **flatNamesAdditional;
int *flatNamesAdditionalIndexMap;

char *PCto3DOtextureIndexMapFilepath = "data/maptex.bin";
char *PCto3DOflatIndexMapFilepath = "data/mapflat.bin";

static void readPCto3DOresourceIndexMaps(char *filepath, char ***names, int **indexMaps, int *num)
{
	int i, count;

	Stream *CDstream;
	CDstream = OpenDiskStream(filepath, 0);

	if (CDstream!=NULL) {
		ReadDiskStream(CDstream, (char*)&count, 4);
		*num = count;

		*indexMaps = (int*)AllocAPointer(sizeof(int) * count);
		*names = (char**)AllocAPointer(sizeof(char*) * count);
		
		for (i=0; i<count; ++i) {
			(*names)[i] = (char*)AllocAPointer(8);
			ReadDiskStream(CDstream, (*names)[i], 8);
			ReadDiskStream(CDstream, (char*)&(*indexMaps)[i], 4);
		}
		CloseDiskStream(CDstream);
	}
}

void releasePCto3DOresourceIndexMaps()
{
	int i;
	if (hasLoadedAdditionalTextureIndexMaps) {
		for (i=0; i<additionalTexturesNum; ++i) {
			DeallocAPointer(textureNamesAdditional[i]);
		}
		DeallocAPointer(textureNamesAdditional);
		DeallocAPointer(textureNamesAdditionalIndexMap);
		hasLoadedAdditionalTextureIndexMaps = false;
	}
	if (hasLoadedAdditionalFlatIndexMaps) {
		for (i=0; i<additionalFlatsNum; ++i) {
			DeallocAPointer(flatNamesAdditional[i]);
		}
		DeallocAPointer(flatNamesAdditional);
		DeallocAPointer(flatNamesAdditionalIndexMap);
		hasLoadedAdditionalFlatIndexMaps = false;
	}
}


static int getNumberOfLumpElements(LumpData *ld, int lumpId)
{
	return ld->info.size / lumpEntrySizeIn[lumpId];
}

static int getTextureIdIfMissing(const char *texName)
{
	int i = 0;
	if (loadingFix == LOADING_FIX_RELAXED) {
		if (!hasLoadedAdditionalTextureIndexMaps) {
			readPCto3DOresourceIndexMaps(PCto3DOtextureIndexMapFilepath, &textureNamesAdditional, &textureNamesAdditionalIndexMap, &additionalTexturesNum);
			hasLoadedAdditionalTextureIndexMaps = true;
		}
		while(strncmp(texName, textureNamesAdditional[i], 8) != 0) {
			if (++i == additionalTexturesNum) return MISSING_TEX_REPLACE;
		};
		return textureNamesAdditionalIndexMap[i];
	} else if (loadingFix == LOADING_FIX_ON) {
		return MISSING_TEX_REPLACE;
	}
	return -1;
}

static int getFlatIdIfMissing(const char *texName)
{
	int i = 0;
	if (loadingFix == LOADING_FIX_RELAXED) {
		if (!hasLoadedAdditionalFlatIndexMaps) {
			readPCto3DOresourceIndexMaps(PCto3DOflatIndexMapFilepath, &flatNamesAdditional, &flatNamesAdditionalIndexMap, &additionalFlatsNum);
			hasLoadedAdditionalFlatIndexMaps = true;
		}
		while(strncmp(texName, flatNamesAdditional[i], 8) != 0) {
			if (++i == additionalFlatsNum) return MISSING_FLAT_REPLACE;
		};
		return flatNamesAdditionalIndexMap[i];
	} else if (loadingFix == LOADING_FIX_ON) {
		return MISSING_FLAT_REPLACE;
	}
	return -1;
}

static int getTextureIdFromName(const char *texName)
{
	int i = 0;
	const char *texNameUpper = getUpperCaseStr(texName, 8);

	if (texNameUpper[0] == '-') return 58;	// could be -1 but right now it maps the ASH_01 texture (in the created lumps when using Versus tools)

	while(strncmp(texNameUpper, textureNames[i], 8) != 0) {
		if (++i == TEXTURE_NAMES_NUM) return getTextureIdIfMissing(texNameUpper);
	};
	return i;
}

static int getFlatIdFromName(const char *flatName)
{
	int i = 0;
	const char *flatNameUpper = getUpperCaseStr(flatName, 8);

	if (strncmp(flatNameUpper, "F_SKY1", 8) == 0) return -1;	// ID for Sky ceiling maps to -1

	while(strncmp(flatNameUpper, flatNames[i], 8) != 0) {
		if (++i == FLAT_NAMES_NUM) return getFlatIdIfMissing(flatNameUpper);
	};

	// in case of using animated textures, map to last one else they don't animate in 3DO
	if (IN_RANGE(i,4,5)) i = 6;		// NUKAGE12to3
	if (IN_RANGE(i,33,35)) i =36;	// FWATER123to4
	if (IN_RANGE(i,37,39)) i = 40;	// LAVA123to4

	return i;
}

static uint32 readEndianShortConvert(uint16 value, int conversion)
{
	const uint32 valueBack = (uint32)READ_ENDIAN_16(value);
	const int32 valueBackSigned = (int32)((int16)READ_ENDIAN_16(value));

	switch(conversion)
	{
		case CONV_FIXED:
			return valueBackSigned << 16;
		break;
		
		case CONV_ANGLE:
			return (uint32)(valueBack / 45) << 29;
		break;

		case CONV_INT:
			return valueBackSigned;
		break;

		case CONV_UINT:
		default:
			return valueBack;
		break;
	}
}

static void convertMapIdFromExMx(char *mapId, int episodeNum, int mapNum)
{
	int newMapNum = 0;
	if (IN_RANGE(episodeNum,1,3) && IN_RANGE(mapNum,1,9)) {
		newMapNum = mapExMxToMAPxx[episodeNum-1][mapNum-1];
	}
	if (newMapNum != 0) {
		sprintf(mapId, "MAP%02d", newMapNum);
	}
}

static int getMapNumberString(char *lumpName)
{
	int i;

	if (loadingFix != LOADING_FIX_OFF && lumpName[0]=='E' && lumpName[2]=='M') {
		const char c1 = lumpName[1];
		const char c3 = lumpName[3];

		if (isCharNumeric(c1) && isCharNumeric(c3)) {
			convertMapIdFromExMx(lumpName, c1-48, c3-48);
		}
	}

	// Starts with MAP
	if (strncmp(lumpName, "MAP", 3) != 0) return 0;

	// Next two chars are numeric?
	for (i=3; i<5; ++i) {
		if (!isCharNumeric(lumpName[i])) return 0;
	}

	return 10*(lumpName[3] - 48) + lumpName[4] - 48;
}

static int getLumpIdFromName(const char *lumpName)
{
	int i = 0;
	while(strncmp(lumpName, mapLumpNames[i], 8) != 0) {
		if (++i == ML_TOTAL) return -1;
	};
	return i;
}

static LumpData *findLumpInMap(MapLumpData *mld, const char *lumpName)
{
	int i = 0;
	while(strncmp(mld->ldata[i].info.name, lumpName, 8) != 0) {
		if (++i == mld->lumpsNum) return NULL;
	}
	return &mld->ldata[i];
}

static void copyLumpInfoToMapLumpData(LumpInfo *li, int lumpsNum, int mapNumber)
{
	int i;

	MapLumpData *mld = &mapLumpData[mapNumber];

	mld->ldata = (LumpData*)AllocMem(lumpsNum * sizeof(LumpData), MEMTYPE_ANY);
	mld->lumpsNum = lumpsNum;

	for (i=0; i<lumpsNum; ++i) {
		memcpy(&mld->ldata[i].info, &li[i], sizeof(LumpInfo));
	}
}

static void initMapLumpDataFromLumpInfo(LumpInfo *li, int lumpsNum)
{
	int count = 0;
	int totalCount = 0;
	int currentMapNumber;
	bool isLastLump = false;

	if (lumpsNum <= 0) return;
	currentMapNumber = getMapNumberString(li[count].name);

	do {
		const int newMapNumber = getMapNumberString(li[count].name);

		isLastLump = (totalCount == lumpsNum-1);
		if ((newMapNumber!=0 && newMapNumber != currentMapNumber) || isLastLump) {
			if (isLastLump) ++count;
			copyLumpInfoToMapLumpData(li, count, currentMapNumber);
			currentMapNumber = newMapNumber;
			li = &li[count];
			count = 0;
			continue;
		}
		++totalCount;
		++count;
	}while(!isLastLump);
}

static void convertLumpThings(const void *src, void *dst, int count)
{
	uint16 *src16 = (uint16*)src;
	uint32 *dst32 = (uint32*)dst;

	do {
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// x position
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// y position
		*dst32++ = readEndianShortConvert(*src16++, CONV_ANGLE);	// angle
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);		// thing type
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);		// flags
	} while(--count != 0);
}

static void convertLumpLinedefs(const void *src, void *dst, int count)
{
	uint16 *src16 = (uint16*)src;
	uint32 *dst32 = (uint32*)dst;

	do {
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);	// start vertex
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);	// end vertex
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);	// flags
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);	// special type
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);	// sector tag
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);	// right sidedef
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);	// left sidedef
	} while(--count != 0);
}

void convertLumpSidedefs(const void *src, void *dst, int count)
{
	uint16 *src16 = (uint16*)src;
	uint32 *dst32 = (uint32*)dst;

	do {
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// x offset
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// y offset
		*dst32++ = getTextureIdFromName((char*)src16); src16 += 4;	// upper texture
		*dst32++ = getTextureIdFromName((char*)src16); src16 += 4;	// lower texture
		*dst32++ = getTextureIdFromName((char*)src16); src16 += 4;	// middle texture
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);		// sector number
	} while(--count != 0);
}

void convertLumpVertices(const void *src, void *dst, int count)
{
	uint16 *src16 = (uint16*)src;
	uint32 *dst32 = (uint32*)dst;

	do {
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// x position
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// y position
	} while(--count != 0);
}

void convertLumpSegs(const void *src, void *dst, int count)
{
	uint16 *src16 = (uint16*)src;
	uint32 *dst32 = (uint32*)dst;

	do {
		uint32 linedefNum, direction, offset;

		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);		// Starting vertex number
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);		// Ending vertex number
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// Angle

		linedefNum = readEndianShortConvert(*src16++, CONV_INT);	// Linedef number
		direction = readEndianShortConvert(*src16++, CONV_INT);		// Direction: 0 (same as linedef) or 1 (opposite of linedef) 
		offset = readEndianShortConvert(*src16++, CONV_FIXED);		// Offset: distance along linedef to start of seg

		*dst32++ = offset;
		*dst32++ = linedefNum;
		*dst32++ = direction;
	} while(--count != 0);
}

void convertLumpSSectors(const void *src, void *dst, int count)
{
	uint16 *src16 = (uint16*)src;
	uint32 *dst32 = (uint32*)dst;

	do {
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);	// Seg count
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);	// First seg number
	} while(--count != 0);
}


void convertLumpSectors(const void *src, void *dst, int count)
{
	uint16 *src16 = (uint16*)src;
	uint32 *dst32 = (uint32*)dst;

	do {
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// Floor height
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// Ceiling height
		*dst32++ = getFlatIdFromName((char*)src16); src16 += 4;		// Name of floor texture
		*dst32++ = getFlatIdFromName((char*)src16); src16 += 4;		// Name of ceiling texture
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);		// Light level
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);		// Type
		*dst32++ = readEndianShortConvert(*src16++, CONV_INT);		// Tag number
	} while(--count != 0);
}

void convertLumpNodes(const void *src, void *dst, int count)
{
	uint16 *src16 = (uint16*)src;
	uint32 *dst32 = (uint32*)dst;

	do {
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// Partition line x coordinate
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// Partition line y coordinate
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// Change in x to end of partition line
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// Change in y to end of partition line

		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// Right bounding box
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// Left bounding box
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);
		*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);

		*dst32++ = readEndianShortConvert(*src16++, CONV_UINT);	// Right child
		*dst32++ = readEndianShortConvert(*src16++, CONV_UINT);	// Left child
	} while(--count != 0);
}

void convertLumpReject(const void *src, void *dst, int count)
{
	memcpy(dst, src, count);
}


int blockListSizeFinal = 0;

void convertLumpBlockmap(const void *src, void *dst, int count)
{
	int i;
	uint16 *src16 = (uint16*)src;
	uint32 *dst32 = (uint32*)dst;
	uint16 *srcStart = src16;

	const uint16 columnsLE = *(src16 + 2);
	const uint16 rowsLE = *(src16 + 3);
	const uint16 columns = READ_ENDIAN_16(columnsLE);
	const uint16 rows = READ_ENDIAN_16(rowsLE);

	const int numBlocks = columns * rows;
	uint32 blocklistIndex = 16 + 4 * numBlocks;
	const uint32 blocklistIndexEmpty = blocklistIndex;

	uint32 *blocklist32 = (uint32*)&dst32[blocklistIndex >> 2];

	*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// x coordinate of grid origin
	*dst32++ = readEndianShortConvert(*src16++, CONV_FIXED);	// y coordinate of grid origin
	*dst32++ = readEndianShortConvert(*src16++, CONV_UINT);		// Number of columns
	*dst32++ = readEndianShortConvert(*src16++, CONV_UINT);		// Number of rows

	// The always existing empty block
	*blocklist32++ = 0;
	*blocklist32++ = 0xFFFFFFFF;
	blocklistIndex += 8;

	for (i=0; i<numBlocks; ++i) {
		uint16 offset = *src16++;
		uint16 *blocklist16 = &srcStart[READ_ENDIAN_16(offset)];
			
		if (*blocklist16==0) {
			if (*(blocklist16+1)==0xFFFF) {
				*dst32++ = blocklistIndexEmpty;
			} else {
				uint16 val;
				*dst32++ = blocklistIndex;
				do {
					val = *blocklist16++;
					*blocklist32++ = readEndianShortConvert(val, CONV_INT);
					blocklistIndex+=4;
				}while(val!=0xFFFF);
			}
		}
	}
	blockListSizeFinal = blocklistIndex;
}

void (*convertLump[ML_TOTAL])(const void*, void*, int) = {	convertLumpThings, 
															convertLumpLinedefs, 
															convertLumpSidedefs, 
															convertLumpVertices, 
															convertLumpSegs, 
															convertLumpSSectors, 
															convertLumpSectors, 
															convertLumpNodes, 
															convertLumpReject, 
															convertLumpBlockmap};

static void convertMapLumpData(LumpData *ld)
{
	const int lumpId = getLumpIdFromName(ld->info.name);
	int numEntries = getNumberOfLumpElements(ld, lumpId);
	int sizeOut = numEntries * lumpEntrySizeOut[lumpId] + 4 * (int)(lumpId != ML_VERTEXES);
	Word *dst32;

	if (lumpId == ML_REJECT) {
		numEntries = sizeOut = ld->info.size;
	}

	if (lumpId == ML_BLOCKMAP) {
		dst32 = (Word*)AllocAPointer(MAX_BLOCKMAP_SIZE);
	} else {
		ld->convertedData = (char*)AllocAPointer(sizeOut);
		dst32 = (Word*)ld->convertedData;
		if (lumpId != ML_VERTEXES && lumpId != ML_REJECT) {
			*dst32++ = numEntries;
		}
	}

	convertLump[lumpId](ld->data, dst32, numEntries);

	if (lumpId == ML_BLOCKMAP) {
		ld->convertedData = (char*)AllocAPointer(blockListSizeFinal);
		memcpy(ld->convertedData, dst32, blockListSizeFinal);
		DeallocAPointer(dst32);
	}
}

void *loadLumpData(int mapNumber, const char *lumpName)
{
	Stream *CDstreamWad;
	int bytesRead = 0;
	LumpData *ld;

	if (wadSelected==NULL) return NULL;

	ld = findLumpInMap(&mapLumpData[mapNumber], lumpName);
	if (ld==NULL) return NULL;
	ld->data = (char*)AllocAPointer(ld->info.size);

	CDstreamWad = OpenDiskStream(wadSelected, 0);

	SeekDiskStream(CDstreamWad, ld->info.start, SEEK_SET);
	bytesRead = ReadDiskStream(CDstreamWad, ld->data, ld->info.size);
	CloseDiskStream(CDstreamWad);

	if (bytesRead != ld->info.size) {
		DeallocAPointer(ld->data);
		return NULL;
	}

	convertMapLumpData(ld);

	DeallocAPointer(ld->data);
	return ld->convertedData;
}

void releaseLumpData(int mapNumber, const char *lumpName)
{
	LumpData *ld = findLumpInMap(&mapLumpData[mapNumber], lumpName);
	if (ld!=NULL && ld->convertedData!=NULL) {
		DeallocAPointer(ld->convertedData);
		ld->convertedData = NULL;
	}
}

bool isMapReplaced(int mapNum)
{
	if (mapNum <= 0 || mapNum >= MAX_MAPS_NUM) return false;

	return (mapLumpData[mapNum].ldata != NULL);
}

void loadSelectedWadLumpInfo(char *filepath)
{
	Stream *CDstreamWad;
	Word lumpStart;
	int wadLumpsNum = 0;
	int i;

	CDstreamWad = OpenDiskStream(filepath, 0);

	if (CDstreamWad != NULL) {
		LumpInfo *tempLumpInfo;

		SeekDiskStream(CDstreamWad, 4, SEEK_SET);
		wadLumpsNum = readDiskStreamEndianSwap32(CDstreamWad);
		lumpStart = readDiskStreamEndianSwap32(CDstreamWad);

		tempLumpInfo = (LumpInfo*)AllocMem(wadLumpsNum * sizeof(LumpInfo), MEMTYPE_TRACKSIZE);
		SeekDiskStream(CDstreamWad, lumpStart, SEEK_SET);
		for (i=0; i<wadLumpsNum; ++i) {
			ReadDiskStream(CDstreamWad, (char*)&tempLumpInfo[i], sizeof(LumpInfo));
			tempLumpInfo[i].size = READ_ENDIAN_32(tempLumpInfo[i].size);
			tempLumpInfo[i].start = READ_ENDIAN_32(tempLumpInfo[i].start);
		}
		CloseDiskStream(CDstreamWad);

		initMapLumpDataFromLumpInfo(tempLumpInfo, wadLumpsNum);
		FreeMem(tempLumpInfo, -1);
	}
}

bool isPwad(char *filepath)
{
	Stream *CDstreamWad = OpenDiskStream(filepath, 0);

	if (CDstreamWad != NULL) {
		char wadId[4];
		char *hasPwadId = NULL;

		ReadDiskStream(CDstreamWad, wadId, 4);
		CloseDiskStream(CDstreamWad);
		hasPwadId = strstr(wadId, "WAD");
		return (hasPwadId != NULL);
	}
	return false;
}

void resetMapLumpData()
{
	int i;
	for (i=0; i<MAX_MAPS_NUM; ++i) {
		mapLumpData[i].ldata = NULL;
		mapLumpData[i].lumpsNum = 0;
	}
}

// NOTES

// I'd like to do now
// ==================
// 4 directional scrolling for floors(2bits) and walls(extra)

// Things that could need more work
// ================================
// Investigate which things/line/sector effects exist and not in Doombuilder compared to 3DO Doom. Could you implement some of them in the future? Or simply map them to others?

// Fix later
// =========
// Optimize more the blockmap creation with search for already existing blocklists of lines.
// Investigate why the BIGDOOR2 resource is missing in DoomBuilder, also add it if so
// Generate texture denoting missing resources?
