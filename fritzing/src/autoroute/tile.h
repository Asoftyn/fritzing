/*
 * tile.h --
 *
 * Definitions of the basic tile structures
 * The definitions in this file are all that is visible to
 * the Ti (tile) modules.
 *
 *     ********************************************************************* 
 *     * Copyright (C) 1985, 1990 Regents of the University of California. * 
 *     * Permission to use, copy, modify, and distribute this              * 
 *     * software and its documentation for any purpose and without        * 
 *     * fee is hereby granted, provided that the above copyright          * 
 *     * notice appear in all copies.  The University of California        * 
 *     * makes no representations about the suitability of this            * 
 *     * software for any purpose.  It is provided "as is" without         * 
 *     * express or implied warranty.  Export of this software outside     * 
 *     * of the United States of America may require an export license.    * 
 *     *********************************************************************
 *
 * rcsid "$Header: /usr/cvsroot/magic-7.5/tiles/tile.h,v 1.7 2010/05/03 15:16:57 tim Exp $"
 */

#ifndef _TILES_H
#define	_TILES_H

#include <QGraphicsItem>

// note: Tile uses math axes as opposed to computer graphic axes.  In other words y increases up.
struct TileRect {
	int xmini;
	int ymini;
	int xmaxi;
	int ymaxi;
};

struct TilePoint {
	int xi;
	int yi;
};


/*
 * A tile is the basic unit used for representing both space and
 * solid area in a plane.  It has the following structure:
 *
 *				       RT
 *					^
 *					|
 *		-------------------------
 *		|			| ---> TR
 *		|			|
 *		|			|
 *		| (lower left)		|
 *	BL <--- -------------------------
 *		|
 *		v
 *	        LB
 *
 * The (x, y) coordinates of the lower left corner of the tile are stored,
 * along with four "corner stitches": RT, TR, BL, LB.
 *
 * Space tiles are distinguished at a higher level by having a distinguished
 * tile body.
 */

typedef void * UserData;


struct Tile
{
	int		 ti_type;		/* another free field */
    QGraphicsItem *	 ti_body;	/* Body of tile */
    struct Tile	*ti_lb;		/* Left bottom corner stitch */
    struct Tile	*ti_bl;		/* Bottom left corner stitch */
    struct Tile	*ti_tr;		/* Top right corner stitch */
    struct Tile	*ti_rt;		/* Right top corner stitch */
    TilePoint	 ti_ll;		/* Lower left coordinate */
    QGraphicsItem *	 ti_client;	/* This space for hire.  */
	int ti_wave;
};

    /*
     * The following macros make it appear as though both
     * the lower left and upper right coordinates of a tile
     * are stored inside it.
     */

inline	Tile* LB(Tile* tileP) { return tileP->ti_lb; }
inline	Tile* BL(Tile* tileP) { return tileP->ti_bl; }
inline	Tile* TR(Tile* tileP) { return tileP->ti_tr; }
inline	Tile* RT(Tile* tileP) { return tileP->ti_rt; }
inline void SETLB(Tile* tileP, Tile * val) { tileP->ti_lb = val; 
	if (val == tileP) {
		int a = 0;
		a = a + 1;
	return;
}}
inline void SETBL(Tile* tileP, Tile * val) { tileP->ti_bl = val; 
	if (val == tileP) {
		int a = 0;
		a = a + 1;
	return;
}}
inline void SETTR(Tile* tileP, Tile * val) { tileP->ti_tr = val; 
	if (val == tileP) {
		int a = 0;
		a = a + 1;
	return;
}}
inline void SETRT(Tile* tileP, Tile * val) { tileP->ti_rt = val; 
	if (val == tileP) {
		int a = 0;
		a = a + 1;
	return;
}}

inline int YMIN(Tile* tileP) { return tileP->ti_ll.yi; }
inline int LEFT(Tile* tileP) { return	tileP->ti_ll.xi; }
inline int YMAX(Tile* tileP) { return	YMIN(RT(tileP)); }
inline int RIGHT(Tile* tileP)	{ return LEFT(TR(tileP)); }
inline void SETYMIN(Tile* tileP, int val) { tileP->ti_ll.yi = val; }
inline void SETLEFT(Tile* tileP, int val) { tileP->ti_ll.xi = val; }
inline void SETYMAX(Tile* tileP, int val) { SETYMIN(RT(tileP), val); }
inline void SETRIGHT(Tile* tileP, int val) { SETLEFT(TR(tileP), val); }

typedef void (*TileFunc)(Tile *);

void TiSetChangedFunc(TileFunc);
void TiSetFreeFunc(TileFunc);

/* ----------------------- Tile planes -------------------------------- */

/*
 * A plane of tiles consists of the four special tiles needed to
 * surround all internal tiles on all sides.  Logically, these
 * tiles appear as below, except for the fact that all are located
 * off at infinity.
 *
 *	 --------------------------------------
 *	 |\				     /|
 *	 | \				    / |
 *	 |  \		   TOP  	   /  |
 *	 |   \				  /   |
 *	 |    \				 /    |
 *	 |     --------------------------     |
 *	 |     |			|     |
 *	 |LEFT |			|RIGHT|
 *	 |     |			|     |
 *	 |     --------------------------     |
 *	 |    /				 \    |
 *	 |   /				  \   |
 *	 |  /		 BOTTOM 	   \  |
 *	 | /				    \ |
 *	 |/				     \|
 *	 --------------------------------------
 */

struct Plane
{
    Tile	*pl_left;	/* Left pseudo-tile */
    Tile	*pl_top;	/* Top pseudo-tile */
    Tile	*pl_right;	/* Right pseudo-tile */
    Tile	*pl_bottom;	/* Bottom pseudo-tile */
    Tile	*pl_hint;	/* Pointer to a "hint" at which to
				 * begin searching.
				 */
};

/*
 * The following coordinate, INFINITY, is used to represent a
 * tile location outside of the tile plane.
 *
 * It must be possible to represent INFINITY+1 as well as
 * INFINITY.
 *
 * Also, because locations involving INFINITY may be transformed,
 * it is desirable that additions and subtractions of small integers
 * from either INFINITY or MINFINITY not cause overflow.
 *
 * Consequently, we define INFINITY to be the largest integer
 * representable in wordsize - 2 bits.
 */

extern int INFINITY;
extern int MINFINITY;
extern int MINDIFF;

typedef int (*TileCallback)(Tile *, UserData);

/* ------------------------ Flags, etc -------------------------------- */

#define	BADTILE		((Tile *) 0)	/* Invalid tile pointer */

/* ============== Function headers and external interface ============= */

/*
 * The following macros and procedures should be all that are
 * ever needed by modules other than the tile module.
 */

Plane *TiNewPlane(Tile *tile);
void TiFreePlane(Plane *plane);
void TiToRect(Tile *tile, TileRect *rect);
Tile *TiSplitX(Tile *tile, int x);
Tile *TiSplitY(Tile *tile, int y);
Tile *TiSplitX_Left(Tile *tile, int x);
Tile *TiSplitY_Bottom(Tile *tile, int y);
void  TiJoinX(Tile *tile1, Tile *tile2, Plane *plane);
void  TiJoinY(Tile *tile1, Tile *tile2, Plane *plane);
int   TiSrArea(Tile *hintTile, Plane *plane, TileRect *rect, TileCallback, UserData arg);
Tile *TiSrPoint( Tile * hintTile, Plane * plane, int x, int y);
Tile* TiInsertTile(Plane *, TileRect * rect, QGraphicsItem * body, int type);

#define	TiBottom(tileP)	(YMIN(tileP))
#define	TiLeft(tileP)		(LEFT(tileP))
#define	TiTop(tileP)		(YMAX(tileP))
#define	TiRight(tileP)		(RIGHT(tileP))

#define	TiGetType(tileP)		((tileP)->ti_type)
#define TiSetType(tileP, t)	((tileP)->ti_type = (int) (t))

#define	TiGetBody(tileP)		((tileP)->ti_body)
/* See diagnostic subroutine version in tile.c */
#define	TiSetBody(tileP, b)	((tileP)->ti_body = (b))
#define	TiGetClient(tileP)		((tileP)->ti_client)
#define	TiSetClient(tileP,b)	((tileP)->ti_client = (b))
#define	TiGetWave(tileP)		((tileP)->ti_wave)
#define	TiSetWave(tileP,b)		((tileP)->ti_wave = (b))


// make sure nobody else uses these values for tile type
enum {
	DUMMYLEFT = 999999,
	DUMMYTOP,
	DUMMYRIGHT,
	DUMMYBOTTOM
};

Tile *TiAlloc();
void TiFree(Tile *);

#define EnclosePoint(tile,point)	((LEFT(tile)   <= (point)->p_x ) && \
					 ((point)->p_x   <  RIGHT(tile)) && \
					 (YMIN(tile) <= (point)->p_y ) && \
					 ((point)->p_y   <  YMAX(tile)  ))

#define EnclosePoint4Sides(tile,point)	((LEFT(tile)   <= (point)->p_x ) && \
					 ((point)->p_x  <=  RIGHT(tile)) && \
					 (YMIN(tile) <= (point)->p_y ) && \
					 ((point)->p_y  <=  YMAX(tile)  ))

/* The four macros below are for finding next tile RIGHT, UP, LEFT or DOWN 
 * from current tile at a given coordinate value.
 *
 * For example, NEXT_TILE_RIGHT points tResult to tile to right of t 
 * at y-coordinate y.
 */

#define NEXT_TILE_RIGHT(tResult, t, y) \
    for ((tResult) = TR(t); YMIN(tResult) > (y); (tResult) = LB(tResult)) \
        /* Nothing */;

#define NEXT_TILE_UP(tResult, t, x) \
    for ((tResult) = RT(t); LEFT(tResult) > (x); (tResult) = BL(tResult)) \
        /* Nothing */;

#define NEXT_TILE_LEFT(tResult, t, y) \
    for ((tResult) = BL(t); YMAX(tResult) <= (y); (tResult) = RT(tResult)) \
        /* Nothing */;
 
#define NEXT_TILE_DOWN(tResult, t, x) \
    for ((tResult) = LB(t); RIGHT(tResult) <= (x); (tResult) = TR(tResult)) \
        /* Nothing */;

#define	TiSrPointNoHint(plane, point)	(TiSrPoint((Tile *) NULL, plane, point))

Tile * gotoPoint(Tile * tile, TilePoint p);

#endif /* _TILES_H */
