//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021-2022 Graham Sanderson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DOOM graphics stuff for SDL.
//


#include <assert.h>
#include <stdbool.h>
#include <string.h>


#include "config.h"
#include "d_loop.h"
#include "deh_str.h"
#include "doomtype.h"
#include "doom/r_data.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "picodoom.h"

#include "tables.h"
#include "v_diskicon.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#if PICO_ON_DEVICE
#include "hardware/dma.h"
#include "hardware/structs/xip.h"
#endif

// palette
static uint16_t palette[256];
static uint16_t shared_pal[NUM_SHARED_PALETTES][16];
static int8_t next_pal=-1;

// display has been set up?

static boolean initialized = false;

// disable mouse?

int usemouse = 1;

// The screen buffer; this is modified to draw things to the screen

pixel_t *I_VideoBuffer = NULL;

// If true, game is running as a screensaver

boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible = true;

// Gamma correction level to use

isb_int8_t usegamma = 0;

#ifndef NO_USE_JOYSTICK
// Joystick/gamepad hysteresis
unsigned int joywait = 0;
#endif

uint8_t __attribute__((aligned(4))) frame_buffer[2][SCREENWIDTH*MAIN_VIEWHEIGHT];

uint8_t *wipe_yoffsets; // position of start of y in each column
int16_t *wipe_yoffsets_raw;
uint32_t *wipe_linelookup; // offset of each line from start of screenbuffer (can be negative for FB 1 to FB 0)
uint8_t next_video_type;
uint8_t next_frame_index; // todo combine with video type?
uint8_t next_overlay_index;
#if !DEMO1_ONLY
uint8_t *next_video_scroll;
uint8_t *video_scroll;
#endif
volatile uint8_t wipe_min;

uint8_t display_frame_index;
uint8_t display_overlay_index;
uint8_t display_video_type;

semaphore_t render_frame_ready, display_frame_freed;

static const patch_t *stbar;

volatile uint8_t interp_in_use;

void I_SetGrabMouseCallback(grabmouse_callback_t func)
{

}

// Set the variable controlling FPS dots.

void I_DisplayFPSDots(boolean dots_on)
{

}

void I_ShutdownGraphics(void)
{
    if (initialized)
    {
        initialized = false;
    }
}


//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}

static void I_ToggleFullScreen(void)
{
}

void I_GetEvent(void)
{
    //TODO
    extern void I_32BlitInput();
    I_32BlitInput();
}

//
// I_StartTic
//
void I_StartTic (void)
{
    if (!initialized)
    {
        return;
    }

    I_GetEvent();
}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
}


//
// I_ReadScreen
//
void I_ReadScreen (pixel_t* scr)
{
    //memcpy(scr, I_VideoBuffer, SCREENWIDTH*SCREENHEIGHT*sizeof(*scr));
}


//
// I_SetPalette
//
void I_SetPalette (should_be_const byte *doompalette)
{
}

void I_SetPaletteNum(int doompalette)
{
    next_pal = doompalette;
}

// Given an RGB value, find the closest matching palette index.

int I_GetPaletteIndex(int r, int g, int b)
{
    return 0;
}

// 
// Set the window title
//

void I_SetWindowTitle(const char *title)
{
}

//
// Call the SDL function to set the window title, based on 
// the title set with I_SetWindowTitle.
//

void I_InitWindowTitle(void)
{
}

// Set the application icon

void I_InitWindowIcon(void)
{
}

void I_GraphicsCheckCommandLine(void)
{
}

// Check if we have been invoked as a screensaver by xscreensaver.

void I_CheckIsScreensaver(void)
{
}

void I_GetWindowPosition(int *x, int *y, int w, int h)
{
    x = y = 0;
}

void I_InitGraphics(void)
{
    should_be_const byte *doompal;

    stbar = resolve_vpatch_handle(VPATCH_STBAR);

    sem_init(&render_frame_ready, 0, 2);
    sem_init(&display_frame_freed, 1, 2);

    pd_init();

    // Set the palette

    //doompal = W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE);
    //I_SetPalette(doompal);


    initialized = true;

    // Call I_ShutdownGraphics on quit

    I_AtExit(I_ShutdownGraphics, true);
}

// Bind all variables controlling video options into the configuration
// file system.
void I_BindVideoVariables(void)
{
}

static inline uint16_t pack_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11);
}

void __noinline new_frame_init_overlays_palette_and_wipe() {
    // re-initialize our overlay drawing
    if (display_video_type >= FIRST_VIDEO_TYPE_WITH_OVERLAYS) {
        memset(vpatchlists->vpatch_next, 0, sizeof(vpatchlists->vpatch_next));
        memset(vpatchlists->vpatch_starters, 0, sizeof(vpatchlists->vpatch_starters));
        memset(vpatchlists->vpatch_doff, 0, sizeof(vpatchlists->vpatch_doff));
        vpatchlist_t *overlays = vpatchlists->overlays[display_overlay_index];
        // do it in reverse so our linked lists are in ascending order
        for (int i = overlays->header.size - 1; i > 0; i--) {
            assert(overlays[i].entry.y < count_of(vpatchlists->vpatch_starters));
            vpatchlists->vpatch_next[i] = vpatchlists->vpatch_starters[overlays[i].entry.y];
            vpatchlists->vpatch_starters[overlays[i].entry.y] = i;
        }
        if (next_pal != -1) {
            static const uint8_t *playpal;
            static bool calculate_palettes;
            if (!playpal) {
                lumpindex_t l = W_GetNumForName("PLAYPAL");
                playpal = W_CacheLumpNum(l, PU_STATIC);
                calculate_palettes = W_LumpLength(l) == 768;
            }
            if (!calculate_palettes || !next_pal) {
                const uint8_t *doompalette = playpal + next_pal * 768;
                for (int i = 0; i < 256; i++) {
                    int r = *doompalette++;
                    int g = *doompalette++;
                    int b = *doompalette++;
                    if (usegamma) {
                        r = gammatable[usegamma-1][r];
                        g = gammatable[usegamma-1][g];
                        b = gammatable[usegamma-1][b];
                    }
                    palette[i] = pack_rgb565(r, g, b);
                }
            } else {
                int mul, r0, g0, b0;
                if (next_pal < 9) {
                    mul = next_pal * 65536 / 9;
                    r0 = 255; g0 = b0 = 0;
                } else if (next_pal < 13) {
                    mul = (next_pal - 8) * 65536 / 8;
                    r0 = 215; g0 = 186; b0 = 69;
                } else {
                    mul = 65536 / 8;
                    r0 = b0 = 0; g0 = 256;
                }
                const uint8_t *doompalette = playpal;
                for (int i = 0; i < 256; i++) {
                    int r = *doompalette++;
                    int g = *doompalette++;
                    int b = *doompalette++;
                    r += ((r0 - r) * mul) >> 16;
                    g += ((g0 - g) * mul) >> 16;
                    b += ((b0 - b) * mul) >> 16;
                    palette[i] = pack_rgb565(r, g, b);
                }
            }
            next_pal = -1;
            assert(vpatch_type(stbar) == vp4_solid); // no transparent, no runs, 4 bpp
            for (int i = 0; i < NUM_SHARED_PALETTES; i++) {
                patch_t *patch = resolve_vpatch_handle(vpatch_for_shared_palette[i]);
                assert(vpatch_colorcount(patch) <= 16);
                assert(vpatch_has_shared_palette(patch));
                for (int j = 0; j < 16; j++) {
                    shared_pal[i][j] = palette[vpatch_palette(patch)[j]];
                }
            }
        }
        if (display_video_type == VIDEO_TYPE_WIPE) {
//            printf("WIPEMIN %d\n", wipe_min);
            if (wipe_min <= 200) {
                bool regular = display_overlay_index; // just happens to toggle every frame
                int new_wipe_min = 200;
                for (int i = 0; i < SCREENWIDTH; i++) {
                    int v;
                    if (wipe_yoffsets_raw[i] < 0) {
                        if (regular) {
                            wipe_yoffsets_raw[i]++;
                        }
                        v = 0;
                    } else {
                        int dy = (wipe_yoffsets_raw[i] < 16) ? (1 + wipe_yoffsets_raw[i] + regular) / 2 : 4;
                        if (wipe_yoffsets_raw[i] + dy > 200) {
                            v = 200;
                        } else {
                            wipe_yoffsets_raw[i] += dy;
                            v = wipe_yoffsets_raw[i];
                        }
                    }
                    wipe_yoffsets[i] = v;
                    if (v < new_wipe_min) new_wipe_min = v;
                }
                assert(new_wipe_min >= wipe_min);
                wipe_min = new_wipe_min;
            }
        }
    }
}

void I_32BlitStartRender()
{
    display_video_type = next_video_type;
    display_frame_index = next_frame_index;
    display_overlay_index = next_overlay_index;
#if !DEMO1_ONLY
    video_scroll = next_video_scroll; // todo does this waste too much space
#endif

    // palette init
    if (display_video_type != VIDEO_TYPE_SAVING)
        new_frame_init_overlays_palette_and_wipe();

    sem_release(&display_frame_freed);
}

static inline unsigned draw_vpatch(uint16_t *dest, patch_t *patch, vpatchlist_t *vp, unsigned off) {
    int repeat = vp->entry.repeat;
    dest += vp->entry.x;
    int w = vpatch_width(patch);
    const uint8_t *data0 = vpatch_data(patch);
    const uint8_t *data = data0 + off;
    if (!vpatch_has_shared_palette(patch)) {
        const uint8_t *pal = vpatch_palette(patch);
        switch (vpatch_type(patch)) {
            case vp4_runs: {
                uint16_t *p = dest;
                uint16_t *pend = dest + w;
                uint8_t gap;
                while (0xff != (gap = *data++)) {
                    p += gap;
                    int len = *data++;
                    for (int i = 1; i < len; i += 2) {
                        unsigned v = *data++;
                        *p++ = palette[pal[v & 0xf]];
                        *p++ = palette[pal[v >> 4]];
                    }
                    if (len & 1) {
                        *p++ = palette[pal[(*data++) & 0xf]];
                    }
                    assert(p <= pend);
                    if (p == pend) break;
                }
                break;
            }
            case vp4_alpha: {
                uint16_t *p = dest;
                for (int i = 0; i < w / 2; i++) {
                    unsigned v = *data++;
                    if (v & 0xf) p[0] = palette[pal[v & 0xf]];
                    if (v >> 4) p[1] = palette[pal[v >> 4]];
                    p += 2;
                }
                if (w & 1) {
                    unsigned v = *data++;
                    if (v & 0xf) p[0] = palette[pal[v & 0xf]];
                }
                break;
            }
            case vp4_solid: {
                uint16_t *p = dest;
                for (int i = 0; i < w / 2; i++) {
                    unsigned v = *data++;
                    p[0] = palette[pal[v & 0xf]];
                    p[1] = palette[pal[v >> 4]];
                    p += 2;
                }
                if (w & 1) {
                    unsigned v = *data++;
                    p[0] = palette[pal[v & 0xf]];
                }
                break;
            }
            case vp6_runs: {
                uint16_t *p = dest;
                uint16_t *pend = dest + w;
                uint8_t gap;
                while (0xff != (gap = *data++)) {
                    p += gap;
                    int len = *data++;
                    for (int i = 3; i < len; i += 4) {
                        unsigned v = *data++;
                        v |= (*data++) << 8;
                        v |= (*data++) << 16;
                        *p++ = palette[pal[v & 0x3f]];
                        *p++ = palette[pal[(v >> 6) & 0x3f]];
                        *p++ = palette[pal[(v >> 12) & 0x3f]];
                        *p++ = palette[pal[(v >> 18) & 0x3f]];
                    }
                    len &= 3;
                    if (len--) {
                        unsigned v = *data++;
                        *p++ = palette[pal[v & 0x3f]];
                        if (len--) {
                            v >>= 6;
                            v |= (*data++) << 2;
                            *p++ = palette[pal[v & 0x3f]];
                            if (len--) {
                                v >>= 6;
                                v |= (*data++) << 4;
                                *p++ = palette[pal[v & 0x3f]];
                                assert(!len);
                            }
                        }
                    }
                    assert(p <= pend);
                    if (p == pend) break;
                }
                break;
            }
            case vp8_runs: {
                uint16_t *p = dest;
                uint16_t *pend = dest + w;
                uint8_t gap;
                while (0xff != (gap = *data++)) {
                    p += gap;
                    int len = *data++;
                    for (int i = 0; i < len; i++) {
                        *p++ = palette[pal[*data++]];
                    }
                    assert(p <= pend);
                    if (p == pend) break;
                }
                break;
            }
            case vp_border: {
                dest[0] = palette[*data++];
                uint16_t col = palette[*data++];
                for (int i = 1; i < w - 1; i++) dest[i] = col;
                dest[w-1] = palette[*data++];
                break;
            }
            default:
                assert(false);
                break;
        }
    } else {
        unsigned sp = vpatch_shared_palette(patch);
        uint16_t *pal16 = shared_pal[sp];
        assert(sp < NUM_SHARED_PALETTES);
        switch (vpatch_type(patch)) {
            case vp4_solid: {
#if PICO_ON_DEVICE
                if (patch == stbar) {
                    static const uint8_t *cached_data;
#if PICO_RP2040
                    static uint32_t __scratch_x("data_cache") data_cache[41];
#else
                    // short of scratch space on RP2350 for some reason, so lets put this in main RAM
                    static uint32_t data_cache[41];
#endif
                    int i = 0;
                    uint32_t *d = (uint32_t *) dest;
#define DMA_CHANNEL 11
                    if (cached_data == data) {
                        const uint8_t *source = (const uint8_t *) data_cache;
                        // we need to correct for the misalignment of data, because the XIP copy ignores the low 2 bits...
                        // the raw bitmap data is always misaligned by 3 (the size of the header in the case of stbar)
                        source += 3;
                        for (; source < (const uint8_t *) dma_hw->ch[DMA_CHANNEL].al1_write_addr; source++) {
                            uint32_t val = pal16[source[0] & 0xf];
                            val |= (pal16[source[0] >> 4]) << 16;
                            *d++ = val;
                        }
                        source -= 3;
                        i = (source - (const uint8_t *) data_cache);
                    }
                    if (true) {
                        //                        once = true;
                        xip_ctrl_hw->stream_ctr = 0;
                        // workaround yucky bug
#if !PICO_RP2350
                        (void) *(io_rw_32 *) XIP_NOCACHE_NOALLOC_BASE;
                        xip_ctrl_hw->stream_fifo;
#endif
                        dma_channel_abort(DMA_CHANNEL);
                        dma_channel_config c = dma_channel_get_default_config(DMA_CHANNEL);
                        channel_config_set_read_increment(&c, false);
                        channel_config_set_write_increment(&c, true);
                        channel_config_set_dreq(&c, DREQ_XIP_STREAM);
#if !PICO_RP2350
                        dma_channel_set_read_addr(DMA_CHANNEL, (void *) XIP_AUX_BASE, false);
#else
                        dma_channel_set_read_addr(DMA_CHANNEL, &xip_ctrl_hw->stream_fifo, false);
#endif
                        dma_channel_set_config(DMA_CHANNEL, &c, false);
                        cached_data = data + SCREENWIDTH / 2;
                        xip_ctrl_hw->stream_addr = (uintptr_t) cached_data;
                        xip_ctrl_hw->stream_ctr = 41;
                        __compiler_memory_barrier();
                        dma_channel_transfer_to_buffer_now(DMA_CHANNEL, data_cache, 41);
                    }
                    for (; i < SCREENWIDTH / 2; i++) {
                        uint32_t val = pal16[data[i] & 0xf];
                        val |= (pal16[data[i] >> 4]) << 16;
                        *d++ = val;
                    }
                    data += SCREENWIDTH / 2;
                    break; // early break from switch
                }
#endif
                if (((uintptr_t)dest)&3) {
                    uint16_t *p = dest;
                    for (int i = 0; i < w / 2; i++) {
                        unsigned v = *data++;
                        p[0] = pal16[v & 0xf];
                        p[1] = pal16[v >> 4];
                        p += 2;
                    }
                } else {
                    uint32_t *wide = (uint32_t *) dest;
                    for (int i = 0; i < w / 2; i++) {
                        unsigned v = *data++;
                        wide[i] = pal16[v & 0xf] | (pal16[v >> 4] << 16);
                    }
                }
                if (w & 1) {
                    unsigned v = *data++;
                    dest[w-1] = pal16[v & 0xf];
                }
                break;
            }
            case vp4_alpha: {
                uint16_t *p = dest;
                for (int i = 0; i < w / 2; i++) {
                    unsigned v = *data++;
                    if (v & 0xf) p[0] = pal16[v & 0xf];
                    if (v >> 4) p[1] = pal16[v >> 4];
                    p += 2;
                }
                if (w & 1) {
                    unsigned v = *data++;
                    if (v & 0xf) p[0] = pal16[v & 0xf];
                }
                break;
            }
            default:
                assert(false);
        }
    }
    if (repeat) {
        // we need them to be solid... which they are, but if not you'll just get some visual funk
        //assert(vpatch_type(patch) == vp4_solid);
        if (vp->entry.patch_handle == VPATCH_M_THERMM) w--; // hackity hack
        for(int i=0;i<repeat*w;i++) {
            dest[w+i] = dest[i];
        }
    }
    return data - data0;
}

static void DrawLine(int line, uint8_t *ptr)
{
    // assume 565
    uint16_t *ptr16 = (uint16_t *)ptr;

    switch(display_video_type)
    {
        case VIDEO_TYPE_DOUBLE:
        {
            if(line < MAIN_VIEWHEIGHT)
            {
                uint8_t *in_ptr = frame_buffer[display_frame_index] + line * SCREENWIDTH;

                for(int x = 0; x < SCREENWIDTH; x++)
                    *ptr16++ = palette[*in_ptr++];
            }
            break;
        }

        case VIDEO_TYPE_SINGLE:
        {
            uint8_t *in_ptr;
            if(line < MAIN_VIEWHEIGHT)
                in_ptr = frame_buffer[display_frame_index] + line * SCREENWIDTH;
            else
                in_ptr = frame_buffer[display_frame_index ^ 1] + (line - 32) * SCREENWIDTH;

#if !DEMO1_ONLY
            if (video_scroll)
            {
                for(int i=SCREENWIDTH-1;i>0;i--)
                    in_ptr[i] = in_ptr[i-1];

                in_ptr[0] = video_scroll[line];
            }
#endif

            for(int x = 0; x < SCREENWIDTH; x++)
                *ptr16++ = palette[*in_ptr++];
            break;
        }

        case VIDEO_TYPE_WIPE:
        {
            uint8_t *in_ptr;
            if(line < MAIN_VIEWHEIGHT)
                in_ptr = frame_buffer[display_frame_index] + line * SCREENWIDTH;
            else
                in_ptr = frame_buffer[display_frame_index ^ 1] + (line - 32) * SCREENWIDTH;

            assert(wipe_yoffsets && wipe_linelookup);
            for (int x = 0; x < SCREENWIDTH; x++)
            {
                int rel = line - wipe_yoffsets[x];
                if (rel < 0)
                    ptr16[x] = palette[in_ptr[x]];
                else
                {
                    const uint8_t *flip;

                    flip = &frame_buffer[0][0] + wipe_linelookup[rel];

                    // todo better protection here
                    if (flip >= &frame_buffer[0][0] && flip < &frame_buffer[0][0] + 2 * SCREENWIDTH * MAIN_VIEWHEIGHT)
                        ptr16[x] = palette[flip[x]];
                }
            }
            break;
        }
    }

    // vpatch
    if (display_video_type >= FIRST_VIDEO_TYPE_WITH_OVERLAYS) {
        assert(line < count_of(vpatchlists->vpatch_starters));
        int prev = 0;
        for (int vp = vpatchlists->vpatch_starters[line]; vp;) {
            int next = vpatchlists->vpatch_next[vp];
            while (vpatchlists->vpatch_next[prev] && vpatchlists->vpatch_next[prev] < vp) {
                prev = vpatchlists->vpatch_next[prev];
            }
            assert(prev != vp);
            assert(vpatchlists->vpatch_next[prev] != vp);
            vpatchlists->vpatch_next[vp] = vpatchlists->vpatch_next[prev];
            vpatchlists->vpatch_next[prev] = vp;
            prev = vp;
            vp = next;
        }
        vpatchlist_t *overlays = vpatchlists->overlays[display_overlay_index];
        prev = 0;
        for (int vp = vpatchlists->vpatch_next[prev]; vp; vp = vpatchlists->vpatch_next[prev]) {
            patch_t *patch = resolve_vpatch_handle(overlays[vp].entry.patch_handle);
            int yoff = line - overlays[vp].entry.y;
            if (yoff < vpatch_height(patch)) {
                vpatchlists->vpatch_doff[vp] = draw_vpatch(ptr16, patch, &overlays[vp],
                                                            vpatchlists->vpatch_doff[vp]);
                prev = vp;
            } else {
                vpatchlists->vpatch_next[prev] = vpatchlists->vpatch_next[vp];
            }
        }
    }
}

void I_32BlitEndRender(uint8_t *data, int w, int h)
{
    uint8_t *ptr = data;

    for(int y = 0; y < SCREENHEIGHT; y++)
    {
        DrawLine(y, ptr);
        ptr += w * 2;

        if(y % 5 == 0)
        {
            // duplicate every 5th line
            memcpy(ptr, ptr - w * 2, w * 2);
            ptr += w * 2;
        }
    }
}