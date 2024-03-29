#include "Doom.h"
#include <Portfolio.h>
#include <event.h>
#include <Init3do.h>
#include <FileFunctions.h>
#include "stdio.h"
#include <IntMath.h>
#include <BlockFile.h>
#include <Time.h>
#include <audio.h>
#include <math.h>

#include "limits.h"
#include "bench.h"

#include "engine_main.h"


static void LowMemCode(Word Type);
static void WipeDoom(LongWord *OldScreen,LongWord *NewScreen);

LongWord LastTics;				/* Time elapsed since last page flip */
Word WorkPage;					/* Which frame is not being displayed */

#define VDL_PAGE_FLIPS 3

uint32 MainTask;					/* My own task item */
static ulong ScreenPageCount;		/* Number of screens */
static Item ScreenItems[SCREENS];	/* Referances to the game screens */
static Item VideoItems[SCREENS];
static Item VDLItems[VDL_PAGE_FLIPS * SCREENS];
static long ScreenByteCount;		/* How many bytes for each screen */
static Item ScreenGroupItem = 0;	/* Main screen referance */
static Byte *ScreenMaps[SCREENS];	/* Pointer to the bitmap screens */
static Item VRAMIOReq;				/* I/O Request for screen copy */
static Item vsyncItem;
Item AllSamples[NUMSFX];			/* Items to sound samples */
Word AllRates[NUMSFX];

static int maxFlipScreens = SCREENS;
static int vdlPage = 0;

int frameTime;
int displayFreq;

int memLowHits = 0;

static unsigned char cGamma[32];
static bool needsVDLcolorsUpdate = false;

//#define DEBUG_OPT_HACK

// When we want to set few alternative rendering options at every start up, for repeating tests and not having to change the same options every time in game
static void optHack()
{
    #ifdef DEBUG_OPT_HACK
        //optGraphics->planeQuality = PLANE_QUALITY_LO;
        //optGraphics->depthShading = DEPTH_SHADING_BRIGHT;
        background_clear = true;
    #endif
}

/*******************

  Stupid debugging

*******************/

#ifdef DEBUG_OPT_HACK
	#define DBG_NUM_MAX 10
	static int dbgNum[DBG_NUM_MAX];
	static int dbgIndex = 0;
	static int dbgMin;
	static int dbgMax;
	static int dbgMinMaxOn = false;

	void printDbg(int value)
	{
		if (dbgIndex==DBG_NUM_MAX) return;

		dbgNum[dbgIndex] = value;
		++dbgIndex;
	}

	void printDbgMinMax(int value)
	{
		if (!dbgMinMaxOn) {
			dbgMin = INT_MAX;
			dbgMax = INT_MIN;
			dbgMinMaxOn = true;
		}

		if (value < dbgMin) dbgMin = value;
		if (value > dbgMax) dbgMax = value;
	}

	static void printSignedNum(int x, int y, int num)
	{
		int xp = x;
		if (num < 0) {
			num = -num;
			PrintBigFont(xp, y, (Byte*)"-");
			xp+=8;
		}
		PrintNumber(xp, y, num, 0);
	}

	static void renderDbg()
	{
		int i;
		int iy = 2 * 16;

		if (dbgMinMaxOn) {
			printSignedNum(8,iy,dbgMin);
			printSignedNum(216,iy,dbgMax);
			iy+=16;
		}

		for (i=0; i<dbgIndex; ++i) {
			printSignedNum(8,iy,dbgNum[i]);
			iy+=16;
		}

		FlushCCBs();

		dbgIndex = 0;
	}
#endif

/**********************************

	Run an external program and wait for compleation

**********************************/

/*static void RunAProgram(char *ProgramName)
{
	Item LogoItem;
	LogoItem=LoadProgram(ProgramName);		// Load and begin execution
	do {
		Yield();						// Yield all CPU time to the other program
	} while (LookupItem(LogoItem));		// Wait until the program quits
	DeleteItem(LogoItem);				// Dispose of the 3DO logo code
}*/

/**********************************

	Init the 3DO variables to a specific screen

**********************************/

void SetMyScreen(Word Page)
{
	VideoItem = VideoItems[Page];			/* Get the bitmap item # */
	VideoScreen = ScreenItems[Page];
	VideoPointer = (Byte *) &ScreenMaps[Page][0];
	CelLine190 = (Byte *) &VideoPointer[190*640];
}

Byte *getVideoPointer(Word Page)
{
	return (Byte *) &ScreenMaps[Page][0];
}

/**********************************

	Init the system tools

	Start up all the tools for the 3DO system
	Return TRUE if all systems are GO!

**********************************/

static Word HeightArray[1] = 200;		/* I want 200 lines for display memory */
static Word MyCustomVDL[] = {
	VDL_RELSEL|			/* Relative pointer to next VDL */
	(1<<VDL_LEN_SHIFT)|		/* (DMA) 1 control words in this VDL entry */
	(20<<VDL_LINE_SHIFT),	/* Scan lines to persist */
	0,				/* Current video buffer */
	0,				/* Previous video buffer */
	4*4,			/* Pointer to next vdl */
	0xE0000000,		/* Set the screen to BLACK */
	VDL_NOP,		/* Filler to align to 16 bytes */
	VDL_NOP,
	VDL_NOP,

	VDL_RELSEL|
	VDL_ENVIDDMA|				/* Enable video DMA */
	VDL_LDCUR|					/* Load current address */
	VDL_LDPREV|					/* Load previous address */
	((32+2)<<VDL_LEN_SHIFT)|			/* (DMA) 2 control words in this VDL entry */
	(198<<VDL_LINE_SHIFT),		/* Scan lines to persist */
	0,				/* Current video buffer */
	0,				/* Previous video buffer */
	(32+4)*4,			/* Pointer to next vdl */

	VDL_DISPCTRL|	/* Video display control word */
	VDL_CLUTBYPASSEN|	/* Allow fixed clut */
	VDL_WINBLSB_BLUE|	/* Normal blue */
#if 1
	VDL_WINVINTEN|		/* Enable HV interpolation */
	VDL_WINHINTEN|
	VDL_VINTEN|
	VDL_HINTEN|
#endif
	VDL_BLSB_BLUE|		/* Normal */
	VDL_HSUB_FRAME,		/* Normal */
	0x00000000,		/* Default CLUT */
	0x01080808,
	0x02101010,
	0x03181818,
	0x04202020,
	0x05292929,
	0x06313131,
	0x07393939,
	0x08414141,
	0x094A4A4A,
	0x0A525252,
	0x0B5A5A5A,
	0x0C626262,
	0x0D6A6A6A,
	0x0E737373,
	0x0F7B7B7B,
	0x10838383,
	0x118B8B8B,
	0x12949494,
	0x139C9C9C,
	0x14A4A4A4,
	0x15ACACAC,
	0x16B4B4B4,
	0x17BDBDBD,
	0x18C5C5C5,
	0x19CDCDCD,
	0x1AD5D5D5,
	0x1BDEDEDE,
	0x1CE6E6E6,
	0x1DEEEEEE,
	0x1EF6F6F6,
	0x1FFFFFFF,
	0xE0000000,		/* Set the screen to BLACK */
	VDL_NOP,				/* Filler to align to 16 bytes */
	VDL_NOP,

	(1<<VDL_LEN_SHIFT)|			/* (DMA) 1 control words in this VDL entry */
	(0<<VDL_LINE_SHIFT),		/* Scan lines to persist (Forever) */
	0,				/* Current video buffer */
	0,				/* Previous video buffer */
	0,				/* Pointer to next vdl (None) */
	0xE0000000,		/* Set the screen to BLACK */
	VDL_NOP,				/* Filler to align to 16 bytes */
	VDL_NOP,
	VDL_NOP
};

static TagArg ScreenTags[] =	{		/* Change this to change the screen count! */
	CSG_TAG_SPORTBITS, (void *)0,	/* Allow SPORT DMA (Must be FIRST) */
	CSG_TAG_SCREENCOUNT, (void *)SCREENS,	/* How many screens to make! */
	CSG_TAG_BITMAPCOUNT,(void *)1,
	CSG_TAG_BITMAPHEIGHT_ARRAY,(void *)&HeightArray[0],
	CSG_TAG_DONE, 0			/* End of list */
};

static TagArg SoundRateArgs[] = {
	AF_TAG_SAMPLE_RATE,(void*)0,	/* Get the sample rate */
	TAG_END,0		/* End of the list */
};
static char FileName[32];

static uint32 *vdlColors = (uint32*)&MyCustomVDL[13];


/*static void showLogos()
{
    Show3DOLogo();				// Show the 3DO Logo
    RunAProgram("IdLogo IDLogo.cel");
    #if 0			// Set to 1 for Japanese version
        RunAProgram("IdLogo LogicLogo.cel");
        RunAProgram("PlayMovie EALogo.cine");
        RunAProgram("IdLogo AdiLogo.cel");
    #else
        RunAProgram("PlayMovie Logic.cine");
        RunAProgram("PlayMovie AdiLogo.cine");
    #endif
}*/

static void loadSoundFx()
{
	Word i = 0;
	do {
        if (!loadPsxSamples) {
            sprintf(FileName,"Sounds/Sound%02d.aiff",i+1);
        } else {
            sprintf(FileName,"Sounds/psx/Sound%02d.aiff",i+1);
        }

		AllSamples[i] = LoadSample(FileName);
		if (AllSamples[i]<0) {
			AllSamples[i] = 0;
		}
		if (AllSamples[i]) {
			GetAudioItemInfo(AllSamples[i],SoundRateArgs);
			AllRates[i] = (Word)(((LongWord)SoundRateArgs[0].ta_Arg)/(44100UL*2UL));	/* Get the DSP rate for the sound */
		}
	} while (++i<(NUMSFX-1));
}

static void initScreenVDL()
{
	Word i;		/* Temp */
	long width, height;	/* Screen width & height */
	Screen *screen;	/* Pointer to screen info */
	ItemNode *Node;

	/* Read page PRF-85 for info */

	screen = (Screen *)LookupItem(ScreenItems[0]);
	if (!screen) {
		exit(10);
	}

	width = screen->scr_TempBitmap->bm_Width;		/* How big is the screen? */
	height = screen->scr_TempBitmap->bm_Height;

	ScreenPageCount = (width*2*height+GrafBase->gf_VRAMPageSize-1)/GrafBase->gf_VRAMPageSize;
	ScreenByteCount = ScreenPageCount * GrafBase->gf_VRAMPageSize;

	i=0;
	do {		/* Process the screens */
		screen = (Screen *)LookupItem(ScreenItems[i]);
		ScreenMaps[i] = (Byte *)screen->scr_TempBitmap->bm_Buffer;
		memset(ScreenMaps[i],0,ScreenByteCount);	/* Clear the screen */
		Node = (ItemNode *) screen->scr_TempBitmap;	/* Get the bitmap pointer */
		VideoItems[i] = (Item)Node->n_Item;			/* Get the bitmap item # */
        SetCEControl(VideoItems[i], 0xffffffff, ASCALL); // To enable Super Clipping
		MyCustomVDL[9]=MyCustomVDL[10] = (Word)ScreenMaps[i];
		VDLItems[i] = SubmitVDL((VDLEntry *)&MyCustomVDL[0],sizeof(MyCustomVDL)/4,VDLTYPE_FULL);
		SetVDL(ScreenItems[i],VDLItems[i]);

		SetClipWidth(VideoItems[i],320);
		SetClipHeight(VideoItems[i],200);		/* I only want 200 lines */
		SetClipOrigin(VideoItems[i],0,0);		/* Set the clip top for the screen */
	} while (++i<SCREENS);
}

static void setVDLcolors()
{
	int i;
	Item *currVDLitem, *prevVDLitem;
	
	int prevVDLpage = vdlPage - 1;
	if (prevVDLpage < 0) prevVDLpage = VDL_PAGE_FLIPS - 1;
	prevVDLitem = (Item*)&VDLItems[SCREENS * prevVDLpage];
	if (++vdlPage==VDL_PAGE_FLIPS) vdlPage = 0;
	currVDLitem = (Item*)&VDLItems[SCREENS*vdlPage];

	for (i=0; i<SCREENS; ++i) {
		Screen *screen = (Screen*)LookupItem(ScreenItems[i]);
		ScreenMaps[i] = (Byte*)screen->scr_TempBitmap->bm_Buffer;
		MyCustomVDL[9] = MyCustomVDL[10] = (Word)ScreenMaps[i];

		currVDLitem[i] = SubmitVDL((VDLEntry *)&MyCustomVDL[0], sizeof(MyCustomVDL)/4, VDLTYPE_FULL);
		SetVDL(ScreenItems[i], currVDLitem[i]);
		DeleteVDL(prevVDLitem[i]);
	}
}

void colorizeVDL(int r, int g, int b)
{
	int i;

	const int r0 = 255 - (r >> 1);
	const int g0 = 255 - (g >> 1);
	const int b0 = 255 - (b >> 1);

	const int r1 = r * 127;
	const int g1 = g * 127;
	const int b1 = b * 127;

	for (i=0; i<32; ++i) {
		const int c = cGamma[i];
		const int rf = (c * r0 + r1) >> 8;
		const int gf = (c * g0 + g1) >> 8;
		const int bf = (c * b0 + b1) >> 8;
		vdlColors[i] = (i << 24) | (rf << 16) | (gf << 8) | bf;
	}

	needsVDLcolorsUpdate = true;
}

void updateVDL(bool invert)
{
	int i;
	
	for (i=0; i<32; ++i) {
		int c = cGamma[i];
		if (invert) c = 255 - c;
		vdlColors[i] = (i << 24) | (c << 16) | (c << 8) | c;
	}

	needsVDLcolorsUpdate = true;
}

void updateGamma(int value, int max, bool shouldUpdateVDL)
{
	int i;
	const float v = 1.0f / (1.0f + ((float)value / (float)(max-1)));
	for (i=0; i<32; ++i) {
		cGamma[i] = (unsigned char)(255 * pow(((float)i / 31), v));
	}

	if (shouldUpdateVDL) {
		updateVDL(false);
	}
}

static void initSystem()
{
 	if (OpenGraphicsFolio() ||	/* Start up the graphics system */
		(OpenAudioFolio()<0) ||		/* Start up the audio system */
		(OpenMathFolio()<0) ) {
		exit(10);
	}

    #if 0	/* Set to 1 for the PAL version, 0 for the NTSC version */
	{
		int width;
        QueryGraphics(QUERYGRAF_TAG_DEFAULTDISPLAYTYPE,&width);
        if (width==DI_TYPE_NTSC) {
            exit(10);
        }
	}
    #endif

    #if 0	/* Remove for final build! */
        ChangeDirectory("/CD-ROM");
    #endif

	ScreenTags[0].ta_Arg = (void *)GETBANKBITS(GrafBase->gf_ZeroPage);
	ScreenGroupItem = CreateScreenGroup(ScreenItems,ScreenTags);

	if (ScreenGroupItem<0) {		/* Error creating screens? */
		exit(10);
	}
	AddScreenGroup(ScreenGroupItem,NULL);		/* Add my screens to the system */

	initScreenVDL();

	InitEventUtility(1,1,FALSE);	/* I want 1 joypad, 1 mouse, and passive listening */

	InitSoundPlayer("system/audio/dsp/varmono8.dsp",0); /* Init memory for the sound player */
	InitMusicPlayer("system/audio/dsp/dcsqxdstereo.dsp");	/* Init memory for the music player */
    //	InitMusicPlayer("system/audio/dsp/fixedstereosample.dsp");	/* Init memory for the music player */

	MainTask = KernelBase->kb_CurrentTask->t.n_Item;	/* My task Item */
	VRAMIOReq = GetVRAMIOReq();
	vsyncItem = GetVBLIOReq();
	SetMyScreen(0);				/* Init the video display */

	QueryGraphics(QUERYGRAF_TAG_FIELDFREQ, &displayFreq);
}


void Init()
{
    initSystem();

    startModMenu();

	// Have removed the logos at all since the version 0.3
	//if (!skipLogos) showLogos();


    loadSoundFx();  // For some reason, this cannot be loaded later or sound effects will be missing (issues with memory allocation?)


	MinHandles = 1200;		/* I will need lot's of memory handles */

	InitMemory();			/* Init the memory manager */
	InitResource();			/* Init the resource manager */

	InterceptKey();			/* Init events */
	SetErrBombFlag(TRUE);	/* Any OS errors will kill me */
	MemPurgeCallBack = LowMemCode;


	initTimer();

	setPrimaryMenuOptions();    // We had to do this here, because some of the initial option menus (floor quality) are needed for early rendering inits
	optHack();                  // These too, just for repeatitive debugging tests
	initAllCCBelements();
	initEngine();
}


/**********************************

	Read a file from NVRAM or a memory card into buf

**********************************/

static LongWord RamFileSize;

int32 StdReadFile(char *fName,char *buf)
{
	int32 err;			/* Error code to return */
	Item fd;			/* Disk file referance */
	Item req;			/* IO request item */
	IOReq *reqp;		/* Pointer to IO request item */
	IOInfo params;		/* Parameter list for I/O information */
	DeviceStatus ds;	/* Struct for device status */

	fd = OpenDiskFile(fName);	/* Open the file */
	if (fd < 0) {				/* Error? */
		return fd;
	}

	req = CreateIOReq(NULL,0,fd,0);			/* Create an I/O item */
	reqp = (IOReq *)LookupItem(req);		/* Deref the item pointer */
	memset(&params,0,sizeof(IOInfo));		/* Blank the I/O record */
	memset(&ds,0,sizeof(DeviceStatus));		/* Blank the device status */
	params.ioi_Command = CMD_STATUS;		/* Make a status command */
	params.ioi_Recv.iob_Buffer = &ds;		/* Set the I/O buffer ptr */
	params.ioi_Recv.iob_Len = sizeof(DeviceStatus);	/* Set the length */
	err = DoIO(req,&params);				/* Perform the status I/O */
	if (err>=0) {			/* Status ok? */
		/* Try to read it in */

		/* Calc the read size based on blocks */
		RamFileSize = ds.ds_DeviceBlockCount * ds.ds_DeviceBlockSize;
		memset(&params,0,sizeof(IOInfo));		/* Zap the I/O info record */
		params.ioi_Command = CMD_READ;			/* Read command */
		params.ioi_Recv.iob_Len = RamFileSize;	/* Data length */
		params.ioi_Recv.iob_Buffer = buf;		/* Data buffer */
		err = DoIO(req,&params);				/* Read the file */
	}
	DeleteIOReq(req);		/* Release the IO request */
	CloseDiskFile(fd);		/* Close the disk file */
	return err;				/* Return the error code (If any) */
}

/**********************************

	Write out the prefs to the NVRAM

**********************************/

#define PREFWORD 0x4C57
static char PrefsName[] = "/NVRAM/DoomPrefs";		/* Save game name */

void WritePrefsFile(void)
{
	Word PrefFile[10];		/* Must match what's in ReadPrefsFile!! */
	Word CheckSum;			/* Checksum total */
	Word i;

	PrefFile[0] = PREFWORD;
	PrefFile[1] = StartSkill;
	PrefFile[2] = StartMap;
	PrefFile[3] = SfxVolume;
	PrefFile[4] = MusicVolume;
	PrefFile[5] = ControlType;
	PrefFile[6] = MaxLevel;
	PrefFile[7] = ScreenSizeOption;
	PrefFile[8] = LowDetail;
	PrefFile[9] = 12345;		/* Init the checksum */
	i = 0;
	CheckSum = 0;
	do {
		CheckSum += PrefFile[i];		/* Make a simple checksum */
	} while (++i<10);
	PrefFile[9] = CheckSum;
	SaveAFile((Byte *)PrefsName,&PrefFile,sizeof(PrefFile));	/* Save the game file */
}

/**********************************

	Clear out the prefs file

**********************************/

void ClearPrefsFile(void)
{
	StartSkill = sk_medium;		/* Init the basic skill level */
	StartMap = 1;				/* Only allow playing from map #1 */
	SfxVolume = 15;				/* Init the sound effects volume */
	MusicVolume = 15;			/* Init the music volume */
	ControlType = 3;			/* Use basic joypad controls */
	MaxLevel = 1;				/* Only allow level 1 to select from */
	ScreenSizeOption = 2;		/* Default screen size option */
	LowDetail = FALSE;			/* Detail mode */
	WritePrefsFile();			/* Output the new prefs */

	setScreenSizeSliderFromOption();
}

/**********************************

	Load in the standard prefs

**********************************/

void ReadPrefsFile(void)
{
	Word PrefFile[88];		/* Must match what's in WritePrefsFile!! */
	Word CheckSum;			/* Running checksum */
	Word i;

	if (StdReadFile(PrefsName,(char *) PrefFile)<0) {	/* Error reading? */
		ClearPrefsFile();		/* Clear it out */
		return;
	}

	i = 0;
	CheckSum = 12345;		/* Init the checksum */
	do {
		CheckSum+=PrefFile[i];	/* Calculate the checksum */
	} while (++i<9);

	if ((CheckSum != PrefFile[10-1]) || (PrefFile[0] !=PREFWORD)) {
		ClearPrefsFile();	/* Bad ID or checksum! */
		return;
	}
	StartSkill = (skill_t)PrefFile[1];
	StartMap = PrefFile[2];
	SfxVolume = PrefFile[3];
	MusicVolume = PrefFile[4];
	ControlType = PrefFile[5];
	MaxLevel = PrefFile[6];
	ScreenSizeOption = PrefFile[7];
	LowDetail = PrefFile[8];

	setScreenSizeSliderFromOption();

	if ((StartSkill >= (sk_nightmare+1)) ||
		(StartMap >= 27) ||
		(SfxVolume >= 16) ||
		(MusicVolume >= 16) ||
		(ControlType >= 6) ||
		(MaxLevel >= 26) ||
		(ScreenSizeOption >= SCREENSIZE_OPTIONS_NUM) ||
		(LowDetail>=2) ) {
		ClearPrefsFile();
	}
}

/**********************************

	Display the current framebuffer
	If < 1/15th second has passed since the last display, busy wait.
	15 fps is the maximum frame rate, and any faster displays will
	only look ragged.

**********************************/

static void updateWipeScreen()
{
	if (DoWipe) {
		Word PrevPage;
		void *NewImage;
		void *OldImage;

		DoWipe = FALSE;
		NewImage = VideoPointer;	/* Pointer to the NEW image */
		PrevPage = WorkPage-1;	/* Get the currently displayed page */
		if (PrevPage==-1) {		/* Wrapped? */
			PrevPage = maxFlipScreens-1;
		}
		SetMyScreen(PrevPage);		/* Set videopointer to display buffer */
		if (!PrevPage) {
			PrevPage=maxFlipScreens;
		}
		--PrevPage;
		OldImage = (Byte *) &ScreenMaps[PrevPage][0];	/* Get work buffer */

			/* Copy the buffer from display to work */
		memcpy(OldImage,VideoPointer,320*200*2);
		WipeDoom((LongWord *)OldImage,(LongWord *)NewImage);			/* Perform the wipe */
	}
}

static void updateMyFpsAndDebugPrint()
{
    if (optOther->stats >= STATS_FPS) {
		printFPS(8, 8);
		++nframe;
    }
    if (optOther->stats >= STATS_MEM) {
		if (debugMode) PrintNumber(0, 128, memLowHits, 0);
		PrintNumber(0, 144, GetTotalFreeMem(), 0);
    }
    if (optOther->stats >= STATS_ALL) {
		PrintNumber(0, 96, visplanesCount-1, 0);
		PrintNumber(0, 112, visplanesCountMax-1, 0);
    }

	FlushCCBs();

#ifdef DEBUG_OPT_HACK
    renderDbg();
#endif
}

static void VBLhackHalf(int ticDiff)
{
	int vblTicUnder = 16;
	if (displayFreq == 50) vblTicUnder = 20;

	if (ticDiff < vblTicUnder) {
		WaitVBL(vsyncItem, 2);
	} else {
		WaitVBL(vsyncItem, 1);
	}
}

LongWord getVBLtic()
{
	static LongWord vblTic;

	QueryGraphics(QUERYGRAF_TAG_FIELDCOUNT, &vblTic);
	return vblTic;
}

static void frameWait()
{
	static LongWord LastTicCount = 0;	// Time mark for page flipping
	static LongWord NewTick;
	static int lastMyTick = 0;

	if (optGraphics->frameLimit >= FRAME_LIMIT_1VBL && optGraphics->frameLimit <= FRAME_LIMIT_4VBL) {
		const int count = optGraphics->frameLimit;
		do {
			NewTick = getVBLtic();	/* Get the time mark */
			LastTics = NewTick - LastTicCount;	/* Get the time elapsed */
		} while (LastTics < count);		/* Hmmm, too fast?!?!? */
	} else {
		if (optGraphics->frameLimit == FRAME_LIMIT_VSYNC) {
			VBLhackHalf(getTicks() - lastMyTick);
		}
		NewTick = getVBLtic();
		LastTics = NewTick - LastTicCount;
	}
	lastMyTick = getTicks();
	if (LastTics < 1) LastTics = 1;
	LastTicCount = NewTick;
}

void copyMainScreenToRest()
{
	int i;
	Byte *mainScreen = ScreenMaps[WorkPage];

	for (i=0; i<VDL_PAGE_FLIPS; ++i) {
		if (i!=WorkPage) {
			memcpy(ScreenMaps[i], mainScreen, 320*198*2);
		}
	}
}

void updateScreenAndWait()
{
	DisplayScreen(ScreenItems[WorkPage],0);		// Display the hidden page
	if (++WorkPage>=maxFlipScreens) {		// Next screen in line
		WorkPage = 0;
	}
	SetMyScreen(WorkPage);		// Set the 3DO vars

	offscreenPage = WorkPage + 1;
	if (offscreenPage>=maxFlipScreens) {
		offscreenPage = 0;
	}

	frameWait();

	frameTime = getTicks();
}

void UpdateAndPageFlip(void)
{
	FlushCCBs();

	updateWipeScreen();

#ifdef PROFILE_ON
	updateBench();
#endif
	updateMyFpsAndDebugPrint();

    updateScreenAndWait();

	if (needsVDLcolorsUpdate) {
		setVDLcolors();
		needsVDLcolorsUpdate = false;
	}
}


/**********************************

	Draw a shape centered on the screen
	Used for "Loading or Paused" pics

**********************************/

void DrawPlaque(Word RezNum)
{
	Word PrevPage;
	void *PicPtr;

	PrevPage = WorkPage-1;
	if (PrevPage==-1) {
		PrevPage = maxFlipScreens-1;
	}
	FlushCCBs();		/* Flush pending draws */
	SetMyScreen(PrevPage);		/* Draw to the active screen */
	PicPtr = LoadAResource(RezNum);
	DrawShape(160-(GetShapeWidth(PicPtr)/2),80,PicPtr);
	FlushCCBs();		/* Make sure it's drawn */
	ReleaseAResource(RezNum);
	SetMyScreen(WorkPage);		/* Reset to normal */
}

static void drawLoadingBarPlaque(int pos, int max)
{
	const int width = 160;
	const int height = 16;
	const int yPos = 156;
	const int wBlock = width / (max + 1);
	int i;

	// Also draw plaque
	DrawPlaque(rLOADING);

	DrawARect(160 - width/2 - 1, yPos - height/2, width, height, MakeRGB15(7,7,7));

	for (i=0; i<=pos; ++i) {
		DrawARect(160 - width/2 + 1 + i*wBlock, yPos - height/2 + 1, wBlock-1, height-2, MakeRGB15(24,28,31));
	}
	
	FlushCCBs();

	updateScreenAndWait();
}

void drawDebugValue(int value)
{
	int i;
	static int yp = 0;

	for (i=0; i<60; ++i) {
		PrintNumber(0, yp, value, 0);
		FlushCCBs();
		updateScreenAndWait();
	}
	yp+=16;
	if (yp==240) yp = 0;
}

void drawLoadingBar(int pos, int max, const char *text)
{
	int i;

	for (i=0; i<30; ++i) {
		if (debugMode) {
			DrawARect(64,160, 232,208, MakeRGB15(3,7,7));
			PrintBigFont(80, 168, (Byte*)text);
			PrintNumber(80, 184, GetTotalFreeMem(), 0);
		}
		drawLoadingBarPlaque(pos, max);
	}
}



/**********************************

	Perform a "Doom" like screen wipe
	I require that VideoPointer is set to the current screen

**********************************/

#define WIPEWIDTH 320		/* Width of the 3DO screen to wipe */
#define WIPEHEIGHT 200

static void WipeDoom(LongWord *OldScreen,LongWord *NewScreen)
{
	LongWord Mark;	/* Last time mark */
	Word TimeCount;	/* Elapsed time since last mark */
	Word i,x;
	Word Quit;		/* Finish flag */
	int delta;		/* YDelta (Must be INT!) */
	LongWord *Screenad;		/* I use short pointers so I can */
	LongWord *SourcePtr;		/* move in pixel pairs... */
	int YDeltaTable[WIPEWIDTH/2];	/* Array of deltas for the jagged look */

    /* First thing I do is to create a ydelta table to */
    /* allow the jagged look to the screen wipe */

	delta = -GetRandom(15);	/* Get the initial position */
	YDeltaTable[0] = delta;	/* Save it */
	x = 1;
	do {
		delta += (GetRandom(2)-1);	/* Add -1,0 or 1 */
		if (delta>0) {		/* Too high? */
			delta = 0;
		}
		if (delta == -16) {	/* Too low? */
			delta = -15;
		}
		YDeltaTable[x] = delta;	/* Save the delta in table */
	} while (++x<(WIPEWIDTH/2));	/* Quit? */

    /* Now perform the wipe using ReadTick to generate a time base */
    /* Do NOT go faster than 30 frames per second */

	Mark = ReadTick()-2;	/* Get the initial time mark */
	do {
		do {
			TimeCount = ReadTick()-Mark;	/* Get the time mark */
		} while (TimeCount<(TICKSPERSEC/30));			/* Enough time yet? */
		Mark+=TimeCount;		/* Adjust the base mark */
		TimeCount/=(TICKSPERSEC/30);	/* Math is for 30 frames per second */

    /* Adjust the YDeltaTable "TimeCount" times to adjust for slow machines */

		Quit = TRUE;		/* Assume I already am finished */
		do {
			x = 0;		/* Start at the left */
			do {
				delta = YDeltaTable[x];		/* Get the delta */
				if (delta<WIPEHEIGHT) {	/* Line finished? */
					Quit = FALSE;		/* I changed one! */
					if (delta < 0) {
						++delta;		/* Slight delay */
					} else if (delta < 16) {
						delta = delta<<1;	/* Double it */
						++delta;
					} else {
						delta+=8;		/* Constant speed */
 						if (delta>WIPEHEIGHT) {
							delta=WIPEHEIGHT;
						}
					}
					YDeltaTable[x] = delta;	/* Save new delta */
				}
			} while (++x<(WIPEWIDTH/2));
		} while (--TimeCount);		/* All tics accounted for? */

    /* Draw a frame of the wipe */

		x = 0;			/* Init the x coord */
		do {
			Screenad = (LongWord *)&VideoPointer[x*8];	/* Dest pointer */
			i = YDeltaTable[x];		/* Get offset */
			if ((int)i<0) {	/* Less than zero? */
				i = 0;		/* Make it zero */
			}
			i>>=1;		/* Force even for 3DO weirdness */
			if (i) {
				TimeCount = i;
				SourcePtr = &NewScreen[x*2];	/* Fetch from source */
				do {
					Screenad[0] = SourcePtr[0];	/* Copy 2 longwords */
					Screenad[1] = SourcePtr[1];
					Screenad+=WIPEWIDTH;
					SourcePtr+=WIPEWIDTH;
				} while (--TimeCount);
			}
			if (i<(WIPEHEIGHT/2)) {		/* Any of the old image to draw? */
				i = (WIPEHEIGHT/2)-i;
				SourcePtr = &OldScreen[x*2];
				do {
					Screenad[0] = SourcePtr[0];	/* Copy 2 longwords */
					Screenad[1] = SourcePtr[1];
					Screenad+=WIPEWIDTH;
					SourcePtr+=WIPEWIDTH;
				} while (--i);
			}
		} while (++x<(WIPEWIDTH/2));
	} while (!Quit);		/* Wipe finished? */
}

/**********************************

	Called when memory is REALLY low!
	This is an OOMQueue callback

**********************************/

static void LowMemCode(Word Stage)
{
	FlushCCBs();		/* Purge all CCB's */
	++memLowHits;
}

/**********************************

	Set the hardware clip rect to the actual game screen

**********************************/

void EnableHardwareClipping(void)
{
	FlushCCBs();		/* Failsafe */
	SetClipWidth(VideoItem,ScreenWidth);
	SetClipHeight(VideoItem,ScreenHeight);		/* I only want 200 lines */
	SetClipOrigin(VideoItem,ScreenXOffset,ScreenYOffset);		/* Set the clip top for the screen */
}

/**********************************

	Restore the clip rect to normal

**********************************/

void DisableHardwareClipping(void)
{
	FlushCCBs();		/* Failsafe */
	SetClipOrigin(VideoItem,0,0);		/* Set the clip top for the screen */
	SetClipWidth(VideoItem,320);
	SetClipHeight(VideoItem,200);		/* I only want 200 lines */
}

void DisableHardwareClippingWithoutFlush()
{
	SetClipOrigin(VideoItem,0,0);		/* Set the clip top for the screen */
	SetClipWidth(VideoItem,320);
	SetClipHeight(VideoItem,200);		/* I only want 200 lines */
}

#if 0
/**********************************

	This will allow screen shots to be taken.
	REMOVE FOR FINAL BUILD!!!

**********************************/

Word LastJoyButtons[4];		/* Save the previous joypad bits */
static Word FileNum;
static Short OneLine[640];

Word ReadJoyButtons(Word PadNum)
{
	char FileName[20];
	ControlPadEventData ControlRec;
	Short *OldImage;
	Short *DestImage;

	GetControlPad(PadNum+1,FALSE,&ControlRec);		/* Read joypad */
	if (PadNum<4) {
		if (((ControlRec.cped_ButtonBits^LastJoyButtons[PadNum]) &
			ControlRec.cped_ButtonBits)&PadC) {
			Word i,j,PrevPage;

			sprintf(FileName,"Art%d.RAW16",FileNum);
			++FileNum;
			PrevPage = WorkPage-1;	/* Get the currently displayed page */
			if (PrevPage==-1) {		/* Wrapped? */
				PrevPage = maxFlipScreens-1;
			}
			OldImage = (Short *) &ScreenMaps[PrevPage][0];	/* Get work buffer */
			i = 0;
			DestImage = OldImage;
			do {
				memcpy(OneLine,DestImage,320*2*2);
				j = 0;
				do {
					DestImage[j] = OneLine[j*2];
					DestImage[j+320] = OneLine[(j*2)+1];
				} while (++j<320);
				DestImage+=640;
			} while (++i<100);
			WriteMacFile(FileName,OldImage,320*200*2);
		}
		LastJoyButtons[PadNum] = (Word)ControlRec.cped_ButtonBits;
	}
	return (Word)ControlRec.cped_ButtonBits;		/* Return the data */
}
#endif

/**********************************

	Main code entry

**********************************/

int main(void)
{
	Init();

	UpdateAndPageFlip();	/* Init the video display's vars */
	ReadPrefsFile();		/* Load defaults */
	D_DoomMain();		/* Start doom */
	return 0;
}
