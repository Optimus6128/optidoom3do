#include "engine_texture.h"

#include "Doom.h"
#include "string.h"


Texture* initTexture(int width, int height, int bpp, int type, ubyte *bmp, uint16 *pal, ubyte numPals)
{
	Texture *tex = (Texture*)AllocAPointer(sizeof(Texture));

	// Can't have palettized texture if bpp over 8
	if (bpp > 8 || numPals == 0) {
		type &= ~TEXTURE_TYPE_PALLETIZED;
	}

	tex->width = width;
	tex->height = height;
	tex->bpp = bpp;
	tex->type = type;

	if (type & TEXTURE_TYPE_PALLETIZED) {
		if (!pal) {
			int palBits = bpp;
			if (palBits > 5) palBits = 5;	// Can't have more than 5bits (32 colors palette), even in 6bpp or 8bpp modes
			tex->pal = (uint16*)AllocAPointer(sizeof(uint16) * (1 << palBits) * numPals);
		} else {
			tex->pal = pal;
		}
	}

	if (!bmp) {
		const int size = (width * height * bpp) / 8;
		tex->bitmap = (ubyte*)AllocAPointer(size);
	} else {
		tex->bitmap = bmp;
	}

	return tex;
}

Texture *initFeedbackTexture(int posX, int posY, int width, int height, int bufferIndex)
{
	Texture *tex = (Texture*)AllocAPointer(sizeof(Texture));

	tex->width = width;
	tex->height = height;
	tex->bpp = 16;
	tex->type = (TEXTURE_TYPE_DYNAMIC | TEXTURE_TYPE_FEEDBACK);


	tex->bitmap = (ubyte*)getVideoPointer(bufferIndex);
	tex->bufferIndex = bufferIndex;
	tex->posX = posX;
	tex->posY = posY;

	return tex;
}

Texture *loadTexture(char *path)
{
	Texture *tex;
	CCB *tempCel;
	int size;

	(Texture*)AllocAPointer(sizeof(Texture));

	tempCel = LoadCel(path, MEMTYPE_ANY);

	tex = initTexture(tempCel->ccb_Width, tempCel->ccb_Height, 16, TEXTURE_TYPE_STATIC, NULL, NULL, 0);
		// 16bit is the only bpp CEL type extracted from BMPTo3DOCel for now (which is what I currently use to make CEL files and testing)
		// In the future, I'll try to deduce this from the CEL bits anyway (I already know how, just too lazy to find out again)
		// Update: BMPTo3DOCel is shit! It saves right now the same format as BMPTo3DOImage (for VRAM structure to use with SPORT copy) instead of the most common linear CEL bitmap structure

	size = (tex->width * tex->height * tex->bpp) / 8;

	memcpy(tex->bitmap, tempCel->ccb_SourcePtr, size);

	UnloadCel(tempCel);

	return tex;
}
