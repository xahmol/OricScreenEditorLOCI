// undo.c - Canvas-edit undo/redo for OricScreenEditorLOCI (Oric Atmos,
// Oscar64)
//
// Faithful port of vdcscreeneditor-v2's undo_new()/undo_performundo()/
// undo_performredo() (local reference at
// /home/xahmol/VDCScreenEditor2/src/main.c, lines 739-905, struct
// UndoStruct in include/defines.h) -- a ring buffer of dirty-rectangle
// snapshots (only the changed bounding box, not the full canvas) stored
// in bank-switched RAM. Adapted here for OSE's single-plane canvas
// (screenmap[] has one byte per cell, unlike VDC RAM's separate screen/
// attribute planes, so each snapshot needs width*height bytes, not
// width*height per plane) and Oric overlay RAM ($C000-$FFFF via
// include/loci.h's enable_overlay_ram()/disable_overlay_ram()) instead of
// bank-switched VDC RAM.
//
// Ring design: a fixed UNDO_MAX_SLOTS-slot array, each slot independently
// storing its own snapshot at its own byte offset in the 16KB region. The
// slot about to be overwritten *next* is invalidated immediately (a
// sentinel), so undo/redo always stops cleanly at the true edge of
// available history. The byte bump-pointer (undo_address) only resets to
// the start of the region (forcing undo_number back to 1 too) when the
// *cumulative* byte budget would be exceeded by the next snapshot -- a
// rare event, not a per-snapshot check -- and even then, slots elsewhere
// in the ring keep their data until a *later* cycle's growing writes
// physically reach them, so history degrades gradually across a wrap,
// not all at once.
//
// undo_escapeundo() (vdcscreeneditor-v2's mid-operation-cancel cleanup)
// has no OSE equivalent: every call site here takes its snapshot only
// once an edit is actually about to be committed (after ENTER/
// confirmation, never speculatively before a cancellable step), so
// there's nothing to unwind on ESC.

#include "oric.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "loci.h"
#include "undo.h"

#define UNDO_MAX_SLOTS    40       // matches vdcscreeneditor-v2's Undo[41]
                                   // (40 active slots; the "+1" there is
                                   // folded into the valid flag here)
#define UNDO_REGION_BASE  0xC000U
#define UNDO_REGION_SIZE  0x4000U // 16KB, all of $C000-$FFFF

typedef struct {
    uint8_t  valid;       // 0 = no data / invalidated -- explicit flag,
                          // not an overloaded address==0 sentinel, since
                          // 0 is a legitimate offset into the region
    uint16_t address;     // offset into UNDO_REGION_BASE+address
    uint16_t x, y;         // canvas-absolute top-left of the dirty rect
    uint16_t width, height;
    uint8_t  redopresent; // 0 = no redo room, 1 = room reserved but not
                          // yet populated, 2 = populated
} UndoSlot;

static UndoSlot undo_slots[UNDO_MAX_SLOTS];
static uint8_t  undo_number;       // current slot, 1-based; 0 = nothing
                                    // recorded yet this session
static uint8_t  undo_undopossible; // gate: can undo_perform() do anything
static uint8_t  undo_redopossible; // gate: can redo_perform() do anything
static uint16_t undo_address;      // next write offset into the region

/**
 * Reset all undo/redo state. Call once from editor_run()'s init block.
 *
 * @return (none)
 */
void undo_init(void)
{
    uint8_t i;
    for (i = 0; i < UNDO_MAX_SLOTS; i++) undo_slots[i].valid = 0;
    undo_number        = 0;
    undo_undopossible  = 0;
    undo_redopossible  = 0;
    undo_address       = 0;
}

/**
 * Copy a w-byte row between screenmap[] and overlay RAM. Caller must
 * already be inside a PHP/SEI/enable_overlay_ram() ... disable_overlay_
 * ram()/PLP bracket (see undo_snapshot()/undo_perform()/redo_perform()).
 *
 * @param canvas_off Offset into screenmap[] (row*canvas_width + x).
 * @param ovl_off    Offset into the UNDO_REGION_BASE+offset overlay slot.
 * @param w          Row length in bytes.
 * @param to_overlay 1 to copy screenmap->overlay, 0 for overlay->screenmap.
 * @return (none)
 */
static void undo_copy_row(uint16_t canvas_off, uint16_t ovl_off, uint16_t w, uint8_t to_overlay)
{
    uint8_t *canvas_ptr = &screenmap[canvas_off];
    uint8_t *ovl_ptr    = (uint8_t *)(UNDO_REGION_BASE + ovl_off);
    uint16_t i;

    if (to_overlay)
        for (i = 0; i < w; i++) ovl_ptr[i] = canvas_ptr[i];
    else
        for (i = 0; i < w; i++) canvas_ptr[i] = ovl_ptr[i];
}

/**
 * Snapshot the rect [x,y .. x+w,y+h) from screenmap[] before a destructive
 * edit overwrites it. See undo.h for the full contract.
 *
 * @param x Canvas-absolute left column.
 * @param y Canvas-absolute top row.
 * @param w Rect width in cells.
 * @param h Rect height in cells.
 * @return (none)
 */
void undo_snapshot(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t bytes = (uint16_t)(w * h);
    uint8_t  redoroom = 1;
    uint16_t next_addr;
    uint16_t row;

    if (!loci_present()) return;

    undo_redopossible = 0;
    undo_undopossible  = 1;

    undo_number = (uint8_t)((undo_number % UNDO_MAX_SLOTS) + 1);

    next_addr = (uint16_t)(undo_address + bytes);
    if (next_addr > UNDO_REGION_SIZE)
    {
        undo_number  = 1;
        undo_address = 0;
        next_addr    = bytes;
    }

    if ((uint16_t)(next_addr + bytes) > UNDO_REGION_SIZE) redoroom = 0;

    __asm { php }
    __asm { sei }
    enable_overlay_ram();
    for (row = 0; row < h; row++)
        undo_copy_row((uint16_t)((y + row) * app.canvas_width + x),
                      (uint16_t)(undo_address + row * w), w, 1);
    disable_overlay_ram();
    __asm { plp }

    undo_slots[undo_number - 1].valid       = 1;
    undo_slots[undo_number - 1].address     = undo_address;
    undo_slots[undo_number - 1].x           = x;
    undo_slots[undo_number - 1].y           = y;
    undo_slots[undo_number - 1].width       = w;
    undo_slots[undo_number - 1].height      = h;
    undo_slots[undo_number - 1].redopresent = redoroom;

    // Invalidate the slot that will be overwritten next, so undo/redo
    // always stops cleanly at the true edge of available history.
    undo_slots[(undo_number < UNDO_MAX_SLOTS) ? undo_number : 0].valid = 0;

    undo_address = (uint16_t)(undo_address + (redoroom ? bytes * 2 : bytes));
}

/**
 * Undo the most recent snapshot ('z' in Main mode). See undo.h for the
 * full contract.
 *
 * @return (none)
 */
void undo_perform(void)
{
    UndoSlot *slot;
    uint16_t  bytes, row;

    if (!undo_undopossible || !loci_present()) return;

    slot  = &undo_slots[undo_number - 1];
    bytes = (uint16_t)(slot->width * slot->height);

    __asm { php }
    __asm { sei }
    enable_overlay_ram();
    if (slot->redopresent)
        for (row = 0; row < slot->height; row++)
            undo_copy_row((uint16_t)((slot->y + row) * app.canvas_width + slot->x),
                          (uint16_t)(slot->address + bytes + row * slot->width), slot->width, 1);
    for (row = 0; row < slot->height; row++)
        undo_copy_row((uint16_t)((slot->y + row) * app.canvas_width + slot->x),
                      (uint16_t)(slot->address + row * slot->width), slot->width, 0);
    disable_overlay_ram();
    __asm { plp }

    canvas_blit();
    statusbar_draw();

    if (slot->redopresent)
    {
        slot->redopresent = 2;
        undo_redopossible = 1;
    }
    undo_address = slot->address;

    undo_number--;
    if (undo_number == 0 && undo_slots[UNDO_MAX_SLOTS - 1].valid)
        undo_number = UNDO_MAX_SLOTS;

    undo_undopossible = (undo_number > 0) ? undo_slots[undo_number - 1].valid : 0;
}

/**
 * Redo the most recently undone snapshot ('y' in Main mode). See undo.h
 * for the full contract.
 *
 * @return (none)
 */
void redo_perform(void)
{
    UndoSlot *slot;
    uint16_t  bytes, row;
    uint8_t   next_index;

    if (!undo_redopossible || !loci_present()) return;

    undo_number = (uint8_t)((undo_number < UNDO_MAX_SLOTS) ? undo_number + 1 : 1);

    slot  = &undo_slots[undo_number - 1];
    bytes = (uint16_t)(slot->width * slot->height);

    __asm { php }
    __asm { sei }
    enable_overlay_ram();
    for (row = 0; row < slot->height; row++)
        undo_copy_row((uint16_t)((slot->y + row) * app.canvas_width + slot->x),
                      (uint16_t)(slot->address + bytes + row * slot->width), slot->width, 0);
    disable_overlay_ram();
    __asm { plp }

    canvas_blit();
    statusbar_draw();

    undo_address      = slot->address;
    undo_undopossible = 1;

    next_index = (uint8_t)((undo_number < UNDO_MAX_SLOTS) ? undo_number : 0);
    undo_redopossible = (undo_slots[next_index].redopresent == 2) ? 1 : 0;
}
