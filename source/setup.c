#include "Doom.h"
#include "wad_loader.h"
#include <IntMath.h>
#include <String.h>
//#include <math.h>


static bool loadPWad = false;	// true if resource loading is going to be overriden by PWAD
static Word pWadMapNum = 0;		// map number to load

static vertex_t *vertexes;	/* Only needed during load, then discarded before game play */
static Word LoadedLevel;	/* Resource number of the loaded level */
static Word numsides;	/* Number of sides loaded */
static side_t *sides;		/* Pointer to array of loaded sides */
static line_t **LineArrayBuffer;	/* Pointer to array of line_t pointers used by sectors */
static Word PreLoadTable[] = {
	rSPR_ZOMBIE,			/* Zombiemen */
	rSPR_SHOTGUY,			/* Shotgun guys */
	rSPR_IMP,				/* Imps */
	rSPR_DEMON,				/* Demons */
	rSPR_CACODEMON,			/* Cacodemons */
	rSPR_LOSTSOUL,			/* Lost souls */
	rSPR_BARON,				/* Baron of Hell */
	rSPR_OURHEROBDY,		/* Our dead hero */
	rSPR_BARREL,			/* Exploding barrel */
	rSPR_SHOTGUN,			/* Shotgun on floor */
	rSPR_CLIP,				/* Clip of bullets */
	rSPR_SHELLS,			/* 4 shotgun shells */
	rSPR_STIMPACK,			/* Stimpack */
	rSPR_MEDIKIT,			/* Med-kit */
	rSPR_GREENARMOR,		/* Normal armor */
	rSPR_BLUEARMOR,			/* Mega armor */
	rSPR_HEALTHBONUS,		/* Health bonus */
	rSPR_ARMORBONUS,		/* Armor bonus */
	rSPR_BLUD,				/* Blood from bullet hit */
	rSPR_PUFF,				/* Gun sparks on wall */
	-1
};

seg_t *segs;				/* Pointer to array of loaded segs */
Word numsectors;			/* Number of sectors loaded */
sector_t *sectors;			/* Pointer to array of loaded sectors */
subsector_t *subsectors;	/* Pointer to array of loaded subsectors */
node_t *FirstBSPNode;	/* First BSP entry */
Word numlines;		/* Number of lines loaded */
line_t *lines;		/* Pointer to array of loaded lines */
line_t ***BlockMapLines;	/* Pointer to line lists based on blockmap */
Word BlockMapWidth,BlockMapHeight;	/* Size of blockmap in blocks */
Fixed BlockMapOrgX,BlockMapOrgY;	/* Origin of block map */
mobj_t **BlockLinkPtr;		/* Starting link for thing chains */
Byte *RejectMatrix;			/* For fast sight rejection */
mapthing_t deathmatchstarts[10],*deathmatch_p;	/* Deathmatch starts */
mapthing_t playerstarts;	/* Starting position for players */

uint16 flatTextureColors[MAX_UNIQUE_TEXTURES];

/**********************************

	Load the sector data, this is loaded in first since it
	doesn't require the presence of any other data type.

**********************************/

typedef	struct {		/* Map sector loaded from disk */
	Fixed floorheight;	/* Floor and ceiling height */
	Fixed ceilingheight;
	Word floorpic;		/* Floor image */
	Word ceilingpic;	/* Ceiling image */
	Word lightlevel;	/* Light level */
	Word special;		/* Special flags */
	Word tag;			/* Tag ID */
} mapsector_t;

static Word extractColorFromSpecial(Word special)
{
	// FOG = 32
	// DIST = 64
	// WARP = 128

	// 2 R * 16384
	// 2 G * 4096
	// 2 B * 1024

	//RRGGBBSW WDF11111
	//11111111 10100000 (SEC_SPEC_RENDER_BITS = 0xFFA0: bits relevant to rendering for splitting visplanes)
	
	// SWW
	// 001	warp floor
	// 010	warp ceiling
	// 011	warp both
	// 100	scroll +x
	// 101	scroll -x
	// 110	scroll +y
	// 111	scroll -y

	static int col[4] = {0, 85, 170, 255};
	
	const int r = col[(special >> 14) & 3];
	const int g = col[(special >> 12) & 3];
	const int b = col[(special >> 10) & 3];

	return (r << 16) | (g << 8) | b;
}

static bool shouldOverrideLump(Word lumpId)
{
	switch(lumpId) {
		case ML_THINGS:
		case ML_LINEDEFS:
		case ML_SIDEDEFS:
		case ML_VERTEXES:
		case ML_SEGS:
		case ML_SSECTORS:
		case ML_SECTORS:
		case ML_NODES:
		case ML_REJECT:
		case ML_BLOCKMAP:
			return true;
		default:
			return false;
	}
}


static void *LoadALump(Word lumpStart, Word lumpId)
{
	if (loadPWad && shouldOverrideLump(lumpId)) {
		void *lumpData = loadLumpData(pWadMapNum, mapLumpNames[lumpId]);
		if (lumpData != NULL) {
			return lumpData;
		}
	}
	return LoadAResource(lumpStart + lumpId);
}

static void KillALump(Word lumpStart, Word lumpId)
{
	if (loadPWad && shouldOverrideLump(lumpId)) {
		releaseLumpData(pWadMapNum, mapLumpNames[lumpId]);
	} else {
		Word lump = lumpStart + lumpId;
		KillAResource(lump);
	}
}

enum {
	NO_LIQUID,
	ACID_LIQUID,
	WATER_LIQUID,
	LAVA_LIQUID
};

static int getLiquidSectorType(int index)
{
	if (index>=4 && index <= 6) return ACID_LIQUID;
	if (index>=33 && index <= 36) return WATER_LIQUID;
	if (index>=37 && index <= 40) return LAVA_LIQUID;
	return NO_LIQUID;
}

static void LoadSectors(Word lumpStart, Word lumpId)
{
	Word i;
	mapsector_t *Map;
	sector_t *ss;

	Map = (mapsector_t *)LoadALump(lumpStart, lumpId);		/* Load the data in */
	numsectors = ((Word *)Map)[0];			/* Get the number of entries */
	Map = (mapsector_t *)&((Word*)Map)[1];	/* Index past the entry count */
	i = numsectors*sizeof(sector_t);	/* Get the data size */
	ss = (sector_t *)AllocAPointer(i);		/* Get some memory */
	memset(ss,0,i);		/* Clear out the memory (Extra fields present) */
	sectors = ss;		/* Save the sector data */
	i = numsectors;		/* Init the loop count */
	do {
		ss->floorheight = Map->floorheight;		/* Copy the floor height */
		ss->ceilingheight = Map->ceilingheight;	/* Copy the ceiling height */
		ss->FloorPic = Map->floorpic;			/* Copy the floor texture # */
		ss->CeilingPic = Map->ceilingpic;		/* Copy the ceiling texture # */
		ss->lightlevel = Map->lightlevel;		/* Copy the ambient light */
		ss->special = Map->special;				/* Copy the event number type */
		ss->tag = Map->tag;						/* Copy the event tag ID */

		{
			const int floorLiquidType = getLiquidSectorType(ss->FloorPic);
			const int ceilingLiquidType = getLiquidSectorType(ss->CeilingPic);

			if (enableWaterFx) {
				if (floorLiquidType!=NO_LIQUID) {
					ss->special |= 0x080;	// warp floor
				}
				if (ceilingLiquidType!=NO_LIQUID) {
					ss->special |= 0x100;	// warp ceiling
				}
			}

			if (enableSectorColors) {
				if (floorLiquidType==ACID_LIQUID || ceilingLiquidType==ACID_LIQUID) {
					ss->special |= 0xB800;
				}
				if (floorLiquidType==WATER_LIQUID || ceilingLiquidType==WATER_LIQUID) {
					ss->special |= 0xAC00;
				}
				if (floorLiquidType==LAVA_LIQUID || ceilingLiquidType==LAVA_LIQUID) {
					ss->special |= 0xE800;
				}
			}

			ss->color = extractColorFromSpecial(ss->special);
		}

		++ss;			/* Next indexs */
		++Map;
	} while (--i);		/* All done? */
	KillALump(lumpStart, lumpId);	/* Dispose of the original */
}

/**********************************

	Load in the wall side definitions
	Requires that the sectors are loaded.

**********************************/

typedef struct {		/* Map sidedef loaded from disk */
	Fixed textureoffset;
	Fixed rowoffset;
	Word toptexture,bottomtexture,midtexture;
	Word sector;		/* on viewer's side */
} mapsidedef_t;

static void LoadSideDefs(Word lumpStart, Word lumpId)
{
	Word i;
	mapsidedef_t *MapSide;
	side_t *sd;

	MapSide = (mapsidedef_t *)LoadALump(lumpStart, lumpId);	/* Load in the data */
	numsides = ((Word *)MapSide)[0];			/* Get the side count */		
	MapSide = (mapsidedef_t *)&((Word *)MapSide)[1];	/* Index to the array */

	i = numsides*sizeof(side_t);		/* How much data do I need? */
	sd = (side_t *)AllocAPointer(i);	/* Allocate it */
	sides=sd;				/* Save the data */
	i = numsides;			/* Number of sides to process */
	do {
		sd->textureoffset = MapSide->textureoffset;	/* Copy the texture X offset */
		sd->rowoffset = MapSide->rowoffset;		/* Copy the texture Y offset */
		sd->toptexture = MapSide->toptexture;	/* Topmost texture */
		sd->bottomtexture = MapSide->bottomtexture;	/* Bottommost texture */
		sd->midtexture = MapSide->midtexture;	/* Center texture */
		sd->sector = &sectors[MapSide->sector];	/* Parent sector */
		++MapSide;		/* Next indexs */
		++sd;
	} while (--i);		/* Count down */
	KillALump(lumpStart, lumpId);	/* Release the memory */
}

/**********************************

	Load in all the line definitions.
	Requires vertexes,sectors and sides to be loaded.
	I also calculate the line's slope for quick processing by line
	slope comparisons.
	I also calculate the line's bounding box in Fixed pixels.

**********************************/

typedef struct {
	Word v1,v2;		/* Indexes to the vertex table */
	Word flags;		/* Line flags */
	Word special;	/* Special event type */
	Word tag;		/* ID tag for external trigger */
	Word sidenum[2];	/* sidenum[1] will be -1 if one sided */
} maplinedef_t;

static void LoadLineDefs(Word lumpStart, Word lumpId)
{
	Word i;
	maplinedef_t *mld;
	line_t *ld;

	mld = (maplinedef_t *)LoadALump(lumpStart, lumpId);	/* Load in the data */
	numlines = ((Word*)mld)[0];			/* Get the number of lines in the struct array */	
	i = numlines*sizeof(line_t);		/* Get the size of the dest buffer */
	ld = (line_t *)AllocAPointer(i);	/* Get the memory for the lines array */
	lines = ld;					/* Save the lines */
	memset(ld,0,i);				/* Blank out the buffer */
	mld = (maplinedef_t *)&((Word *)mld)[1];	/* Index to the first record of the struct array */
	i = numlines;			
	do {
		Fixed dx,dy;
		ld->flags = mld->flags;		/* Copy the flags */
		ld->special = mld->special;	/* Copy the special type */
		ld->tag = mld->tag;			/* Copy the external tag ID trigger */
		ld->v1 = vertexes[mld->v1];		/* Copy the end points to the line */
		ld->v2 = vertexes[mld->v2];
		dx = ld->v2.x - ld->v1.x;	/* Get the delta offset (Line vector) */
		dy = ld->v2.y - ld->v1.y;
		
		/* What type of line is this? */
		
		if (!dx) {				/* No x motion? */
			ld->slopetype = ST_VERTICAL;		/* Vertical line only */
		} else if (!dy) {		/* No y motion? */
			ld->slopetype = ST_HORIZONTAL;		/* Horizontal line only */
		} else {
			if ((dy^dx) >= 0) {	/* Is this a positive or negative slope */
				ld->slopetype = ST_POSITIVE;	/* Like signs, positive slope */
			} else {
				ld->slopetype = ST_NEGATIVE;	/* Unlike signs, negative slope */
			}
		}
		
		/* Create the bounding box */
		
		if (dx>=0) {			/* V2>=V1? */
			ld->bbox[BOXLEFT] = ld->v1.x;		/* Leftmost x */
			ld->bbox[BOXRIGHT] = ld->v2.x;		/* Rightmost x */
		} else {
			ld->bbox[BOXLEFT] = ld->v2.x;
			ld->bbox[BOXRIGHT] = ld->v1.x;
		}
		if (dy>=0) {
			ld->bbox[BOXBOTTOM] = ld->v1.y;		/* Bottommost y */
			ld->bbox[BOXTOP] = ld->v2.y;		/* Topmost y */
		} else {
			ld->bbox[BOXBOTTOM] = ld->v2.y;
			ld->bbox[BOXTOP] = ld->v1.y;
		}
		
		/* Copy the side numbers and sector pointers */
		
		ld->SidePtr[0] = &sides[mld->sidenum[0]];		/* Get the side number */
		ld->frontsector = ld->SidePtr[0]->sector;	/* All lines have a front side */	
		if (mld->sidenum[1] != -1) {				/* But maybe not a back side */
			ld->SidePtr[1] = &sides[mld->sidenum[1]];
			ld->backsector = ld->SidePtr[1]->sector;	/* Get the sector pointed to */
		}

		++ld;			/* Next indexes */
		++mld;
	} while (--i);
	KillALump(lumpStart, lumpId);	/* Release the resource */
}

/**********************************

	Load in the block map
	I need to have the lines preloaded before I can process the blockmap
	The data has 4 longwords at the beginning which will have an array of offsets
	to a series of line #'s. These numbers will be turned into pointers into the line
	array for quick compares to lines.
	
**********************************/

static void LoadBlockMap(Word lumpStart, Word lumpId)
{
	Word Count;
	Word Entries;
	Byte *MyLumpPtr;
	void **BlockHandle = NULL;
	LongWord *StartIndex;
	Word lump = lumpStart + lumpId;

	if (loadPWad && shouldOverrideLump(lumpId)) {
		MyLumpPtr = (Byte *)LoadALump(lumpStart, lumpId);
	} else {
		BlockHandle = LoadAResourceHandle(lump);	/* Load the data */
		MyLumpPtr = (Byte *)LockAHandle(BlockHandle);
	}


	BlockMapOrgX = ((Word *)MyLumpPtr)[0];		/* Get the orgx and y */
	BlockMapOrgY = ((Word *)MyLumpPtr)[1];
	BlockMapWidth = ((Word *)MyLumpPtr)[2];		/* Get the map size */
	BlockMapHeight = ((Word *)MyLumpPtr)[3];

	Entries = BlockMapWidth*BlockMapHeight;	/* How many entries are there? */
	
/* Convert the loaded block map table into a huge array of block entries */

	Count = Entries;			/* Init the longword count */
	StartIndex = &((LongWord *)MyLumpPtr)[4];	/* Index to the offsets! */
	BlockMapLines = (line_t ***)StartIndex;		/* Save in global */
	do {
		StartIndex[0] = (LongWord)&MyLumpPtr[StartIndex[0]];	/* Convert to pointer */
		++StartIndex;		/* Next offset entry */
	} while (--Count);		/* All done? */	

/* Convert the lists appended to the array into pointers to lines */

	if (loadPWad && shouldOverrideLump(lumpId)) {
		Count = blockListSizeFinal/4;
	} else {
		Count = GetAHandleSize(BlockHandle)/4;		/* How much memory is needed? (Longs) */
	}
	Count -= (Entries+4);		/* Remove the header count */
	do {
		if (StartIndex[0]!=-1) {	/* End of a list? */
			StartIndex[0] = (LongWord)&lines[StartIndex[0]];	/* Get the line pointer */
		} else {
			StartIndex[0] = 0;	/* Insert a null pointer */
		}
		++StartIndex;	/* Next entry */
	} while (--Count);
	
/* Clear out mobj chains */

	Count = sizeof(*BlockLinkPtr)*Entries;	/* Get memory */
	BlockLinkPtr = (mobj_t **)AllocAPointer(Count);	/* Allocate memory */
	memset(BlockLinkPtr,0,Count);			/* Clear it out */
}

/**********************************

	Load the line segments structs for rendering
	Requires vertexes,sides and lines to be preloaded

**********************************/

typedef struct {
	Word v1,v2;			/* Index to the vertexs */
	angle_t	angle;		/* Angle of the line segment */
	Fixed offset;		/* Texture offset */
	Word linedef;		/* Line definition */
	Word side;			/* Side of the line segment */
} mapseg_t;

static void LoadSegs(Word lumpStart, Word lumpId)
{
	Word i;
	mapseg_t *ml;
	seg_t *li;
	Word numsegs;
	
	ml = (mapseg_t *)LoadALump(lumpStart, lumpId);		/* Load in the map data */
	numsegs = ((Word*)ml)[0];		/* Get the count */
	i = numsegs*sizeof(seg_t);		/* Get the memory size */
	li = (seg_t *)AllocAPointer(i);	/* Allocate it */
	segs = li;				/* Save pointer to global */				
	memset(li,0,i);			/* Clear it out */

	ml = (mapseg_t *)&((Word *)ml)[1];	/* Init pointer to first record */
	i = 0;
	do {
		line_t *ldef;
		Word side;
		//Fixed dx,dy;

		li->v1 = vertexes[ml->v1];	/* Get the line points */
		li->v2 = vertexes[ml->v2];
		li->angle = ml->angle;		/* Set the angle of the line */
		li->offset = ml->offset;	/* Get the texture offset */
		ldef = &lines[ml->linedef];	/* Get the line pointer */
		li->linedef = ldef;
		side = ml->side;		/* Get the side number */
		li->sidedef = ldef->SidePtr[side];	/* Grab the side pointer */
		li->frontsector = li->sidedef->sector;	/* Get the front sector */
		if (ldef->flags & ML_TWOSIDED) {		/* Two sided? */
			li->backsector = ldef->SidePtr[side^1]->sector;	/* Mark the back sector */
		}
		if (ldef->v1.x == li->v1.x && ldef->v1.y == li->v1.y) {	/* Init the fineangle */
			ldef->fineangle = li->angle>>ANGLETOFINESHIFT;	/* This is a point only */
		}

		/*dx = (li->v2.x - li->v1.x) >> 16;
		dy = (li->v2.y - li->v1.y) >> 16;
		li->length = (int)sqrt(dx*dx + dy*dy);*/

		++li;		/* Next entry */
		++ml;		/* Next resource entry */
	} while (++i<numsegs);
	KillALump(lumpStart, lumpId);	/* Release the resource */
}

/**********************************

	Load in all the subsectors
	Requires segs, sectors, sides

**********************************/

typedef struct {		/* Loaded map subsectors */
	Word numlines;		/* Number of line segments */
	Word firstline;		/* Segs are stored sequentially */
} mapsubsector_t;

static void LoadSubsectors(Word lumpStart, Word lumpId)
{
	Word numsubsectors;
	Word i;
	mapsubsector_t *ms;
	subsector_t *ss;

	ms = (mapsubsector_t *)LoadALump(lumpStart, lumpId);	/* Get the map data */
	numsubsectors = ((Word*)ms)[0];		/* Get the subsector count */
	i = numsubsectors*sizeof(subsector_t);	/* Calc needed buffer */
	ss = (subsector_t *)AllocAPointer(i);	/* Get the memory */
	subsectors=ss;		/* Save in global */
	ms = (mapsubsector_t *)&((Word *)ms)[1];	/* Index to first entry */
	i = numsubsectors;
	do {
		seg_t *seg;
		ss->numsublines = ms->numlines;	/* Number of lines in the sub sectors */
		seg = &segs[ms->firstline];		/* Get the first line segment pointer */
		ss->firstline = seg;			/* Save it */
		ss->sector = seg->sidedef->sector;	/* Get the parent sector */
		++ss;		/* Index to the next entry */
		++ms;
	} while (--i);
	KillALump(lumpStart, lumpId);
}

/**********************************

	Load in the BSP tree and convert the indexs into pointers
	to either the node list or the subsector array.
	I require that the subsectors are loaded in.

**********************************/

#define	NF_SUBSECTOR 0x8000

typedef struct {
	Fixed x,y,dx,dy;	/* Partition vector */
	Fixed bbox[2][4];	/* Bounding box for each child */
	LongWord children[2];	/* if NF_SUBSECTOR it's a subsector index else node index */
} mapnode_t;

static void LoadNodes(Word lumpStart, Word lumpId)
{
	Word numnodes;		/* Number of BSP nodes */
	Word i,j,k;
	mapnode_t *mn;
	node_t *no;	
	node_t *nodes;

	mn = (mapnode_t *)LoadALump(lumpStart, lumpId);	/* Get the data */
	numnodes = ((Word*)mn)[0];			/* How many nodes to process */
	mn = (mapnode_t *)&((Word *)mn)[1];
	no = (node_t *)mn;
	nodes = no;
	i = numnodes;			/* Get the node count */
	do {
		j = 0;
		do {
			k = (Word)mn->children[j];	/* Get the child offset */
			if (k&NF_SUBSECTOR) {		/* Subsector? */
				k&=(~NF_SUBSECTOR);		/* Clear the flag */
				no->Children[j] = (void *)((LongWord)&subsectors[k]|1);
			} else {	
				no->Children[j] = &nodes[k];	/* It's a node offset */
			}
		} while (++j<2);		/* Both branches done? */
		++no;		/* Next index */
		++mn;
	} while (--i);
	FirstBSPNode = no-1;	/* The last node is the first entry into the tree */
}

/**********************************

	Builds sector line lists and subsector sector numbers
	Finds block bounding boxes for sectors

**********************************/

static void GroupLines(void)
{
	line_t **linebuffer;	/* Pointer to linebuffer array */
	Word total;		/* Number of entries needed for linebuffer array */
	line_t *li;		/* Pointer to a work line record */
	Word i,j;
	sector_t *sector;	/* Work sector pointer */
	Fixed block;	/* Clipped bounding box value */
	Fixed bbox[4];

/* count number of lines in each sector */

	li = lines;		/* Init pointer to line array */
	total = 0;		/* How many line pointers are needed for sector line array */
	i = numlines;	/* How many lines to process */
	do {
		li->frontsector->linecount++;	/* Inc the front sector's line count */
		if (li->backsector && li->backsector != li->frontsector) {	/* Two sided line? */
			li->backsector->linecount++;	/* Add the back side referance */
			++total;	/* Inc count */
		}
		++total;	/* Inc for the front */
		++li;		/* Next line down */
	} while (--i);

/* Build line tables for each sector */

	linebuffer = (line_t **)AllocAPointer(total*sizeof(line_t*));
	LineArrayBuffer = linebuffer;	/* Save in global for later disposal */
	sector = sectors;		/* Init the sector pointer */
	i = numsectors;		/* Get the sector count */
	do {
		bbox[BOXTOP] = bbox[BOXRIGHT] = MININT;	/* Invalidate the rect */
		bbox[BOXBOTTOM] = bbox[BOXLEFT] = MAXINT;
		sector->lines = linebuffer;	/* Get the current list entry */
		li = lines;		/* Init the line array pointer */
		j = numlines;
		do {
			if (li->frontsector == sector || li->backsector == sector) {
				linebuffer[0] = li;	/* Add the pointer to the entry list */
				++linebuffer;		/* Add to the count */
				AddToBox(bbox,li->v1.x,li->v1.y);	/* Adjust the bounding box */
				AddToBox(bbox,li->v2.x,li->v2.y);	/* Both points */
			}
			++li;
		} while (--j);		/* All done? */

		/* Set the sound origin to the center of the bounding box */

		sector->SoundX = (bbox[BOXRIGHT]+bbox[BOXLEFT])/2;	/* Get average */
		sector->SoundY = (bbox[BOXTOP]+bbox[BOXBOTTOM])/2;	/* This is SIGNED! */

		/* Adjust bounding box to map blocks and clip to unsigned values */

		block = (bbox[BOXTOP]-BlockMapOrgY+MAXRADIUS)>>MAPBLOCKSHIFT;
		++block;
		block = (block > (int)BlockMapHeight) ? BlockMapHeight : block;
		sector->blockbox[BOXTOP]=block;		/* Save the topmost point */

		block = (bbox[BOXBOTTOM]-BlockMapOrgY-MAXRADIUS)>>MAPBLOCKSHIFT;
		block = (block < 0) ? 0 : block;
		sector->blockbox[BOXBOTTOM]=block;	/* Save the bottommost point */

		block = (bbox[BOXRIGHT]-BlockMapOrgX+MAXRADIUS)>>MAPBLOCKSHIFT;
		++block;
		block = (block > (int)BlockMapWidth) ? BlockMapWidth : block;
		sector->blockbox[BOXRIGHT]=block;	/* Save the rightmost point */

		block = (bbox[BOXLEFT]-BlockMapOrgX-MAXRADIUS)>>MAPBLOCKSHIFT;
		block = (block < 0) ? 0 : block;
		sector->blockbox[BOXLEFT]=block;	/* Save the leftmost point */
		++sector;
	} while (--i);
}

/**********************************

	Spawn items and critters

**********************************/

static void LoadThings(Word lumpStart, Word lumpId)
{
	Word i;
	mapthing_t *mt = (mapthing_t *)LoadALump(lumpStart, lumpId);	/* Load the thing list */
	i = ((Word*)mt)[0];			/* Get the count */
	mt = (mapthing_t *)&((Word *)mt)[1];	/* Point to the first entry */
	do {
		SpawnMapThing(mt);	/* Spawn the thing */
		++mt;		/* Next item */
	} while (--i);	/* More? */
	KillALump(lumpStart, lumpId);	/* Release the list */
}

/**********************************

	Draw the word "Loading" on the screen

**********************************/

static void LoadingPlaque(void)
{
	DrawPlaque(rLOADING);	/* Show the loading logo */
}

/**********************************

	Preload all the wall and flat shapes

**********************************/

static void calculateWallTextureAverageColor(texture_t *tex)
{
	Byte *rawData = (Byte *)*tex->data;
	uint16 *texPal = (uint16*)&rawData[0];
	Byte *texBitmap = &rawData[32];
	int col;

	int i;
	const unsigned int numPixels = tex->width * tex->height;
	const unsigned int size = numPixels >> 1;	// 4bit

	unsigned int sumR = 0;
	unsigned int sumG = 0;
	unsigned int sumB = 0;

	for (i=0; i<size; ++i) {
        const Byte p = texBitmap[i];
        const Byte p0 = p >> 4;
        const Byte p1 = p & 15;
        const uint16 c0 = texPal[p0];
        const uint16 c1 = texPal[p1];

        sumR += ((c0 >> 10) & 31) + ((c1 >> 10) & 31);
        sumG += ((c0 >> 5) & 31) + ((c1 >> 5) & 31);
        sumB += (c0 & 31) + (c1 & 31);
	}
	if (numPixels > 0) {
		sumR /= numPixels;
		sumG /= numPixels;
		sumB /= numPixels;
	}

	col = ((sumR << 10) | (sumG << 5) | sumB) & 32767;
	tex->color = (col << 16) | col;
}

static void calculateFlatTextureAverageColor(void **tex, int index)
{
	Byte *rawData = (Byte *)*tex;
	uint16 *texPal = (uint16*)&rawData[0];
	Byte *texBitmap = &rawData[64];

	int i;
	const unsigned int numPixels = 64 * 64; // flats are always 64*64
	const unsigned int size = numPixels;	// 8bit

	unsigned int sumR = 0;
	unsigned int sumG = 0;
	unsigned int sumB = 0;

	for (i=0; i<size; ++i) {
        const Byte p = texBitmap[i];
        const uint16 c = texPal[p];

        sumR += ((c >> 10) & 31);
        sumG += ((c >> 5) & 31);
        sumB += (c & 31);
	}
	if (numPixels > 0) {
		sumR /= numPixels;
		sumG /= numPixels;
		sumB /= numPixels;
	}

	flatTextureColors[index] = (uint16)(((sumR << 10) | (sumG << 5) | sumB) & 32767);
}

static void PreloadWalls(void)
{
	Word i;				/* Index */
	texture_t *TexPtr;
	Boolean TextureLoadFlags[MAX_UNIQUE_TEXTURES];		/* Which textures should I load? */

    memset(TextureLoadFlags,0,sizeof(TextureLoadFlags));		/* Set to zilch */

	i = numsides;		/* How many side do I have? */
	if (i) {
		Word Tex;		/* Temp */
		side_t *sd;		/* Pointer to sidedef table */

		sd = sides;		/* Init the pointer */
		do {
			Tex = sd->toptexture;		/* Is there a top texture? */
			if (Tex<NumTextures) {
				TextureLoadFlags[Tex] = TRUE;	/* Load it in */
			}
			Tex = sd->midtexture;
			if (Tex<NumTextures) {
				TextureLoadFlags[Tex] = TRUE;
			}
			Tex = sd->bottomtexture;
			if (Tex<NumTextures) {
				TextureLoadFlags[Tex] = TRUE;
			}
			++sd;		/* Next side def */
		} while (--i);	/* All done? */
	}

	/* Now, scan the walls for switches */
	
	i = NumSwitches;
	if (i) {
		do {
			--i;
			if (TextureLoadFlags[SwitchList[i]]) {		/* Found a switch? */
				TextureLoadFlags[SwitchList[i^1]] = TRUE;	/* Get the alternate */
			}
		} while (i);	/* Any more? */
	}

	/* Now load in the walls */
	
	i = 0;			/* Init index */
	TexPtr = TextureInfo;		/* Init texture table */
	do {
		if (TextureLoadFlags[i]) {	/* Load it in? */
			TexPtr->data = LoadAResourceHandle(i+FirstTexture);	/* Get it */
			calculateWallTextureAverageColor(TexPtr);
		}
		++TexPtr;
	} while (++i<NumTextures);

	/* Now scan for the flats */
	
	memset(TextureLoadFlags,0,sizeof(TextureLoadFlags));		/* Set to zilch */
	i = numsectors;
	if (i) {
		Word Tex;		/* Temp */
		sector_t *sd;		/* Pointer to sidedef table */

		sd = sectors;		/* Init the pointer */
		do {
			TextureLoadFlags[sd->FloorPic] = TRUE;	/* Load it in */
			Tex = sd->CeilingPic;
			if (Tex<NumFlats) {		/* Make sure it's ok */
				TextureLoadFlags[Tex] = TRUE;
			}
			++sd;		/* Next side def */
		} while (--i);	/* All done? */
	}
	i = NumFlatAnims;
	if (i) {
		anim_t *sd;
		Word j;
		sd = FlatAnims;
		do {
			if (TextureLoadFlags[sd->LastPicNum]) {
				j = sd->BasePic;
				do {
					TextureLoadFlags[j] = TRUE;
				} while (++j<=sd->LastPicNum);
			}
			++sd;
		} while (--i);
	}
	
	i = 0;			/* Init index */
	do {
		if (TextureLoadFlags[i]) {	/* Load it in? */
			FlatInfo[i] = LoadAResourceHandle(i+FirstFlat);	/* Get it */
			calculateFlatTextureAverageColor(FlatInfo[i], i);
		}
	} while (++i<NumFlats);
	memcpy(FlatTranslation,FlatInfo,sizeof(Byte *)*NumFlats);
	i = 0;
	do {
		LoadAResourceHandle(PreLoadTable[i]);
		ReleaseAResource(PreLoadTable[i]);
		++i;
	} while (PreLoadTable[i]!=-1);
}

/**********************************

	Load and prepare the game level

**********************************/

void SetupLevel(Word map)
{
	const int barMax = 11;
	int barCurrent = 0;

	Word lumpnum;
	player_t *p;

	pWadMapNum = map;
	loadPWad = isMapReplaced(map);

	Randomize();			/* Reset the random number generator */
	LoadingPlaque();		/* Display "Loading" */
	PurgeHandles(0);		/* Purge memory */
	CompactHandles();		/* Pack remaining memory */
	TotalKillsInLevel = ItemsFoundInLevel = SecretsFoundInLevel = 0;
	p = &players;
	p->killcount = 0;		/* Nothing killed */
	p->secretcount = 0;		/* No secrets found */
	p->itemcount = 0;		/* No items found */
	
	InitThinkers();			/* Zap the think logics */


	lumpnum = ((map-1)*ML_TOTAL)+rMAP01;	/* Get the map number */
	LoadedLevel = lumpnum;		/* Save the loaded resource number */

/* Note: most of this ordering is important */

drawLoadingBar(barCurrent++, barMax, "VERTEXES");
	vertexes = (vertex_t *)LoadALump(lumpnum, ML_VERTEXES);	/* Load the map vertexes */
drawLoadingBar(barCurrent++, barMax, "SECTORS");
	LoadSectors(lumpnum, ML_SECTORS);	/* Needs nothing */
drawLoadingBar(barCurrent++, barMax, "SIDEDEFS");
	LoadSideDefs(lumpnum, ML_SIDEDEFS);	/* Needs sectors */

	releasePCto3DOresourceIndexMaps();

drawLoadingBar(barCurrent++, barMax, "LINEDEFS");
	LoadLineDefs(lumpnum, ML_LINEDEFS);	/* Needs vertexes,sectors and sides */
drawLoadingBar(barCurrent++, barMax, "BLOCKMAP");
	LoadBlockMap(lumpnum, ML_BLOCKMAP);	/* Needs lines */
drawLoadingBar(barCurrent++, barMax, "SEGS");
	LoadSegs(lumpnum, ML_SEGS);		/* Needs vertexes,lines,sides */
drawLoadingBar(barCurrent++, barMax, "SSECTORS");
	LoadSubsectors(lumpnum, ML_SSECTORS);	/* Needs sectors and segs and sides */
drawLoadingBar(barCurrent++, barMax, "NODES");
	LoadNodes(lumpnum, ML_NODES);		/* Needs subsectors */
	KillALump(lumpnum, ML_VERTEXES);		/* Release the map vertexes */
drawLoadingBar(barCurrent++, barMax, "REJECT");
	RejectMatrix = (Byte *)LoadALump(lumpnum, ML_REJECT);	/* Get the reject matrix */
	GroupLines();			/* Final last minute data arranging */
	deathmatch_p = deathmatchstarts;
drawLoadingBar(barCurrent++, barMax, "THINGS");
	LoadThings(lumpnum, ML_THINGS);	/* Spawn all the items */
	SpawnSpecials();		/* Spawn all sector specials */
drawLoadingBar(barCurrent++, barMax, "PRELOAD WALLS");
	PreloadWalls();			/* Load all the wall textures and sprites */
drawLoadingBar(barCurrent++, barMax, "END");

/* if deathmatch, randomly spawn the active players */

	gamepaused = FALSE;		/* Game in progress */
}

/**********************************

	Dispose of all memory allocated by loading a level

**********************************/

void ReleaseMapMemory(void)
{
	Word i;
	
	DeallocAPointer(sectors);		/* Dispose of the sectors */
	DeallocAPointer(sides);			/* Dispose of the side defs */
	DeallocAPointer(lines);			/* Dispose of the lines */
	KillALump(LoadedLevel, ML_BLOCKMAP);	/* Make sure it's discarded since I modified it */
	DeallocAPointer(BlockLinkPtr);	/* Discard the block map mobj linked list */
	DeallocAPointer(segs);		/* Release the line segment memory */
	DeallocAPointer(subsectors);	/* Release the sub sectors */
	KillALump(LoadedLevel, ML_NODES);	/* Release the BSP tree */
	KillALump(LoadedLevel, ML_REJECT);	/* Release the quick reject matrix */	
	DeallocAPointer(LineArrayBuffer);
	sectors = 0;		/* Zap the pointers */
	sides = 0;			/* May cause a memory fault, but this will aid in debugging! */
	lines = 0;
	BlockMapLines = 0;		/* Force zero for resource */
	BlockLinkPtr = 0;
	segs = 0;
	subsectors = 0;
	FirstBSPNode = 0;
	RejectMatrix = 0;
	
	i = 0;					/* Start at the first wall texture */
	do {
		ReleaseAResource(i+FirstTexture);	/* Release all wall textures */
	} while (++i<NumTextures);			/* All released? */
	i = 0;					/* Start at the first flat texture */
	do {
		ReleaseAResource(i+FirstFlat);
	} while (++i<NumFlats);
	memset(FlatInfo,0,NumFlats*sizeof(void *));	/* Kill the cached flat table */
	InitThinkers();			/* Dispose of all remaining memory */
}

/**********************************

	Init the machine independant code

**********************************/

void P_Init(void)
{
	P_InitSwitchList();		/* Init the switch picture lookup list */
	P_InitPicAnims();		/* Init the picture animation scripts */
}
