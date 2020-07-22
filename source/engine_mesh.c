#include "engine_mesh.h"
#include "engine_texture.h"
#include "3dlib.h"

#include "Doom.h"

void updateMeshCELs(Mesh *ms)
{
	int i;
	for (i=0; i<ms->quadsNum; i++) {
		Texture *tex = &ms->tex[ms->quad[i].textureId];

		if (tex->type & TEXTURE_TYPE_DYNAMIC) {
			CCB *cel = ms->quad[i].cel;
			int woffset;
			int vcnt;

			// In the future, also take account of offscreen buffer position too
			if (tex->type & TEXTURE_TYPE_FEEDBACK) {
				cel->ccb_PRE1 |= PRE1_LRFORM;
				cel->ccb_SourcePtr = (CelData*)getVideoPointer(tex->bufferIndex);
				woffset = SCREEN_WIDTH - 2;
				vcnt = (tex->height / 2) - 1;
			} else {
				cel->ccb_PRE1 &= ~PRE1_LRFORM;
				cel->ccb_SourcePtr = (CelData*)tex->bitmap;
				woffset = tex->width / 2 - 2;
				vcnt = tex->height - 1;
			}

			// Should spare the magic numbers at some point
			cel->ccb_PRE0 = (cel->ccb_PRE0 & ~(((1<<10) - 1)<<6)) | (vcnt << 6);
			cel->ccb_PRE1 = (cel->ccb_PRE1 & (65536 - 1024)) | (woffset << 16) | tex->width;

			cel->ccb_PLUTPtr = (uint16*)tex->pal[ms->quad[i].palId];
		}
	}
}

void prepareCelList(Mesh *ms)
{
	int i;
	for (i=0; i<ms->quadsNum; i++)
	{
		Texture *tex = &ms->tex[ms->quad[i].textureId];

		int32 celType = CREATECEL_UNCODED;
		if (tex->type & TEXTURE_TYPE_PALLETIZED)
			celType = CREATECEL_CODED;

		ms->quad[i].cel = CreateCel(tex->width, tex->height, tex->bpp, celType, tex->bitmap);
		ms->quad[i].cel->ccb_SourcePtr = (CelData*)tex->bitmap;	// I used to have issues, fixed it on sprite_engine. In the future I'll simple replace CreateCel
		ms->quad[i].cel->ccb_PLUTPtr = (uint16*)&tex->pal[ms->quad[i].palId];

		ms->quad[i].cel->ccb_Flags &= ~CCB_ACW;	// Initially, ACW is off and only ACCW (counterclockwise) polygons are visible

		ms->quad[i].cel->ccb_Flags |= (CCB_ACSC | CCB_ALSC | CCB_BGND);

		if (i!=0) LinkCel(ms->quad[i-1].cel, ms->quad[i].cel);
	}
	ms->quad[ms->quadsNum-1].cel->ccb_Flags |= CCB_LAST;
}

static void setMeshCELflags(Mesh *ms, uint32 flags, bool enable)
{
	int i;
	for (i=0; i<ms->quadsNum; i++) {
		if (enable) {
			ms->quad[i].cel->ccb_Flags |= flags;
		} else {
			ms->quad[i].cel->ccb_Flags &= ~flags;
		}
	}
}

void setMeshPosition(Mesh *ms, int px, int py, int pz)
{
	ms->posX = px;
	ms->posY = py;
	ms->posZ = pz;
}

void setMeshRotation(Mesh *ms, int rx, int ry, int rz)
{
	ms->rotX = rx;
	ms->rotY = ry;
	ms->rotZ = rz;
}

void setMeshPolygonOrder(Mesh *ms, bool cw, bool ccw)
{
	if (cw) {
		setMeshCELflags(ms, CCB_ACW, true);
	} else {
		setMeshCELflags(ms, CCB_ACW, false);
	}

	if (ccw) {
		setMeshCELflags(ms, CCB_ACCW, true);
	} else {
		setMeshCELflags(ms, CCB_ACCW, false);
	}
}

void setMeshTranslucency(Mesh *ms, bool enable)
{
	int i;
	for (i=0; i<ms->quadsNum; i++) {
		if (enable) {
			ms->quad[i].cel->ccb_PIXC = TRANSLUCENT_CEL;
		} else {
			ms->quad[i].cel->ccb_PIXC = SOLID_CEL;
		}
	}
}

Mesh* initMesh(int vrtxNum, int quadsNum)
{
	Mesh *ms = (Mesh*)AllocMem(sizeof(Mesh), MEMTYPE_ANY);

	ms->vrtxNum = vrtxNum;
	ms->quadsNum = quadsNum;

	ms->indexNum = ms->quadsNum << 2;
	ms->vrtx = (Vertex*)AllocMem(ms->vrtxNum * sizeof(Vertex), MEMTYPE_ANY);
	ms->index = (int*)AllocMem(ms->indexNum * sizeof(int), MEMTYPE_ANY);
	ms->quad = (QuadData*)AllocMem(ms->quadsNum * sizeof(QuadData), MEMTYPE_ANY);

	return ms;
}
