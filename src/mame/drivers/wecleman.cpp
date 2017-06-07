// license:BSD-3-Clause
// copyright-holders:Luca Elia
/***************************************************************************
                        WEC Le Mans 24  &   Hot Chase

                          (C)   1986 & 1988 Konami

                    driver by       Luca Elia (l.elia@tin.it)

- Note: press F2 to enter service mode -

---------------------------------------------------------------------------
                                TODO list
---------------------------------------------------------------------------
WEC Le Mans 24:
- The parallactic scrolling is sometimes wrong (related to v-cnt bit enabled?)
Hot Chase:
- Sound BGMs are regressed (hiccups badly);
- Samples pitch is too low, for instance game over speech;
Common Issues:
- Too many hacks with protection/blitter/colors.
  Additionally, there's a bug report that claims that current arrangement is broken for later levels in WEC Le Mans.
  007643 / 007645 could do with a rewrite, in short.
- One ROM unused (32K in hotchase, 16K in wecleman)
- Incomplete DSWs
- Sprite ram is not cleared by the game and no sprite list end-marker
  is written. We cope with that with an hack in the Blitter but there
  must be a register to do the trick



----------------------------------------------------------------------
Hardware                Main    Sub             Sound   Sound Chips
----------------------------------------------------------------------
[WEC Le Mans 24]        68000   68000   Z-80    YM2151 YM3012 1x007232

[Hot Chase]             68000   68000   68B09E                3x007232

[CPU PCB GX763 350861B]
    007641  007770  3x007232  051550

[VID PCB GX763 350860A AI AM-1]
    007634  007635  3x051316  007558  007557
----------------------------------------------------------------------


----------------------------------------------------------------
Main CPU                     [WEC Le Mans 24]     [Hot Chase]
----------------------------------------------------------------
ROM                R         000000-03ffff        <
Work RAM           RW        040000-043fff        040000-063fff*
?                  RW        060000-060007        -
Blitter             W        080000-080011        <
Page RAM           RW        100000-103fff        -
Text RAM           RW        108000-108fff        -
Palette RAM        RW        110000-110fff        110000-111fff**
Shared RAM         RW        124000-127fff        120000-123fff
Sprites RAM        RW        130000-130fff        <
Input Ports        RW        1400xx-1400xx        <
Background         RW                             100000-100fff
Background Ctrl     W        -                    101000-10101f
Foreground         RW        -                    102000-102fff
Foreground Ctrl     W        -                    103000-10301f

* weird    ** only half used


----------------------------------------------------------------
Sub CPU                      [WEC Le Mans 24]     [Hot Chase]
----------------------------------------------------------------

ROM                R         000000-00ffff        000000-01ffff
Work RAM           RW        -                    060000-060fff
Road RAM           RW        060000-060fff        020000-020fff
Shared RAM         RW        070000-073fff        040000-043fff


---------------------------------------------------------------------------
                                Game code
                            [WEC Le Mans 24]
---------------------------------------------------------------------------

                    Interesting locations (main cpu)
                    --------------------------------

There's some 68000 assembly code in ASCII around d88 :-)

040000+
7-9                             *** hi score/10 (BCD 3 bytes) ***
b-d                             *** score/10 (BCD 3 bytes) ***
1a,127806               <- 140011.b
1b,127807               <- 140013.b
1c,127808               <- 140013.b
1d,127809               <- 140015.b
1e,12780a               <- 140017.b
1f                      <- 140013.b
30                              *** credits ***
3a,3b,3c,3d             <-140021.b
3a = accelerator   3b = ??   3c = steering   3d = table

d2.w                    -> 108f24 fg y scroll
112.w                   -> 108f26 bg y scroll

16c                             influences 140031.b
174                             screen address
180                             input port selection (->140003.b ->140021.b)
181                     ->140005.b
185                             bit 7 high -> must copy sprite data to 130000
1dc+(1da).w             ->140001.b

40a.w,c.w               *** time (BCD) ***
411                             EF if brake, 0 otherwise
416                             ?
419                             gear: 0=lo,1=hi
41e.w                   speed related ->127880
424.w                   speed BCD
43c.w                   accel?
440.w                   level?

806.w                   scrollx related
80e.w                   scrolly related

c08.b                   routine select: 1>1e1a4 2>1e1ec 3>1e19e other>1e288 (map screen)

117a.b                  selected letter when entering name in hi-scores
117e.w                  cycling color in hi-scores

12c0.w                  ?time,pos,len related?
12c2.w
12c4.w
12c6.w

1400-1bff               color data (0000-1023 chars)
1c00-23ff               color data (1024-2047 sprites?)

2400                    Sprite data: 40 entries x  4 bytes =  100
2800                    Sprite data: 40 entries x 40 bytes = 1000
3800                    Sprite data: 40 entries x 10 bytes =  400

                    Interesting routines (main cpu)
                    -------------------------------

804                     mem test
818                     end mem test (cksums at 100, addresses at A90)
82c                     other cpu test
a0c                     rom test
c1a                     prints string (a1)
1028                    end test
204c                    print 4*3 box of chars to a1, made up from 2 2*6 (a0)=0xLR (Left,Righ index)
4e62                    raws in the fourth page of chars
6020                    test screen (print)
60d6                    test screen
62c4                    motor test?
6640                    print input port values ( 6698 = scr_disp.w,ip.b,bit.b[+/-] )

819c                    prepares sprite data
8526                    blitter: 42400->130000
800c                    8580    sprites setup on map screen

1833a                   cycle cols on hi-scores
18514                   hiscores: main loop
185e8                   hiscores: wheel selects letter

TRAP#0                  prints string: A0-> addr.l, attr.w, (char.b)*, 0

IRQs                    [1,3,6]  602
IRQs                    [2,7]    1008->12dc      ORI.W    #$2700,(A7) RTE
IRQs                    [4]      1004->124c
IRQs                    [5]      106c->1222      calls sequence: $3d24 $1984 $28ca $36d2 $3e78


                    Interesting locations (sub cpu)
                    -------------------------------

                    Interesting routines (sub cpu)
                    ------------------------------

1028    'wait for command' loop.
1138    lev4 irq
1192    copies E0*4 bytes: (a1)+ -> (a0)+


---------------------------------------------------------------------------
                                 Game code
                                [Hot Chase]
---------------------------------------------------------------------------

This game has been probably coded by the same programmers of WEC Le Mans 24
It shares some routines and there is the (hidden?) string "WEC 2" somewhere

                            Main CPU                Sub CPU

Interrupts: [1, 7]          FFFFFFFF                FFFFFFFF
Interrupts: [2,3,4,5,6]     221c                    1288

Self Test:
 0] pause,120002==55,pause,120002==AA,pause,120002==CC, (on error set bit d7.0)
 6] 60000-63fff(d7.1),40000-41fff(d7.2)
 8] 40000/2<-chksum 0-20000(e/o);40004/6<-chksum 20000-2ffff(e/o) (d7.3456)
 9] chksums from sub cpu: even->40004   odd->(40006)    (d7.78)
 A] 110000-111fff(even)(d7.9),102000-102fff(odd)(d7.a)
 C] 100000-100fff(odd)(d7.b),pause,pause,pause
10] 120004==0(d7.c),120006==0(d7.d),130000-1307ff(first $A of every $10 bytes only)(d7.e),pause
14] 102000<-hw screen+(d7==0)? jmp 1934/1000
15] 195c start of game


                    Interesting locations (main cpu)
                    --------------------------------

60024.b                 <- !140017.b (DSW 1 - coinage)
60025.b                 <- !140015.b (DSW 2 - options)
6102c.w                 *** time ***

                    Interesting routines (main cpu)
                    -------------------------------

18d2                    (d7.d6)?print BAD/OK to (a5)+, jmp(D0)
1d58                    print d2.w to (a4)+, jmp(a6)
580c                    writes at 60000
61fc                    print test strings
18cbe                   print "game over"

Revisions:

05-05-2002 David Haywood(Haze)
- improved Wec Le Mans steering

05-01-2002 Hiromitsu Shioya(Shica)
- fixed Hot Chase volume and sound interrupt

xx-xx-2003 Acho A. Tang
[Wec Le Mans 24]
- generalized blitter to support Wec Le Mans
- emulated custom alpha blending chip used for protection
- fixed game color and sound volume
- added shadows and sprite-to-sprite priority
- added curbs effect
- modified zoom equation to close tile gaps
- fixed a few tile glitches
- converted driver to use RGB direct
- cloud transition(needs verification from board owners)
- fixed sound banking
- source clean-up

TODO:
- check dust color on title screen(I don't think it should be black)
- check brake light(LED) support
- check occasional off-pitch music and samples(sound interrupt related?)

* Sprite, road and sky drawings do not support 32-bit color depth.
  Certain sprites with incorrect z-value still pop in front of closer
  billboards and some appear a few pixels off the ground. They could be
  the game's intrinsic flaws. (reference: www.system16.com)

[Hot Chase]
- shared changes with Wec Le Mans
- removed junk tiles during introduction(needs verification)

* Special thanks to Luca Elia for bringing us so many enjoyable games.

***************************************************************************/

#include "emu.h"

#include "cpu/m68000/m68000.h"
#include "cpu/m6809/m6809.h"
#include "cpu/z80/z80.h"
#include "machine/gen_latch.h"
#include "screen.h"
#include "sound/k007232.h"
#include "sound/ym2151.h"
#include "speaker.h"
#include "video/k051316.h"

#include "wecleman.lh"


class wecleman_state : public driver_device
{
public:
	wecleman_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_videostatus(*this, "videostatus"),
		m_protection_ram(*this, "protection_ram"),
		m_blitter_regs(*this, "blitter_regs"),
		m_pageram(*this, "pageram"),
		m_txtram(*this, "txtram"),
		m_spriteram(*this, "spriteram"),
		m_roadram(*this, "roadram"),
		m_generic_paletteram_16(*this, "paletteram"),
		m_maincpu(*this, "maincpu"),
		m_audiocpu(*this, "audiocpu"),
		m_subcpu(*this, "sub"),
		m_roz_1(*this, "roz_1"),
		m_roz_2(*this, "roz_2"),
		m_k007232(*this, "k007232"),
		m_k007232_1(*this, "k007232_1"),
		m_k007232_2(*this, "k007232_2"),
		m_k007232_3(*this, "k007232_3"),
		m_gfxdecode(*this, "gfxdecode"),
		m_palette(*this, "palette"),
		m_screen(*this, "screen"),
		m_soundlatch(*this, "soundlatch") { }

	optional_shared_ptr<uint16_t> m_videostatus;
	optional_shared_ptr<uint16_t> m_protection_ram;
	required_shared_ptr<uint16_t> m_blitter_regs;
	optional_shared_ptr<uint16_t> m_pageram;
	optional_shared_ptr<uint16_t> m_txtram;
	required_shared_ptr<uint16_t> m_spriteram;
	required_shared_ptr<uint16_t> m_roadram;
	required_shared_ptr<uint16_t> m_generic_paletteram_16;

	int m_multiply_reg[2];
	int m_spr_color_offs;
	int m_prot_state;
	int m_selected_ip;
	int m_irqctrl;
	int m_bgpage[4];
	int m_fgpage[4];
	const int *m_gfx_bank;
	tilemap_t *m_bg_tilemap;
	tilemap_t *m_fg_tilemap;
	tilemap_t *m_txt_tilemap;
	int *m_spr_idx_list;
	int *m_spr_pri_list;
	int *m_t32x32pm;
	int m_gameid;
	int m_spr_offsx;
	int m_spr_offsy;
	int m_spr_count;
	uint16_t *m_rgb_half;
	int m_cloud_blend;
	int m_cloud_ds;
	int m_cloud_visible;
	int m_sound_hw_type;
	bool m_hotchase_sound_hs;
	pen_t m_black_pen;
	struct sprite *m_sprite_list;
	struct sprite **m_spr_ptr_list;
	DECLARE_READ16_MEMBER(wecleman_protection_r);
	DECLARE_WRITE16_MEMBER(wecleman_protection_w);
	DECLARE_WRITE16_MEMBER(irqctrl_w);
	DECLARE_WRITE16_MEMBER(selected_ip_w);
	DECLARE_READ16_MEMBER(selected_ip_r);
	DECLARE_WRITE16_MEMBER(blitter_w);
	DECLARE_READ8_MEMBER(multiply_r);
	DECLARE_WRITE8_MEMBER(multiply_w);
	DECLARE_WRITE16_MEMBER(hotchase_soundlatch_w);
	DECLARE_WRITE8_MEMBER(hotchase_sound_control_w);
	DECLARE_WRITE16_MEMBER(wecleman_soundlatch_w);
	DECLARE_WRITE16_MEMBER(wecleman_txtram_w);
	DECLARE_WRITE16_MEMBER(wecleman_pageram_w);
	DECLARE_WRITE16_MEMBER(wecleman_videostatus_w);
	DECLARE_WRITE16_MEMBER(hotchase_paletteram16_SBGRBBBBGGGGRRRR_word_w);
	DECLARE_WRITE16_MEMBER(wecleman_paletteram16_SSSSBBBBGGGGRRRR_word_w);
	DECLARE_WRITE8_MEMBER(wecleman_K00723216_bank_w);
	DECLARE_WRITE8_MEMBER(wecleman_volume_callback);
	DECLARE_READ8_MEMBER(hotchase_1_k007232_r);
	DECLARE_WRITE8_MEMBER(hotchase_1_k007232_w);
	DECLARE_READ8_MEMBER(hotchase_2_k007232_r);
	DECLARE_WRITE8_MEMBER(hotchase_2_k007232_w);
	DECLARE_READ8_MEMBER(hotchase_3_k007232_r);
	DECLARE_WRITE8_MEMBER(hotchase_3_k007232_w);
	DECLARE_DRIVER_INIT(wecleman);
	DECLARE_DRIVER_INIT(hotchase);
	TILE_GET_INFO_MEMBER(wecleman_get_txt_tile_info);
	TILE_GET_INFO_MEMBER(wecleman_get_bg_tile_info);
	TILE_GET_INFO_MEMBER(wecleman_get_fg_tile_info);
	DECLARE_MACHINE_RESET(wecleman);
	DECLARE_VIDEO_START(wecleman);
	DECLARE_MACHINE_RESET(hotchase);
	DECLARE_VIDEO_START(hotchase);
	uint32_t screen_update_wecleman(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	uint32_t screen_update_hotchase(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	INTERRUPT_GEN_MEMBER(hotchase_sound_timer);
	TIMER_DEVICE_CALLBACK_MEMBER(wecleman_scanline);
	TIMER_DEVICE_CALLBACK_MEMBER(hotchase_scanline);
	void draw_cloud(bitmap_rgb32 &bitmap,gfx_element *gfx,uint16_t *tm_base,int x0,int y0,int xcount,int ycount,int scrollx,int scrolly,int tmw_l2,int tmh_l2,int alpha,int pal_offset);
	void wecleman_unpack_sprites();
	void bitswap(uint8_t *src,size_t len,int _14,int _13,int _12,int _11,int _10,int _f,int _e,int _d,int _c,int _b,int _a,int _9,int _8,int _7,int _6,int _5,int _4,int _3,int _2,int _1,int _0);
	void hotchase_sprite_decode( int num16_banks, int bank_size );
	void get_sprite_info();
	void sortsprite(int *idx_array, int *key_array, int size);
	template<class _BitmapClass> void do_blit_zoom32(_BitmapClass &bitmap, const rectangle &cliprect, struct sprite *sprite);
	template<class _BitmapClass> void sprite_draw(_BitmapClass &bitmap, const rectangle &cliprect);
	void wecleman_draw_road(bitmap_rgb32 &bitmap, const rectangle &cliprect, int priority);
	void hotchase_draw_road(bitmap_ind16 &bitmap, const rectangle &cliprect);
	DECLARE_CUSTOM_INPUT_MEMBER(hotchase_sound_status_r);
	DECLARE_WRITE8_MEMBER(hotchase_sound_hs_w);

	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_audiocpu;
	required_device<cpu_device> m_subcpu;
	optional_device<k051316_device> m_roz_1;
	optional_device<k051316_device> m_roz_2;
	optional_device<k007232_device> m_k007232;
	optional_device<k007232_device> m_k007232_1;
	optional_device<k007232_device> m_k007232_2;
	optional_device<k007232_device> m_k007232_3;
	required_device<gfxdecode_device> m_gfxdecode;
	required_device<palette_device> m_palette;
	required_device<screen_device> m_screen;
	required_device<generic_latch_8_device> m_soundlatch;
};

#define BMP_PAD     8
#define BLEND_STEPS 16
#define BLEND_MIN   0
#define BLEND_MAX   (BLEND_STEPS*0x20-1)
#define BLEND_INC   1
#define BLEND_DEC   -8

#define SPRITE_FLIPX    0x01
#define SPRITE_FLIPY    0x02
#define NUM_SPRITES     256

struct sprite
{
	uint8_t *pen_data;    /* points to top left corner of tile data */
	int line_offset;

	const pen_t *pal_data;
	rgb_t pal_base;

	int x_offset, y_offset;
	int tile_width, tile_height;
	int total_width, total_height;  /* in screen coordinates */
	int x, y;
	int shadow_mode, flags;
};


/***************************************************************************

                        Sprite Description and Routines
                        -------------------------------

    Sprites: 256 entries, 16 bytes each, first ten bytes used (and tested)

    Offset  Bits                    Meaning

    00.w    fedc ba98 ---- ----     Screen Y stop
            ---- ---- 7654 3210     Screen Y start

    02.w    fedc ba-- ---- ----     High bits of sprite "address"
            ---- --9- ---- ----     Flip Y ?
            ---- ---8 7654 3210     Screen X start

    04.w    fedc ba98 ---- ----     Color
            ---- ---- 7654 3210     Source Width / 8

    06.w    f--- ---- ---- ----     Flip X
            -edc ba98 7654 3210     Low bits of sprite "address"

    08.w    --dc ba98 ---- ----     Y? Shrink Factor
            ---- ---- --54 3210     X? Shrink Factor

    Sprite "address" is the index of the pixel the hardware has to start
    fetching data from, divided by 8. Only the on-screen height and source data
    width are provided, along with two shrinking factors. So on screen width
    and source height are calculated by the hardware using the shrink factors.
    The factors are in the range 0 (no shrinking) - 3F (half size).

    Hot Chase: shadow of trees is pen 0x0a

***************************************************************************/

void wecleman_state::get_sprite_info()
{
	const pen_t *base_pal = m_palette->pens();
	uint8_t *base_gfx = memregion("gfx1")->base();
	int gfx_max     = memregion("gfx1")->bytes();

	uint16_t *source = m_spriteram;

	struct sprite *sprite = m_sprite_list;
	struct sprite *finish = m_sprite_list + NUM_SPRITES;

	int bank, code, gfx, zoom;

	for (m_spr_count=0; sprite<finish; source+=0x10/2, sprite++)
	{
		if (source[0x00/2] == 0xffff) break;

		sprite->y = source[0x00/2] & 0xff;
		sprite->total_height = (source[0x00/2] >> 8) - sprite->y;
		if (sprite->total_height < 1) continue;

		sprite->x = source[0x02/2] & 0x1ff;
		bank = source[0x02/2] >> 10;
		if (bank == 0x3f) continue;

		sprite->tile_width = source[0x04/2] & 0xff;
		if (sprite->tile_width < 1) continue;

		sprite->shadow_mode = source[0x04/2] & 0x4000;

		code = source[0x06/2];
		zoom = source[0x08/2];

		sprite->pal_base = (source[0x0e/2] & 0xff) << 4;
		sprite->pal_data = base_pal + sprite->pal_base;

		gfx = (m_gfx_bank[bank] << 15) + (code & 0x7fff);

		sprite->flags = 0;
		if (code & 0x8000) { sprite->flags |= SPRITE_FLIPX; gfx += 1-sprite->tile_width; }
		if (source[0x02/2] & 0x0200) sprite->flags |= SPRITE_FLIPY;

		gfx <<= 3;
		sprite->tile_width <<= 3;
		sprite->tile_height = (sprite->total_height * 0x80) / (0x80 - (zoom >> 8)); // needs work

		if ((gfx + sprite->tile_width * sprite->tile_height - 1) >= gfx_max) continue;

		sprite->pen_data = base_gfx + gfx;
		sprite->line_offset = sprite->tile_width;
		sprite->total_width = sprite->tile_width - (sprite->tile_width * (zoom & 0xff)) / 0x80;
		sprite->total_height += 1;
		sprite->x += m_spr_offsx;
		sprite->y += m_spr_offsy;

		if (m_gameid == 0)
		{
			m_spr_idx_list[m_spr_count] = m_spr_count;
			m_spr_pri_list[m_spr_count] = source[0x0e/2] >> 8;
		}

		m_spr_ptr_list[m_spr_count] = sprite;
		m_spr_count++;
	}
}

// priority sorting, silly but good for smaller arrays
void wecleman_state::sortsprite(int *idx_array, int *key_array, int size)
{
	int i, j, tgt_val, low_val, low_pos, src_idx, tgt_idx, hi_idx;

	idx_array += size;

	for (j=-size; j<-1; j++)
	{
		src_idx = idx_array[j];
		low_pos = j;
		low_val = key_array[src_idx];
		hi_idx = src_idx;
		for (i=j+1; i; i++)
		{
			tgt_idx = idx_array[i];
			tgt_val = key_array[tgt_idx];
			if (low_val > tgt_val)
				{ low_val = tgt_val; low_pos = i; }
			else if ((low_val == tgt_val) && (hi_idx <= tgt_idx))
				{ hi_idx = tgt_idx; low_pos = i; }
		}
		low_val = idx_array[low_pos];
		idx_array[low_pos] = src_idx;
		idx_array[j] = low_val;
	}
}

// draws a 8bpp palette sprites on a 16bpp direct RGB target (sub-par implementation)
template<class _BitmapClass>
void wecleman_state::do_blit_zoom32(_BitmapClass &bitmap, const rectangle &cliprect, struct sprite *sprite)
{
#define PRECISION_X 20
#define PRECISION_Y 20
#define FPY_HALF (1<<(PRECISION_Y-1))

	const pen_t *pal_base;
	int src_f0y, src_fdy, src_f0x, src_fdx, src_fpx;
	int x1, x2, y1, y2, dx, dy, sx, sy;
	int xcount0=0, ycount0=0;

	if (sprite->flags & SPRITE_FLIPX)
	{
		x2 = sprite->x;
		x1 = x2 + sprite->total_width;
		dx = -1;
		if (x2 < cliprect.min_x) x2 = cliprect.min_x;
		if (x1 > cliprect.max_x )
		{
			xcount0 = x1 - cliprect.max_x;
			x1 = cliprect.max_x;
		}
		if (x2 >= x1) return;
		x1--; x2--;
	}
	else
	{
		x1 = sprite->x;
		x2 = x1 + sprite->total_width;
		dx = 1;
		if (x1 < cliprect.min_x )
		{
			xcount0 = cliprect.min_x - x1;
			x1 = cliprect.min_x;
		}
		if (x2 > cliprect.max_x ) x2 = cliprect.max_x;
		if (x1 >= x2) return;
	}

	if (sprite->flags & SPRITE_FLIPY)
	{
		y2 = sprite->y;
		y1 = y2 + sprite->total_height;
		dy = -1;
		if (y2 < cliprect.min_y ) y2 = cliprect.min_y;
		if (y1 > cliprect.max_y )
		{
			ycount0 = cliprect.max_y;
			y1 = cliprect.max_y;
		}
		if (y2 >= y1) return;
		y1--; y2--;
	}
	else
	{
		y1 = sprite->y;
		y2 = y1 + sprite->total_height;
		dy = 1;
		if (y1 < cliprect.min_y )
		{
			ycount0 = cliprect.min_y - y1;
			y1 = cliprect.min_y;
		}
		if (y2 > cliprect.max_y) y2 = cliprect.max_y;
		if (y1 >= y2) return;
	}

	// calculate entry point decimals
	src_fdy = (sprite->tile_height<<PRECISION_Y) / sprite->total_height;
	src_f0y = src_fdy * ycount0 + FPY_HALF;

	src_fdx = (sprite->tile_width<<PRECISION_X) / sprite->total_width;
	src_f0x = src_fdx * xcount0;

	// pre-loop assignments and adjustments
	pal_base = sprite->pal_data;

	if (x1 > cliprect.min_x)
	{
		x1 -= dx;
		x2 -= dx;
	}

	for (sy = y1; sy != y2; sy += dy)
	{
		uint8_t *row_base = sprite->pen_data + (src_f0y>>PRECISION_Y) * sprite->line_offset;
		src_fpx = src_f0x;
		typename _BitmapClass::pixel_t *dst_ptr = &bitmap.pix(sy);

		if (bitmap.format() == BITMAP_FORMAT_RGB32) // Wec Le Mans
		{
			if (!sprite->shadow_mode)
			{
				for (sx = x1; sx != x2; sx += dx)
				{
					int pix = row_base[src_fpx >> PRECISION_X];
					if (pix & 0x80) break;
					if (pix)
						dst_ptr[sx] = pal_base[pix];
					src_fpx += src_fdx;
				}
			}
			else
			{
				for (sx = x1; sx != x2; sx += dx)
				{
					int pix = row_base[src_fpx >> PRECISION_X];
					if (pix & 0x80) break;
					if (pix)
					{
						if (pix != 0xa)
							dst_ptr[sx] = pal_base[pix];
						else
							dst_ptr[sx] = (dst_ptr[sx] >> 1) & rgb_t(0x7f,0x7f,0x7f);
					}
					src_fpx += src_fdx;
				}
			}
		}
		else    // Hot Chase
		{
			pen_t base = sprite->pal_base;

			if (!sprite->shadow_mode)
			{
				for (sx = x1; sx != x2; sx += dx)
				{
					int pix = row_base[src_fpx >> PRECISION_X];
					if (pix & 0x80) break;
					if (pix)
						dst_ptr[sx] = base + pix;
					src_fpx += src_fdx;
				}
			}
			else
			{
				for (sx = x1; sx != x2; sx += dx)
				{
					int pix = row_base[src_fpx >> PRECISION_X];
					if (pix & 0x80) break;
					if (pix)
					{
						if (pix != 0xa)
							dst_ptr[sx] = base + pix;
						else
						{
							if (dst_ptr[sx] != m_black_pen)
								dst_ptr[sx] |= 0x800;
						}
					}
					src_fpx += src_fdx;
				}
			}
		}

		src_f0y += src_fdy;
	}
}

template<class _BitmapClass>
void wecleman_state::sprite_draw(_BitmapClass &bitmap, const rectangle &cliprect)
{
	int i;

	if (m_gameid == 0)   // Wec Le Mans
	{
		sortsprite(m_spr_idx_list, m_spr_pri_list, m_spr_count);

		for (i=0; i<m_spr_count; i++) do_blit_zoom32(bitmap, cliprect, m_spr_ptr_list[m_spr_idx_list[i]]);
	}
	else    // Hot Chase
	{
		for (i=0; i<m_spr_count; i++) do_blit_zoom32(bitmap, cliprect, m_spr_ptr_list[i]);
	}
}


/***************************************************************************

                    Background Description and Routines
                    -----------------------------------

                            [WEC Le Mans 24]

[ 2 Scrolling Layers ]
    [Background]
    [Foreground]
        Tile Size:              8x8

        Tile Format:            see wecleman_get_bg_tile_info()

        Layer Size:             4 Pages - Page0 Page1 Page2 Page3
                                each page is 512 x 256 (64 x 32 tiles)

        Page Selection Reg.:    108efe  [Bg]
                                108efc  [Fg]
                                4 pages to choose from

        Scrolling Columns:      1
        Scrolling Columns Reg.: 108f26  [Bg]
                                108f24  [Fg]

        Scrolling Rows:         224 / 8 (Screen-wise scrolling)
        Scrolling Rows Reg.:    108f82/4/6..    [Bg]
                                108f80/2/4..    [Fg]

[ 1 Text Layer ]
        Tile Size:              8x8

        Tile Format:            see wecleman_get_txt_tile_info()

        Layer Size:             1 Page: 512 x 256 (64 x 32 tiles)

        Scrolling:              -

[ 1 Road Layer ]

[ 256 Sprites ]
    Zooming Sprites, see below


                                [Hot Chase]

[ 3 Zooming Layers ]
    [Background]
    [Foreground (text)]
    [Road]

[ 256 Sprites ]
    Zooming Sprites, see below

***************************************************************************/

/***************************************************************************
                                WEC Le Mans 24
***************************************************************************/

#define PAGE_GFX        (0)
#define PAGE_NX         (0x40)
#define PAGE_NY         (0x20)
#define TILEMAP_DIMY    (PAGE_NY * 2 * 8)

/*------------------------------------------------------------------------
                [ Frontmost (text) layer + video registers ]
------------------------------------------------------------------------*/

TILE_GET_INFO_MEMBER(wecleman_state::wecleman_get_txt_tile_info)
{
	int code = m_txtram[tile_index];
	SET_TILE_INFO_MEMBER(PAGE_GFX, code&0xfff, (code>>5&0x78)+(code>>12), 0);
}

WRITE16_MEMBER(wecleman_state::wecleman_txtram_w)
{
	uint16_t old_data = m_txtram[offset];
	uint16_t new_data = COMBINE_DATA(&m_txtram[offset]);

	if ( old_data != new_data )
	{
		if (offset >= 0xE00/2 ) /* Video registers */
		{
			/* pages selector for the background */
			if (offset == 0xEFE/2)
			{
				m_bgpage[0] = (new_data >> 0x4) & 3;
				m_bgpage[1] = (new_data >> 0x0) & 3;
				m_bgpage[2] = (new_data >> 0xc) & 3;
				m_bgpage[3] = (new_data >> 0x8) & 3;
				m_bg_tilemap->mark_all_dirty();
			}

			/* pages selector for the foreground */
			if (offset == 0xEFC/2)
			{
				m_fgpage[0] = (new_data >> 0x4) & 3;
				m_fgpage[1] = (new_data >> 0x0) & 3;
				m_fgpage[2] = (new_data >> 0xc) & 3;
				m_fgpage[3] = (new_data >> 0x8) & 3;
				m_fg_tilemap->mark_all_dirty();
			}

			/* Parallactic horizontal scroll registers follow */
		}
		else
			m_txt_tilemap->mark_tile_dirty(offset);
	}
}

/*------------------------------------------------------------------------
                            [ Background ]
------------------------------------------------------------------------*/

TILE_GET_INFO_MEMBER(wecleman_state::wecleman_get_bg_tile_info)
{
	int page = m_bgpage[((tile_index&0x7f)>>6) + ((tile_index>>12)<<1)];
	int code = m_pageram[(tile_index&0x3f) + ((tile_index>>7&0x1f)<<6) + (page<<11)];

	SET_TILE_INFO_MEMBER(PAGE_GFX, code&0xfff, (code>>5&0x78)+(code>>12), 0);
}

/*------------------------------------------------------------------------
                            [ Foreground ]
------------------------------------------------------------------------*/

TILE_GET_INFO_MEMBER(wecleman_state::wecleman_get_fg_tile_info)
{
	int page = m_fgpage[((tile_index&0x7f)>>6) + ((tile_index>>12)<<1)];
	int code = m_pageram[(tile_index&0x3f) + ((tile_index>>7&0x1f)<<6) + (page<<11)];

	if (!code || code==0xffff) code = 0x20;
	SET_TILE_INFO_MEMBER(PAGE_GFX, code&0xfff, (code>>5&0x78)+(code>>12), 0);
}

/*------------------------------------------------------------------------
                    [ Pages (Background & Foreground) ]
------------------------------------------------------------------------*/

/* Pages that compose both the background and the foreground */
WRITE16_MEMBER(wecleman_state::wecleman_pageram_w)
{
	COMBINE_DATA(&m_pageram[offset]);

	{
		int page,col,row;

		page = ( offset ) / (PAGE_NX * PAGE_NY);
		col  = ( offset ) % PAGE_NX;
		row  = ( offset / PAGE_NX ) % PAGE_NY;

		/* background */
		if (m_bgpage[0] == page) m_bg_tilemap->mark_tile_dirty((col+PAGE_NX*0) + (row+PAGE_NY*0)*PAGE_NX*2 );
		if (m_bgpage[1] == page) m_bg_tilemap->mark_tile_dirty((col+PAGE_NX*1) + (row+PAGE_NY*0)*PAGE_NX*2 );
		if (m_bgpage[2] == page) m_bg_tilemap->mark_tile_dirty((col+PAGE_NX*0) + (row+PAGE_NY*1)*PAGE_NX*2 );
		if (m_bgpage[3] == page) m_bg_tilemap->mark_tile_dirty((col+PAGE_NX*1) + (row+PAGE_NY*1)*PAGE_NX*2 );

		/* foreground */
		if (m_fgpage[0] == page) m_fg_tilemap->mark_tile_dirty((col+PAGE_NX*0) + (row+PAGE_NY*0)*PAGE_NX*2 );
		if (m_fgpage[1] == page) m_fg_tilemap->mark_tile_dirty((col+PAGE_NX*1) + (row+PAGE_NY*0)*PAGE_NX*2 );
		if (m_fgpage[2] == page) m_fg_tilemap->mark_tile_dirty((col+PAGE_NX*0) + (row+PAGE_NY*1)*PAGE_NX*2 );
		if (m_fgpage[3] == page) m_fg_tilemap->mark_tile_dirty((col+PAGE_NX*1) + (row+PAGE_NY*1)*PAGE_NX*2 );
	}
}

/*------------------------------------------------------------------------
                                Road Drawing

    This layer is composed of horizontal lines gfx elements
    There are 256 lines in ROM, each is 512 pixels wide

    Offset:         Elements:       Data:
    0000-01ff       100 Words       Code

        fedcba98--------    Priority?
        --------76543210    Line Number

    0200-03ff       100 Words       Horizontal Scroll
    0400-05ff       100 Words       Color
    0600-07ff       100 Words       ??

    We draw each line using a bunch of 64x1 tiles

------------------------------------------------------------------------*/

void wecleman_state::wecleman_draw_road(bitmap_rgb32 &bitmap, const rectangle &cliprect, int priority)
{
// must be powers of 2
#define XSIZE 512
#define YSIZE 256

#define YMASK (YSIZE-1)

#define DST_WIDTH 320
#define DST_HEIGHT 224

#define MIDCURB_DY 5
#define TOPCURB_DY 7

	static const pen_t road_color[48] =
	{
		0x3f1,0x3f3,0x3f5,0x3fd,0x3fd,0x3fb,0x3fd,0x7ff,    // road color 0
		0x3f0,0x3f2,0x3f4,0x3fc,0x3fc,0x3fb,0x3fc,0x7fe,    // road color 1
			0,    0,    0,0x3f9,0x3f9,    0,    0,    0,    // midcurb color 0
			0,    0,    0,0x3f8,0x3f8,    0,    0,    0,    // midcurb color 1
			0,    0,    0,0x3f7,    0,    0,    0,    0,    // topcurb color 0
			0,    0,    0,0x3f6,    0,    0,    0,    0     // topcutb color 1
	};


	const uint8_t *src_ptr;
	const pen_t *pal_ptr, *rgb_ptr;

	int scrollx, sy, sx;
	int mdy, tdy, i;

	rgb_ptr = m_palette->pens();

	if (priority == 0x02)
	{
		// draw sky; each scanline is assumed to be dword aligned
		for (sy=cliprect.min_y-BMP_PAD; sy<DST_HEIGHT; sy++)
		{
			uint32_t *dst = &bitmap.pix32(sy+BMP_PAD, BMP_PAD);
			uint32_t pix;
			uint16_t road;

			road = m_roadram[sy];
			if ((road>>8) != 0x02) continue;

			pix = rgb_ptr[(m_roadram[sy+(YSIZE*2)] & 0xf) + 0x7f0];

			for (sx = 0; sx < DST_WIDTH; sx++)
				dst[sx] = pix;
		}
	}
	else if (priority == 0x04)
	{
		// draw road
		pen_t road_rgb[48];

		for (i=0; i<48; i++)
		{
			int color = road_color[i];
			road_rgb[i] = color ? rgb_ptr[color] : 0xffffffff;
		}

		for (sy=cliprect.min_y-BMP_PAD; sy<DST_HEIGHT; sy++)
		{
			uint32_t *dst = &bitmap.pix32(sy+BMP_PAD, BMP_PAD);
			uint32_t pix;
			uint16_t road;

			road = m_roadram[sy];
			if ((road>>8) != 0x04) continue;
			road &= YMASK;

			src_ptr = m_gfxdecode->gfx(1)->get_data((road << 3));
			m_gfxdecode->gfx(1)->get_data((road << 3) + 1);
			m_gfxdecode->gfx(1)->get_data((road << 3) + 2);
			m_gfxdecode->gfx(1)->get_data((road << 3) + 3);
			m_gfxdecode->gfx(1)->get_data((road << 3) + 4);
			m_gfxdecode->gfx(1)->get_data((road << 3) + 5);
			m_gfxdecode->gfx(1)->get_data((road << 3) + 6);
			m_gfxdecode->gfx(1)->get_data((road << 3) + 7);
			mdy = ((road * MIDCURB_DY) >> 8) * bitmap.rowpixels();
			tdy = ((road * TOPCURB_DY) >> 8) * bitmap.rowpixels();

			scrollx = m_roadram[sy+YSIZE] + (0x18 - 0xe00);

			pal_ptr = road_rgb + ((m_roadram[sy+(YSIZE*2)]<<3) & 8);

			for (sx = 0; sx < DST_WIDTH; sx++, scrollx++)
			{
				if (scrollx >= 0 && scrollx < XSIZE)
				{
					pen_t temp;

					pix = src_ptr[scrollx];
					dst[sx] = pal_ptr[pix];

					temp = pal_ptr[pix + 16];
					if (temp != 0xffffffff) dst[sx - mdy] = temp;

					temp = pal_ptr[pix + 32];
					if (temp != 0xffffffff) dst[sx - tdy] = temp;
				}
				else
					dst[sx] = pal_ptr[7];
			}
		}
	}

#undef YSIZE
#undef XSIZE
}

/*------------------------------------------------------------------------
                                Sky Drawing
------------------------------------------------------------------------*/

// blends two 8x8x16bpp direct RGB tilemaps
void wecleman_state::draw_cloud(bitmap_rgb32 &bitmap,
					gfx_element *gfx,
					uint16_t *tm_base,
					int x0, int y0,             // target coordinate
					int xcount, int ycount,     // number of tiles to draw in x and y
					int scrollx, int scrolly,       // tilemap scroll position
					int tmw_l2, int tmh_l2,     // tilemap width and height in log(2)
					int alpha, int pal_offset ) // alpha(0-3f), # of color codes to shift
{
	const uint8_t *src_ptr;
	uint16_t *tmap_ptr;
	uint32_t *dst_base, *dst_ptr;
	const pen_t *pal_base, *pal_ptr;

	int tilew, tileh;
	int tmskipx, tmskipy, tmscanx, tmmaskx, tmmasky;
	int dx, dy;
	int i, j, tx, ty;

	if (alpha > 0x1f) return;

	tilew = gfx->width();
	tileh = gfx->height();

	tmmaskx = (1<<tmw_l2) - 1;
	tmmasky = (1<<tmh_l2) - 1;

	scrollx &= ((tilew<<tmw_l2) - 1);
	scrolly &= ((tileh<<tmh_l2) - 1);

	tmskipx = scrollx / tilew;
	dx = -(scrollx & (tilew-1));
	tmskipy = scrolly / tileh;
	dy = -(scrolly & (tileh-1));

	dst_base = &bitmap.pix32(y0+dy, x0+dx);

	pal_base = m_palette->pens() + pal_offset * gfx->granularity();

	alpha <<= 6;

	dst_base += 8;
	for (i = 0; i < ycount; i++)
	{
		tmap_ptr = tm_base + ((tmskipy++ & tmmasky)<<tmw_l2);
		tmscanx = tmskipx;

		for (j = 0; j < xcount; j++)
		{
			uint16_t tiledata = tmap_ptr[tmscanx++ & tmmaskx];

			// Wec Le Mans specific: decodes tile index in EBX
			uint16_t tile_index = tiledata & 0xfff;

			// Wec Le Mans specific: decodes tile color in EAX
			uint16_t tile_color = ((tiledata >> 5) & 0x78) + (tiledata >> 12);

			src_ptr = gfx->get_data(tile_index);
			pal_ptr = pal_base + tile_color * gfx->granularity();
			dst_ptr = dst_base + j * tilew;

			/* alpha case */
			if (alpha > 0)
			{
				for (ty = 0; ty < tileh; ty++)
				{
					for (tx = 0; tx < tilew; tx++)
					{
						uint8_t srcpix = *src_ptr++;
						pen_t srcrgb = pal_ptr[srcpix];
						uint32_t dstrgb = dst_ptr[tx];
						int sr, sg, sb, dr, dg, db;

						sr = (srcrgb >> 3) & 0x1f;
						sg = (srcrgb >> 11) & 0x1f;
						sb = (srcrgb >> 19) & 0x1f;

						dr = (dstrgb >> 3) & 0x1f;
						dg = (dstrgb >> 11) & 0x1f;
						db = (dstrgb >> 19) & 0x1f;

						dr = (m_t32x32pm[dr - sr + alpha] >> 5) + dr;
						dg = (m_t32x32pm[dg - sg + alpha] >> 5) + dg;
						db = (m_t32x32pm[db - sb + alpha] >> 5) + db;

						dst_ptr[tx] = rgb_t(pal5bit(db), pal5bit(dg), pal5bit(dr));
					}
					dst_ptr += bitmap.rowpixels();
				}
			}

			/* non-alpha case */
			else
			{
				for (ty = 0; ty < tileh; ty++)
				{
					for (tx = 0; tx < tilew; tx++)
						dst_ptr[tx] = pal_ptr[*src_ptr++];
					dst_ptr += bitmap.rowpixels();
				}
			}
		}

		dst_base += bitmap.rowpixels() * tileh;
	}
}

/***************************************************************************
                                Hot Chase
***************************************************************************/

/*------------------------------------------------------------------------
                                Road Drawing

    This layer is composed of horizontal lines gfx elements
    There are 512 lines in ROM, each is 512 pixels wide

    Offset:         Elements:       Data:
    0000-03ff       00-FF           Code (4 bytes)

    Code:
        00.w
            fedc ba98 ---- ----     Unused?
            ---- ---- 7654 ----     color
            ---- ---- ---- 3210     scroll x
        02.w
            fedc ba-- ---- ----     scroll x
            ---- --9- ---- ----     ?
            ---- ---8 7654 3210     code

    We draw each line using a bunch of 64x1 tiles

------------------------------------------------------------------------*/

void wecleman_state::hotchase_draw_road(bitmap_ind16 &bitmap, const rectangle &cliprect)
{
/* Referred to what's in the ROMs */
#define XSIZE 512
#define YSIZE 512

	int sx, sy;
	const rectangle &visarea = m_screen->visible_area();

	/* Let's draw from the top to the bottom of the visible screen */
	for (sy = visarea.min_y;sy <= visarea.max_y;sy++)
	{
		int code    = m_roadram[sy*4/2+2/2] + (m_roadram[sy*4/2+0/2] << 16);
		int color   = ((code & 0x00f00000) >> 20) + 0x70;
		int scrollx = ((code & 0x0007fc00) >> 10) * 2;

		/* convert line number in gfx element number: */
		/* code is the tile code of the start of this line */
		code &= 0x1ff;
		code *= XSIZE / 32;

		for (sx=0; sx<2*XSIZE; sx+=64)
		{
			m_gfxdecode->gfx(0)->transpen(bitmap,cliprect,
					code++,
					color,
					0,0,
					((sx-scrollx)&0x3ff)-(384-32),sy,0);
		}
	}

#undef XSIZE
#undef YSIZE
}


/***************************************************************************
                            Palette Routines
***************************************************************************/

// new video and palette code
// TODO: remove me.
WRITE16_MEMBER(wecleman_state::wecleman_videostatus_w)
{
	COMBINE_DATA(m_videostatus);

	// bit0-6: background transition, 0=off, 1=on
	// bit7: palette being changed, 0=no, 1=yes
	if (ACCESSING_BITS_0_7)
	{
		if ((data & 0x7f) == 0 && !m_cloud_ds)
			m_cloud_ds = BLEND_INC;
		else
		if ((data & 0x7f) == 1 && !m_cloud_visible)
		{
			data ^= 1;
			m_cloud_ds = BLEND_DEC;
			m_cloud_visible = 1;
		}
	}
}

WRITE16_MEMBER(wecleman_state::hotchase_paletteram16_SBGRBBBBGGGGRRRR_word_w)
{
	int newword, r, g, b;

	newword = COMBINE_DATA(&m_generic_paletteram_16[offset]);

	r = ((newword << 1) & 0x1E ) | ((newword >> 12) & 0x01);
	g = ((newword >> 3) & 0x1E ) | ((newword >> 13) & 0x01);
	b = ((newword >> 7) & 0x1E ) | ((newword >> 14) & 0x01);

	m_palette->set_pen_color(offset, pal5bit(r), pal5bit(g), pal5bit(b));
	r>>=1; g>>=1; b>>=1;
	m_palette->set_pen_color(offset+0x800, pal5bit(r)/2, pal5bit(g)/2, pal5bit(b)/2);
}

WRITE16_MEMBER(wecleman_state::wecleman_paletteram16_SSSSBBBBGGGGRRRR_word_w)
{
	int newword = COMBINE_DATA(&m_generic_paletteram_16[offset]);

	// the highest nibble has some unknown functions
//  if (newword & 0xf000) logerror("MSN set on color %03x: %1x\n", offset, newword>>12);
	m_palette->set_pen_color(offset, pal4bit(newword >> 0), pal4bit(newword >> 4), pal4bit(newword >> 8));
}


/***************************************************************************
                            Initializations
***************************************************************************/

VIDEO_START_MEMBER(wecleman_state,wecleman)
{
	/*
	    Sprite banking - each bank is 0x20000 bytes (we support 0x40 bank codes)
	    This game has ROMs for 16 banks
	*/
	static const int bank[0x40] =
	{
		0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,
		8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,
		0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,
		8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15
	};

	uint8_t *buffer;
	int i, j;

	assert(m_screen->format() == BITMAP_FORMAT_RGB32);
	buffer = auto_alloc_array(machine(), uint8_t, 0x12c00);   // working buffer for sprite operations

	m_gameid = 0;
	m_gfx_bank = bank;
	m_spr_offsx = -0xbc + BMP_PAD;
	m_spr_offsy = 1 + BMP_PAD;
	m_cloud_blend = BLEND_MAX;
	m_cloud_ds = 0;
	m_cloud_visible = 0;
	m_black_pen = m_palette->black_pen();

	m_rgb_half     =          (uint16_t*)(buffer + 0x00000);
	m_t32x32pm     =             (int*)(buffer + 0x10020);
	m_spr_ptr_list = (struct sprite **)(buffer + 0x12000);
	m_spr_idx_list =            (int *)(buffer + 0x12400);
	m_spr_pri_list =            (int *)(buffer + 0x12800);

	for (i=0; i<0x8000; i++)
	{
		j = i>>1;
		m_rgb_half[i] = (j&0xf) | (j&0x1e0) | (j&0x3c00);
	}

	for (j=0; j<0x20; j++)
	{
		for (i=-0x1f; i<0x20; i++)
		{
			*(m_t32x32pm + (j<<6) + i) = i * j;
		}
	}

	m_sprite_list = auto_alloc_array_clear(machine(), struct sprite, NUM_SPRITES);

	m_bg_tilemap = &machine().tilemap().create(*m_gfxdecode, tilemap_get_info_delegate(FUNC(wecleman_state::wecleman_get_bg_tile_info),this),
								TILEMAP_SCAN_ROWS,
									/* We draw part of the road below */
								8,8,
								PAGE_NX * 2, PAGE_NY * 2 );

	m_fg_tilemap = &machine().tilemap().create(*m_gfxdecode, tilemap_get_info_delegate(FUNC(wecleman_state::wecleman_get_fg_tile_info),this),
								TILEMAP_SCAN_ROWS,

								8,8,
								PAGE_NX * 2, PAGE_NY * 2);

	m_txt_tilemap = &machine().tilemap().create(*m_gfxdecode, tilemap_get_info_delegate(FUNC(wecleman_state::wecleman_get_txt_tile_info),this),
									TILEMAP_SCAN_ROWS,

									8,8,
									PAGE_NX * 1, PAGE_NY * 1);

	m_bg_tilemap->set_scroll_rows(TILEMAP_DIMY);    /* Screen-wise scrolling */
	m_bg_tilemap->set_scroll_cols(1);
	m_bg_tilemap->set_transparent_pen(0);

	m_fg_tilemap->set_scroll_rows(TILEMAP_DIMY);    /* Screen-wise scrolling */
	m_fg_tilemap->set_scroll_cols(1);
	m_fg_tilemap->set_transparent_pen(0);

	m_txt_tilemap->set_scroll_rows(1);
	m_txt_tilemap->set_scroll_cols(1);
	m_txt_tilemap->set_transparent_pen(0);

	m_txt_tilemap->set_scrollx(0, 512-320-16 -BMP_PAD);
	m_txt_tilemap->set_scrolly(0, -BMP_PAD );

	// patches out a mysterious pixel floating in the sky (tile decoding bug?)
	*const_cast<uint8_t *>(m_gfxdecode->gfx(0)->get_data(0xaca)+7) = 0;
}

VIDEO_START_MEMBER(wecleman_state,hotchase)
{
	/*
	    Sprite banking - each bank is 0x20000 bytes (we support 0x40 bank codes)
	    This game has ROMs for 0x30 banks
	*/
	static const int bank[0x40] =
	{
		0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
		16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
		32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
		0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
	};

	uint8_t *buffer;

	buffer = auto_alloc_array(machine(), uint8_t, 0x400); // reserve 1k for sprite list

	m_gameid = 1;
	m_gfx_bank = bank;
	m_spr_offsx = -0xc0;
	m_spr_offsy = 0;
	m_black_pen = m_palette->black_pen();

	m_spr_ptr_list = (struct sprite **)buffer;

	m_sprite_list = auto_alloc_array_clear(machine(), struct sprite, NUM_SPRITES);
}


/***************************************************************************
                            Video Updates
***************************************************************************/

uint32_t wecleman_state::screen_update_wecleman(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	const pen_t *mrct;
	int video_on;
	int fg_x, bg_x, fg_y, bg_y;
	int cloud_sx, cloud_sy;
	int i, j, k;

	mrct = m_palette->pens();

	video_on = m_irqctrl & 0x40;

	output().set_led_value(0, m_selected_ip & 0x04); // Start lamp

	fg_y = (m_txtram[0x0f24>>1] & (TILEMAP_DIMY - 1));
	bg_y = (m_txtram[0x0f26>>1] & (TILEMAP_DIMY - 1));

	cloud_sx = m_txtram[0xfee>>1] + 0xb0;
	cloud_sy = bg_y;

	m_bg_tilemap->set_scrolly(0, bg_y -BMP_PAD);
	m_fg_tilemap->set_scrolly(0, fg_y -BMP_PAD);

	for (i=0; i<(28<<2); i+=4)
	{
		fg_x = m_txtram[(i+0xf80)>>1] + (0xb0 -BMP_PAD);
		bg_x = m_txtram[(i+0xf82)>>1] + (0xb0 -BMP_PAD);

		k = i<<1;
		for (j=0; j<8; j++)
		{
			m_fg_tilemap->set_scrollx((fg_y + k + j) & (TILEMAP_DIMY - 1), fg_x);
			m_bg_tilemap->set_scrollx((bg_y + k + j) & (TILEMAP_DIMY - 1), bg_x);
		}
	}

	// temporary fix for ranking screen tile masking
	/* palette hacks! */
	((pen_t *)mrct)[0x27] = mrct[0x24];

	get_sprite_info();

	bitmap.fill(m_black_pen, cliprect);

	/* Draw the road (lines which have priority 0x02) */
	if (video_on) wecleman_draw_road(bitmap, cliprect, 0x02);

	/* Draw the background */
	if (video_on) m_bg_tilemap->draw(screen, bitmap, cliprect, 0, 0);

	// draws the cloud layer; needs work
	if (m_cloud_visible)
	{
		/* palette hacks! */
		((pen_t *)mrct)[0] = ((pen_t *)mrct)[0x40] = ((pen_t *)mrct)[0x200] = ((pen_t *)mrct)[0x205];

		if (video_on)
			draw_cloud(bitmap,
			m_gfxdecode->gfx(0),
			m_pageram+0x1800,
			BMP_PAD, BMP_PAD,
			41, 20,
			cloud_sx, cloud_sy,
			6, 5,
			m_cloud_blend/BLEND_STEPS, 0);

		m_cloud_blend += m_cloud_ds;

		if (m_cloud_blend < BLEND_MIN)
			{ m_cloud_blend = BLEND_MIN; m_cloud_ds = 0; *m_videostatus |= 1; }
		else if (m_cloud_blend > BLEND_MAX)
			{ m_cloud_blend = BLEND_MAX; m_cloud_ds = 0; m_cloud_visible = 0; }
	}

	/* Draw the foreground */
	if (video_on) m_fg_tilemap->draw(screen, bitmap, cliprect, 0, 0);

	/* Draw the road (lines which have priority 0x04) */
	if (video_on) wecleman_draw_road(bitmap,cliprect, 0x04);

	/* Draw the sprites */
	if (video_on) sprite_draw(bitmap,cliprect);

	/* Draw the text layer */
	if (video_on) m_txt_tilemap->draw(screen, bitmap, cliprect, 0, 0);
	return 0;
}

/***************************************************************************
                                Hot Chase
***************************************************************************/

uint32_t wecleman_state::screen_update_hotchase(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	int video_on;

	video_on = m_irqctrl & 0x40;

	output().set_led_value(0, m_selected_ip & 0x04); // Start lamp

	get_sprite_info();

	bitmap.fill(m_black_pen, cliprect);

	/* Draw the background */
#if 0
	if (video_on)
		m_k051316_1->zoom_draw(screen, bitmap, cliprect, 0, 0);
#endif

	/* Draw the road */
	if (video_on)
		hotchase_draw_road(bitmap, cliprect);

	/* Draw the sprites */
	if (video_on)
		sprite_draw(bitmap,cliprect);

	/* Draw the foreground (text) */
#if 0
	if (video_on)
		m_k051316_2->zoom_draw(screen, bitmap, cliprect, 0, 0);
#endif
	return 0;
}


/***************************************************************************
                            Common Routines
***************************************************************************/

READ16_MEMBER(wecleman_state::wecleman_protection_r)
{
	int blend, data0, data1, r0, g0, b0, r1, g1, b1;

	data0 = m_protection_ram[0];
	blend = m_protection_ram[2];
	data1 = m_protection_ram[1];
	blend &= 0x3ff;

	// a precalculated table will take an astronomical 4096^2(colors) x 1024(steps) x 2(word) bytes
	r0 = data0;  g0 = data0;  b0 = data0;
	r0 &= 0xf;   g0 &= 0xf0;  b0 &= 0xf00;
	r1 = data1;  g1 = data1;  b1 = data1;
	r1 &= 0xf;   g1 &= 0xf0;  b1 &= 0xf00;
	r1 -= r0;    g1 -= g0;    b1 -= b0;
	r1 *= blend; g1 *= blend; b1 *= blend;
	r1 >>= 10;   g1 >>= 10;   b1 >>= 10;
	r0 += r1;    g0 += g1;    b0 += b1;
	g0 &= 0xf0;  b0 &= 0xf00;

	r0 |= g0;
	r0 |= b0;

	return(r0);
}

WRITE16_MEMBER(wecleman_state::wecleman_protection_w)
{
	if (offset == 2) m_prot_state = data & 0x2000;
	if (!m_prot_state) COMBINE_DATA(m_protection_ram + offset);
}



/* 140005.b (WEC Le Mans 24 Schematics)

 COMMAND
 ___|____
|   CK  8|--/        7
| LS273 7| TV-KILL   6
|       6| SCR-VCNT  5
|       5| SCR-HCNT  4
|   5H  4| SOUND-RST 3
|       3| SOUND-ON  2
|       2| NSUBRST   1
|       1| SUBINT    0
|__CLR___|
    |
  NEXRES

 Schems: SUBRESET does a RST+HALT
         Sub CPU IRQ 4 generated by SUBINT, no other IRQs
*/
WRITE16_MEMBER(wecleman_state::irqctrl_w)
{
	if (ACCESSING_BITS_0_7)
	{
		// logerror("CPU #0 - PC = %06X - $140005 <- %02X (old value: %02X)\n",space.device().safe_pc(), data&0xFF, old_data&0xFF);

		// Bit 0 : SUBINT
		if ( (m_irqctrl & 1) && (!(data & 1)) ) // 1->0 transition
			m_subcpu->set_input_line(4, HOLD_LINE);

		// Bit 1 : NSUBRST
		m_subcpu->set_input_line(INPUT_LINE_RESET,  (data & 2) ? CLEAR_LINE : ASSERT_LINE);

		// Bit 2 : SOUND-ON: send a interrupt to sound CPU, 0 -> 1 transition
		if ( (m_irqctrl & 4) && (!(data & 4)) )
		{
			if(m_sound_hw_type == 0) // wec le mans
				m_audiocpu->set_input_line(0, HOLD_LINE);
			else // hot chase
			{
				m_hotchase_sound_hs = false;
				// TODO: ASSERT_LINE here?
				m_audiocpu->set_input_line(M6809_IRQ_LINE, HOLD_LINE);
			}
		}
		// Bit 3 : SOUNDRST, pc=0x18ea in Hot Chase POST, 1 -> 0 -> 1
		m_audiocpu->set_input_line(INPUT_LINE_RESET, (data & 8) ? CLEAR_LINE : ASSERT_LINE);
		// Bit 4 : SCR-HCNT
		// Bit 5 : SCR-VCNT: active in WEC Le Mans, disabled in Hot Chase (where's the latch anyway?)
		// Bit 6 : TV-KILL: active low, disables screen.
		m_irqctrl = data;   // latch the value
	}
}

/* 140003.b (usually paired with a write to 140021.b)

    Bit:

    7-------        ?
    -65-----        input selection (0-3)
    ---43---        ?
    -----2--        start light
    ------10        ? out 1/2

*/
WRITE16_MEMBER(wecleman_state::selected_ip_w)
{
	if (ACCESSING_BITS_0_7) m_selected_ip = data & 0xff;    // latch the value
}

/* $140021.b - Return the previously selected input port's value */
READ16_MEMBER(wecleman_state::selected_ip_r)
{
	switch ( (m_selected_ip >> 5) & 3 )
	{                                                                   // From WEC Le Mans Schems:
		case 0:  return ioport("ACCEL")->read();        // Accel - Schems: Accelevr
		case 1:  return ~0;                                             // ????? - Schems: Not Used
		case 2:  return ioport("STEER")->read();        // Wheel - Schems: Handlevr
		case 3:  return ~0;                                             // Table - Schems: Turnvr

		default: return ~0;
	}
}

/* Word Blitter - Copies data around (Work RAM, Sprite RAM etc.)
                  It's fed with a list of blits to do

    Offset:

    00.b            ? Number of words - 1 to add to address per transfer
    01.b            ? logic function / blit mode
    02.w            ? (always 0)
    04.l            Source address (Base address of source data)
    08.l            List of blits address
    0c.l            Destination address
    01.b            ? Number of transfers
    10.b            Triggers the blit
    11.b            Number of words per transfer

    The list contains 4 bytes per blit:


    Offset:

    00.w            ?
    02.w            offset from Base address


    Note:

    Hot Chase explicitly copies color information from sprite parameters back to list[4n+1](byte ptr)
    and that tips me off where the colors are actually encoded. List[4n+0] is believed to hold the
    sprites' depth value. Wec Le Mans will z-sort the sprites before writing them to video RAM but
    the order is not always right. It is possible the video hardware performs additional sorting.

    The color code in the original sprite encoding has special meanings on the other hand. I'll take
    a shortcut by manually copying list[0] and list[1] to sprite RAM for further process.
*/
WRITE16_MEMBER(wecleman_state::blitter_w)
{
	COMBINE_DATA(&m_blitter_regs[offset]);

	/* do a blit if $80010.b has been written */
	if ( (offset == 0x10/2) && (ACCESSING_BITS_8_15) )
	{
		/* 80000.b = ?? usually 0 - other values: 02 ; 00 - ? logic function ? */
		/* 80001.b = ?? usually 0 - other values: 3f ; 01 - ? height ? */
		int minterm  = ( m_blitter_regs[0x0/2] & 0xFF00 ) >> 8;
		int list_len = ( m_blitter_regs[0x0/2] & 0x00FF ) >> 0;

		/* 80002.w = ?? always 0 - ? increment per horizontal line ? */
		/* no proof at all, it's always 0 */
		//int srcdisp = m_blitter_regs[0x2/2] & 0xFF00;
		//int destdisp = m_blitter_regs[0x2/2] & 0x00FF;

		/* 80004.l = source data address */
		int src  = ( m_blitter_regs[0x4/2] << 16 ) + m_blitter_regs[0x6/2];

		/* 80008.l = list of blits address */
		int list = ( m_blitter_regs[0x8/2] << 16 ) + m_blitter_regs[0xA/2];

		/* 8000C.l = destination address */
		int dest = ( m_blitter_regs[0xC/2] << 16 ) + m_blitter_regs[0xE/2];

		/* 80010.b = number of words to move */
		int size = ( m_blitter_regs[0x10/2] ) & 0x00FF;

		/* Word aligned transfers only ?? */
		src  &= (~1);   list &= (~1);    dest &= (~1);

		/* Two minterms / blit modes are used */
		if (minterm != 2)
		{
			/* One single blit */
			for ( ; size > 0 ; size--)
			{
				/* maybe slower than a memcpy but safer (and errors are logged) */
				space.write_word(dest, space.read_word(src));
				src += 2;
				dest += 2;
			}
		}
		else
		{
			/* Number of blits in the list */
			for ( ; list_len > 0 ; list_len-- )
			{
				int i, j, destptr;

				/* Read offset of source from the list of blits */
				i = src + space.read_word(list+2);
				j = i + (size<<1);
				destptr = dest;

				for (; i<j; destptr+=2, i+=2)
					space.write_word(destptr, space.read_word(i));

				destptr = dest + 14;
				i = space.read_word(list) + m_spr_color_offs;
				space.write_word(destptr, i);

				dest += 16;
				list += 4;
			}

			/* hack for the blit to Sprites RAM - Sprite list end-marker */
			space.write_word(dest, 0xFFFF);
		}
	}
}


/***************************************************************************
                    WEC Le Mans 24 Main CPU Handlers
***************************************************************************/

static ADDRESS_MAP_START( wecleman_map, AS_PROGRAM, 16, wecleman_state )
	AM_RANGE(0x000000, 0x03ffff) AM_ROM // ROM (03c000-03ffff used as RAM sometimes!)
	AM_RANGE(0x040494, 0x040495) AM_WRITE(wecleman_videostatus_w) AM_SHARE("videostatus")   // cloud blending control (HACK)
	AM_RANGE(0x040000, 0x043fff) AM_RAM // RAM
	AM_RANGE(0x060000, 0x060005) AM_WRITE(wecleman_protection_w) AM_SHARE("protection_ram")
	AM_RANGE(0x060006, 0x060007) AM_READ(wecleman_protection_r) // MCU read
	AM_RANGE(0x080000, 0x080011) AM_RAM_WRITE(blitter_w) AM_SHARE("blitter_regs")   // Blitter
	AM_RANGE(0x100000, 0x103fff) AM_RAM_WRITE(wecleman_pageram_w) AM_SHARE("pageram")   // Background Layers
	AM_RANGE(0x108000, 0x108fff) AM_RAM_WRITE(wecleman_txtram_w) AM_SHARE("txtram") // Text Layer
	AM_RANGE(0x110000, 0x110fff) AM_RAM_WRITE(wecleman_paletteram16_SSSSBBBBGGGGRRRR_word_w) AM_SHARE("paletteram")
	AM_RANGE(0x124000, 0x127fff) AM_RAM AM_SHARE("share1")  // Shared with main CPU
	AM_RANGE(0x130000, 0x130fff) AM_RAM AM_SHARE("spriteram")   // Sprites
	AM_RANGE(0x140000, 0x140001) AM_WRITE(wecleman_soundlatch_w)    // To sound CPU
	AM_RANGE(0x140002, 0x140003) AM_WRITE(selected_ip_w)    // Selects accelerator / wheel / ..
	AM_RANGE(0x140004, 0x140005) AM_WRITE(irqctrl_w)    // Main CPU controls the other CPUs
	AM_RANGE(0x140006, 0x140007) AM_WRITENOP    // Watchdog reset
	AM_RANGE(0x140010, 0x140011) AM_READ_PORT("IN0")    // Coins + brake + gear
	AM_RANGE(0x140012, 0x140013) AM_READ_PORT("IN1")    // ??
	AM_RANGE(0x140014, 0x140015) AM_READ_PORT("DSWA")   // DSW 2
	AM_RANGE(0x140016, 0x140017) AM_READ_PORT("DSWB")   // DSW 1
	AM_RANGE(0x140020, 0x140021) AM_WRITEONLY   // Paired with writes to $140003
	AM_RANGE(0x140020, 0x140021) AM_READ(selected_ip_r) // Accelerator or Wheel or ..
	AM_RANGE(0x140030, 0x140031) AM_WRITENOP    // toggles between 0 & 1 on hitting bumps and crashes (vibration?)
ADDRESS_MAP_END


/***************************************************************************
                        Hot Chase Main CPU Handlers
***************************************************************************/



static ADDRESS_MAP_START( hotchase_map, AS_PROGRAM, 16, wecleman_state )
	AM_RANGE(0x000000, 0x03ffff) AM_ROM
	AM_RANGE(0x040000, 0x041fff) AM_RAM                                 // RAM
	AM_RANGE(0x060000, 0x063fff) AM_RAM                                 // RAM
	AM_RANGE(0x080000, 0x080011) AM_RAM_WRITE(blitter_w) AM_SHARE("blitter_regs")   // Blitter
	AM_RANGE(0x100000, 0x100fff) AM_DEVREADWRITE8("roz_1", k051316_device, vram_r, vram_w, 0x00ff) // Background
	AM_RANGE(0x101000, 0x10101f) AM_DEVICE8("roz_1", k051316_device, map, 0x00ff)   // Background Ctrl
	AM_RANGE(0x102000, 0x102fff) AM_DEVREADWRITE8("roz_2", k051316_device, vram_r, vram_w, 0x00ff) // Foreground
	AM_RANGE(0x103000, 0x10301f) AM_DEVICE8("roz_2", k051316_device, map, 0x00ff)   // Foreground Ctrl
	AM_RANGE(0x110000, 0x111fff) AM_RAM_WRITE(hotchase_paletteram16_SBGRBBBBGGGGRRRR_word_w) AM_SHARE("paletteram")
	AM_RANGE(0x120000, 0x123fff) AM_RAM AM_SHARE("share1")                  // Shared with sub CPU
	AM_RANGE(0x130000, 0x130fff) AM_RAM AM_SHARE("spriteram")   // Sprites
	AM_RANGE(0x140000, 0x140001) AM_WRITE(hotchase_soundlatch_w)    // To sound CPU
	AM_RANGE(0x140002, 0x140003) AM_WRITE(selected_ip_w)    // Selects accelerator / wheel /
	AM_RANGE(0x140004, 0x140005) AM_WRITE(irqctrl_w)    // Main CPU controls the other CPUs
	AM_RANGE(0x140006, 0x140007) AM_READNOP // Watchdog reset
	AM_RANGE(0x140010, 0x140011) AM_READ_PORT("IN0")    // Coins + brake + gear
	AM_RANGE(0x140012, 0x140013) AM_READ_PORT("IN1")    // ?? bit 4 from sound cpu
	AM_RANGE(0x140014, 0x140015) AM_READ_PORT("DSW2")   // DSW 2
	AM_RANGE(0x140016, 0x140017) AM_READ_PORT("DSW1")   // DSW 1
	AM_RANGE(0x140020, 0x140021) AM_READ(selected_ip_r) AM_WRITENOP // Paired with writes to $140003
	AM_RANGE(0x140022, 0x140023) AM_READNOP // read and written at $601c0, unknown purpose
	AM_RANGE(0x140030, 0x140031) AM_WRITENOP    // signal to cabinet vibration motors?
ADDRESS_MAP_END


/***************************************************************************
                    WEC Le Mans 24 Sub CPU Handlers
***************************************************************************/

static ADDRESS_MAP_START( wecleman_sub_map, AS_PROGRAM, 16, wecleman_state )
	AM_RANGE(0x000000, 0x00ffff) AM_ROM // ROM
	AM_RANGE(0x060000, 0x060fff) AM_RAM AM_SHARE("roadram") // Road
	AM_RANGE(0x070000, 0x073fff) AM_RAM AM_SHARE("share1")  // RAM (Shared with main CPU)
ADDRESS_MAP_END


/***************************************************************************
                        Hot Chase Sub CPU Handlers
***************************************************************************/

static ADDRESS_MAP_START( hotchase_sub_map, AS_PROGRAM, 16, wecleman_state )
	AM_RANGE(0x000000, 0x01ffff) AM_ROM // ROM
	AM_RANGE(0x020000, 0x020fff) AM_RAM AM_SHARE("roadram") // Road
	AM_RANGE(0x040000, 0x043fff) AM_RAM AM_SHARE("share1") // Shared with main CPU
	AM_RANGE(0x060000, 0x060fff) AM_RAM // a table, presumably road related
	AM_RANGE(0x061000, 0x06101f) AM_RAM // road vregs?
ADDRESS_MAP_END


/***************************************************************************
                    WEC Le Mans 24 Sound CPU Handlers
***************************************************************************/

/* 140001.b */
WRITE16_MEMBER(wecleman_state::wecleman_soundlatch_w)
{
	if (ACCESSING_BITS_0_7)
	{
		m_soundlatch->write(space, 0, data & 0xFF);
	}
}

/* Protection - an external multiplier connected to the sound CPU */
READ8_MEMBER(wecleman_state::multiply_r)
{
	return (m_multiply_reg[0] * m_multiply_reg[1]) & 0xFF;
}

WRITE8_MEMBER(wecleman_state::multiply_w)
{
	m_multiply_reg[offset] = data;
}

/*      K007232 registers reminder:

[Ch A]  [Ch B]  [Meaning]
00      06      address step    (low  byte)
01      07      address step    (high byte, max 1)
02      08      sample address  (low  byte)
03      09      sample address  (mid  byte)
04      0a      sample address  (high byte, max 1 -> max rom size: $20000)
05      0b      Reading this byte triggers the sample

[Ch A & B]
0c              volume
0d              play sample once or looped (2 channels -> 2 bits (0&1))

** sample playing ends when a byte with bit 7 set is reached **/

WRITE8_MEMBER(wecleman_state::wecleman_volume_callback)
{
	m_k007232->set_volume(0, (data >> 4) * 0x11, 0);
	m_k007232->set_volume(1, 0, (data & 0x0f) * 0x11);
}

WRITE8_MEMBER(wecleman_state::wecleman_K00723216_bank_w)
{
	m_k007232->set_bank(0, ~data&1 );  //* (wecleman062gre)
}

static ADDRESS_MAP_START( wecleman_sound_map, AS_PROGRAM, 8, wecleman_state )
	AM_RANGE(0x0000, 0x7fff) AM_ROM
	AM_RANGE(0x8000, 0x83ff) AM_RAM
	AM_RANGE(0x8500, 0x8500) AM_WRITENOP            // increased with speed (global volume)?
	AM_RANGE(0x9000, 0x9000) AM_READ(multiply_r)    // 007452: Protection
	AM_RANGE(0x9000, 0x9001) AM_WRITE(multiply_w)   // 007452: Protection
	AM_RANGE(0x9006, 0x9006) AM_WRITENOP            // 007452: ?
	AM_RANGE(0xa000, 0xa000) AM_DEVREAD("soundlatch", generic_latch_8_device, read) // From main CPU
	AM_RANGE(0xb000, 0xb00d) AM_DEVREADWRITE("k007232", k007232_device, read, write) // K007232 (Reading offset 5/b triggers the sample)
	AM_RANGE(0xc000, 0xc001) AM_DEVREADWRITE("ymsnd", ym2151_device, read, write)
	AM_RANGE(0xf000, 0xf000) AM_WRITE(wecleman_K00723216_bank_w)    // Samples banking
ADDRESS_MAP_END


/***************************************************************************
                        Hot Chase Sound CPU Handlers
***************************************************************************/

/* 140001.b */
WRITE16_MEMBER(wecleman_state::hotchase_soundlatch_w)
{
	if (ACCESSING_BITS_0_7)
	{
		m_soundlatch->write(space, 0, data & 0xFF);
	}
}

WRITE8_MEMBER(wecleman_state::hotchase_sound_control_w)
{
//  int reg[8];


//  reg[offset] = data;

	switch (offset)
	{
		/* change volume
		    offset 00000xxx----- channel select (0:channel 0, 1:channel 1)
		    ++------ chip select ( 0:chip 1, 1:chip2, 2:chip3)
		    data&0x0f left volume  (data>>4)&0x0f right volume
		    */
		case 0x0:
		case 0x1:
			m_k007232_1->set_volume( offset&1,  (data&0x0f) * 0x08, (data>>4) * 0x08 );
			break;
		case 0x2:
		case 0x3:
			m_k007232_2->set_volume( offset&1,  (data&0x0f) * 0x08, (data>>4) * 0x08 );
			break;
		case 0x4:
		case 0x5:
			m_k007232_3->set_volume( offset&1,  (data&0x0f) * 0x08, (data>>4) * 0x08 );
			break;

		case 0x06:  /* Bankswitch for chips 0 & 1 */
		{
			int bank0_a = (data >> 1) & 1;
			int bank1_a = (data >> 2) & 1;
			int bank0_b = (data >> 3) & 1;
			int bank1_b = (data >> 4) & 1;
			// bit 6: chip 2 - ch0 ?
			// bit 7: chip 2 - ch1 ?

			m_k007232_1->set_bank( bank0_a, bank0_b );
			m_k007232_2->set_bank( bank1_a, bank1_b );
		}
		break;

		case 0x07:  /* Bankswitch for chip 2 */
		{
			int bank2_a = (data >> 0) & 7;
			int bank2_b = (data >> 3) & 7;

			m_k007232_3->set_bank( bank2_a, bank2_b );
		}
		break;
	}
}

WRITE8_MEMBER(wecleman_state::hotchase_sound_hs_w)
{
	m_hotchase_sound_hs = true;
}

/* Read and write handlers for one K007232 chip:
   even and odd register are mapped swapped */
READ8_MEMBER(wecleman_state::hotchase_1_k007232_r)
{
	return m_k007232_1->read(space, offset ^ 1);
}

WRITE8_MEMBER(wecleman_state::hotchase_1_k007232_w)
{
	m_k007232_1->write(space, offset ^ 1, data);
}

READ8_MEMBER(wecleman_state::hotchase_2_k007232_r)
{
	return m_k007232_2->read(space, offset ^ 1);
}

WRITE8_MEMBER(wecleman_state::hotchase_2_k007232_w)
{
	m_k007232_2->write(space, offset ^ 1, data);
}

READ8_MEMBER(wecleman_state::hotchase_3_k007232_r)
{
	return m_k007232_3->read(space, offset ^ 1);
}

WRITE8_MEMBER(wecleman_state::hotchase_3_k007232_w)
{
	m_k007232_3->write(space, offset ^ 1, data);
}

static ADDRESS_MAP_START( hotchase_sound_map, AS_PROGRAM, 8, wecleman_state )
	AM_RANGE(0x0000, 0x07ff) AM_RAM
	AM_RANGE(0x1000, 0x100d) AM_READWRITE(hotchase_1_k007232_r, hotchase_1_k007232_w)   // 3 x K007232
	AM_RANGE(0x2000, 0x200d) AM_READWRITE(hotchase_2_k007232_r, hotchase_2_k007232_w)
	AM_RANGE(0x3000, 0x300d) AM_READWRITE(hotchase_3_k007232_r, hotchase_3_k007232_w)
	AM_RANGE(0x4000, 0x4007) AM_WRITE(hotchase_sound_control_w) // Sound volume, banking, etc.
	AM_RANGE(0x5000, 0x5000) AM_WRITENOP   // 0 at start of IRQ service, 1 at end (irq mask?)
	AM_RANGE(0x6000, 0x6000) AM_DEVREAD("soundlatch", generic_latch_8_device, read) // From main CPU (Read on IRQ)
	AM_RANGE(0x7000, 0x7000) AM_WRITE(hotchase_sound_hs_w)    // ACK signal to main CPU
	AM_RANGE(0x8000, 0xffff) AM_ROM
ADDRESS_MAP_END


/***************************************************************************
                        WEC Le Mans 24 Input Ports
***************************************************************************/

static INPUT_PORTS_START( wecleman )
	PORT_START("IN0")   /* $140011.b */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_HIGH )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_NAME("Shift") PORT_TOGGLE
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_NAME("Brake")
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN1")   /* Motor? - $140013.b */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE2 ) PORT_NAME("Right SW")  // right sw
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 ) PORT_NAME("Left SW")  // left sw
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE4 ) PORT_NAME("Thermo SW")  // thermo
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SPECIAL )   // from sound cpu ?
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("DSWA")  /* $140015.b */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, "No Coin B" )
	/* "No Coin B" = coins produce sound, but no effect on coin counter */

	PORT_START("DSWB")  /* $140017.b */
	PORT_DIPNAME( 0x01, 0x01, "Speed Unit" )
	PORT_DIPSETTING(    0x01, "km/h" )
	PORT_DIPSETTING(    0x00, "mph" )
	PORT_DIPNAME( 0x02, 0x02, "Unknown B-1" )   // single
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Unknown B-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x18, DEF_STR( Easy ) )     // 66 seconds at the start
	PORT_DIPSETTING(    0x10, DEF_STR( Normal ) )   // 64
	PORT_DIPSETTING(    0x08, DEF_STR( Hard ) )     // 62
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )  // 60
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Unknown B-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unknown B-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("ACCEL") /* Accelerator - $140021.b (0) */
	PORT_BIT( 0xff, 0, IPT_PEDAL ) PORT_MINMAX(0,0x80) PORT_SENSITIVITY(30) PORT_KEYDELTA(10)

	PORT_START("STEER") /* Steering Wheel - $140021.b (2) */
	PORT_BIT( 0xff, 0x80, IPT_PADDLE ) PORT_SENSITIVITY(50) PORT_KEYDELTA(5)
INPUT_PORTS_END


/***************************************************************************
                            Hot Chase Input Ports
***************************************************************************/

CUSTOM_INPUT_MEMBER(wecleman_state::hotchase_sound_status_r)
{
	return m_hotchase_sound_hs;
}

static INPUT_PORTS_START( hotchase )
	PORT_START("IN0")   /* $140011.b */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_NAME("Shift") PORT_TOGGLE
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_NAME("Brake")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("IN1")   /* Motor? - $140013.b */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE2 ) PORT_NAME("Right SW")   // right sw
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 ) PORT_NAME("Left SW")  // left sw
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE4 ) PORT_NAME("Thermo SW")  // thermo
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SPECIAL ) // from sound cpu
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL ) PORT_CUSTOM_MEMBER(DEVICE_SELF, wecleman_state,hotchase_sound_status_r, nullptr)
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("DSW2")  /* $140015.b */
	PORT_DIPNAME( 0x01, 0x01, "Speed Unit" )
	PORT_DIPSETTING(    0x01, "KM" )
	PORT_DIPSETTING(    0x00, "M.P.H." )
	PORT_DIPNAME( 0x02, 0x02, "Unknown 2-1" )   // single (wheel related)
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Unknown 2-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, "Unknown 2-3&4" ) // Most likely Difficulty
	PORT_DIPSETTING(    0x18, "0" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x08, "8" )
	PORT_DIPSETTING(    0x00, "c" )
	PORT_DIPNAME( 0x20, 0x20, "Unknown 2-5" )   // single
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* wheel <-> brake ; accel -> start */
	PORT_DIPNAME( 0x40, 0x40, "Unknown 2-6" )   // single (wheel<->brake)
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("DSW1")  /* $140017.b */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 5C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 5C_3C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x00, "1 Coin/99 Credits" )

	PORT_START("ACCEL") /* Accelerator - $140021.b (0) */
	PORT_BIT( 0xff, 0, IPT_PEDAL ) PORT_MINMAX(0,0x80) PORT_SENSITIVITY(30) PORT_KEYDELTA(10)

	PORT_START("STEER") /* Steering Wheel - $140021.b (2) */
	PORT_BIT( 0xff, 0x80, IPT_PADDLE ) PORT_SENSITIVITY(50) PORT_KEYDELTA(5)
INPUT_PORTS_END


/***************************************************************************
                            WEC Le Mans 24 Graphics Layout
***************************************************************************/

static const gfx_layout wecleman_bg_layout =
{
	8,8,
	8*0x8000*3/(8*8*3),
	3,
	{ 0,0x8000*8,0x8000*8*2 },
	{0,7,6,5,4,3,2,1},
	{0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8},
	8*8
};

static const uint32_t wecleman_road_layout_xoffset[64] =
{
		0,7,6,5,4,3,2,1,
		8,15,14,13,12,11,10,9,
		16,23,22,21,20,19,18,17,
		24,31,30,29,28,27,26,25,

		0+32,7+32,6+32,5+32,4+32,3+32,2+32,1+32,
		8+32,15+32,14+32,13+32,12+32,11+32,10+32,9+32,
		16+32,23+32,22+32,21+32,20+32,19+32,18+32,17+32,
		24+32,31+32,30+32,29+32,28+32,27+32,26+32,25+32
};

/* We draw the road, made of 512 pixel lines, using 64x1 tiles */
static const gfx_layout wecleman_road_layout =
{
	64,1,
	8*0x4000*3/(64*1*3),
	3,
	{ 0x4000*8*2,0x4000*8*1,0x4000*8*0 },
	EXTENDED_XOFFS,
	{0},
	64*1,
	wecleman_road_layout_xoffset,
	nullptr
};

static GFXDECODE_START( wecleman )
	// "gfx1" holds sprite, which are not decoded here
	GFXDECODE_ENTRY( "gfx2", 0, wecleman_bg_layout,   0, 2048/8 )   // [0] bg + fg + txt
	GFXDECODE_ENTRY( "gfx3", 0, wecleman_road_layout, 0, 2048/8 )   // [1] road
GFXDECODE_END


/***************************************************************************
                            Hot Chase Graphics Layout
***************************************************************************/

static const uint32_t hotchase_road_layout_xoffset[64] =
{
		0*4,0*4,1*4,1*4,2*4,2*4,3*4,3*4,4*4,4*4,5*4,5*4,6*4,6*4,7*4,7*4,
		8*4,8*4,9*4,9*4,10*4,10*4,11*4,11*4,12*4,12*4,13*4,13*4,14*4,14*4,15*4,15*4,
		16*4,16*4,17*4,17*4,18*4,18*4,19*4,19*4,20*4,20*4,21*4,21*4,22*4,22*4,23*4,23*4,
		24*4,24*4,25*4,25*4,26*4,26*4,27*4,27*4,28*4,28*4,29*4,29*4,30*4,30*4,31*4,31*4
};

/* We draw the road, made of 512 pixel lines, using 64x1 tiles */
/* tiles are doubled horizontally */
static const gfx_layout hotchase_road_layout =
{
	64,1,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	EXTENDED_XOFFS,
	{0},
	32*4,
	hotchase_road_layout_xoffset,
	nullptr
};

static GFXDECODE_START( hotchase )
	// "gfx1" holds sprite, which are not decoded here
	// "gfx2" and 3 are for the 051316
	GFXDECODE_ENTRY( "gfx4", 0, hotchase_road_layout, 0x70*16, 16 ) // road
GFXDECODE_END


/***************************************************************************
                        WEC Le Mans 24 Hardware Definitions
***************************************************************************/


TIMER_DEVICE_CALLBACK_MEMBER(wecleman_state::wecleman_scanline)
{
	int scanline = param;

	if(scanline == 232) // vblank irq
		m_maincpu->set_input_line(4, HOLD_LINE);
	else if(((scanline % 64) == 0)) // timer irq TODO: wrong place maybe? Could do with blitter chip irq (007643/007645?) or "V-CNT" signal.
		m_maincpu->set_input_line(5, HOLD_LINE);
}

TIMER_DEVICE_CALLBACK_MEMBER(wecleman_state::hotchase_scanline)
{
	int scanline = param;

	if(scanline == 224) // vblank irq
		m_maincpu->set_input_line(4, HOLD_LINE);
}


MACHINE_RESET_MEMBER(wecleman_state,wecleman)
{
	m_k007232->set_bank( 0, 1 );
}

static MACHINE_CONFIG_START( wecleman )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M68000, 10000000)   /* Schems show 10MHz */
	MCFG_CPU_PROGRAM_MAP(wecleman_map)
	MCFG_TIMER_DRIVER_ADD_SCANLINE("scantimer", wecleman_state, wecleman_scanline, "screen", 0, 1)

	MCFG_CPU_ADD("sub", M68000, 10000000)   /* Schems show 10MHz */
	MCFG_CPU_PROGRAM_MAP(wecleman_sub_map)

	/* Schems: can be reset, no nmi, soundlatch, 3.58MHz */
	MCFG_CPU_ADD("audiocpu", Z80, 3579545)
	MCFG_CPU_PROGRAM_MAP(wecleman_sound_map)

	MCFG_QUANTUM_TIME(attotime::from_hz(6000))

	MCFG_MACHINE_RESET_OVERRIDE(wecleman_state,wecleman)

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_SIZE(320 +16, 256)
	MCFG_SCREEN_VISIBLE_AREA(0 +8, 320-1 +8, 0 +8, 224-1 +8)
	MCFG_SCREEN_UPDATE_DRIVER(wecleman_state, screen_update_wecleman)

	MCFG_GFXDECODE_ADD("gfxdecode", "palette", wecleman)

	MCFG_PALETTE_ADD("palette", 2048)

	MCFG_VIDEO_START_OVERRIDE(wecleman_state,wecleman)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")

	MCFG_GENERIC_LATCH_8_ADD("soundlatch")

	MCFG_YM2151_ADD("ymsnd", 3579545)
	MCFG_SOUND_ROUTE(0, "mono", 0.85)
	MCFG_SOUND_ROUTE(1, "mono", 0.85)

	MCFG_SOUND_ADD("k007232", K007232, 3579545)
	MCFG_K007232_PORT_WRITE_HANDLER(WRITE8(wecleman_state, wecleman_volume_callback))
	MCFG_SOUND_ROUTE(0, "mono", 0.20)
	MCFG_SOUND_ROUTE(1, "mono", 0.20)
MACHINE_CONFIG_END


/***************************************************************************
                        Hot Chase Hardware Definitions
***************************************************************************/

INTERRUPT_GEN_MEMBER(wecleman_state::hotchase_sound_timer)
{
	device.execute().set_input_line(M6809_FIRQ_LINE, HOLD_LINE);
}

MACHINE_RESET_MEMBER(wecleman_state,hotchase)
{
	int i;

	/* TODO: PCB reference clearly shows that the POST has random/filled data on the paletteram.
	         For now let's fill everything with white colors until we have better info about it */
	for(i=0;i<0x2000/2;i++)
	{
		m_generic_paletteram_16[i] = 0xffff;
		m_palette->set_pen_color(i,0xff,0xff,0xff);
	}
}


static MACHINE_CONFIG_START( hotchase )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M68000, 10000000)   /* 10 MHz - PCB is drawn in one set's readme */
	MCFG_CPU_PROGRAM_MAP(hotchase_map)
	MCFG_TIMER_DRIVER_ADD_SCANLINE("scantimer", wecleman_state, hotchase_scanline, "screen", 0, 1)

	MCFG_CPU_ADD("sub", M68000, 10000000)   /* 10 MHz - PCB is drawn in one set's readme */
	MCFG_CPU_PROGRAM_MAP(hotchase_sub_map)

	MCFG_CPU_ADD("audiocpu", M6809, 3579545 / 2)    /* 3.579/2 MHz - PCB is drawn in one set's readme */
	MCFG_CPU_PROGRAM_MAP(hotchase_sound_map)
	MCFG_CPU_PERIODIC_INT_DRIVER(wecleman_state, hotchase_sound_timer,  496)

	MCFG_QUANTUM_TIME(attotime::from_hz(6000))

	MCFG_MACHINE_RESET_OVERRIDE(wecleman_state,hotchase)

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_SIZE(320 +16, 256)
	MCFG_SCREEN_VISIBLE_AREA(0, 320-1, 0, 224-1)
	MCFG_SCREEN_UPDATE_DRIVER(wecleman_state, screen_update_hotchase)
	MCFG_SCREEN_PALETTE("palette")

	MCFG_GFXDECODE_ADD("gfxdecode", "palette", hotchase)
	MCFG_PALETTE_ADD("palette", 2048*2)

	MCFG_VIDEO_START_OVERRIDE(wecleman_state, hotchase)

	MCFG_K051316_ADD("roz_1", 4, false, [](u32 address, u32 &code, u16 &color) { code = address & 0x03ffff; color = (address & 0xfc0000) >> 14; })
	MCFG_K051316_WRAP(1)

	MCFG_K051316_ADD("roz_2", 4, false, [](u32 address, u32 &code, u16 &color) { code = address & 0x007fff; color = (address & 0x3f8000) >> 11; })

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")

	MCFG_GENERIC_LATCH_8_ADD("soundlatch")

	MCFG_SOUND_ADD("k007232_1", K007232, 3579545)
	// SLEV not used, volume control is elsewhere
	MCFG_SOUND_ROUTE(0, "mono", 0.20)
	MCFG_SOUND_ROUTE(1, "mono", 0.20)

	MCFG_SOUND_ADD("k007232_2", K007232, 3579545)
	// SLEV not used, volume control is elsewhere
	MCFG_SOUND_ROUTE(0, "mono", 0.20)
	MCFG_SOUND_ROUTE(1, "mono", 0.20)

	MCFG_SOUND_ADD("k007232_3", K007232, 3579545)
	// SLEV not used, volume control is elsewhere
	MCFG_SOUND_ROUTE(0, "mono", 0.20)
	MCFG_SOUND_ROUTE(1, "mono", 0.20)
MACHINE_CONFIG_END


/***************************************************************************
                        WEC Le Mans 24 ROM Definitions
***************************************************************************/

ROM_START( wecleman )
	ROM_REGION( 0x40000, "maincpu", 0 ) /* Main CPU Code */
	ROM_LOAD16_BYTE( "602f08.17h", 0x00000, 0x10000, CRC(493b79d3) SHA1(9625e3b65c211d5081d8ed8977de287eff100842) )
	ROM_LOAD16_BYTE( "602f11.23h", 0x00001, 0x10000, CRC(6bb4f1fa) SHA1(2cfb7885b42b49dab9892e8dfd54914b64eeab06) )
	ROM_LOAD16_BYTE( "602a09.18h", 0x20000, 0x10000, CRC(8a9d756f) SHA1(12605e86ce29e6300b5400720baac7b0293d9e66) )
	ROM_LOAD16_BYTE( "602a10.22h", 0x20001, 0x10000, CRC(569f5001) SHA1(ec2dd331a279083cf847fbbe71c017038a1d562a) )

	ROM_REGION( 0x10000, "sub", 0 ) /* Sub CPU Code */
	ROM_LOAD16_BYTE( "602a06.18a", 0x00000, 0x08000, CRC(e12c0d11) SHA1(991afd48bf1b2c303b975ce80c754e5972c39111) )
	ROM_LOAD16_BYTE( "602a07.20a", 0x00001, 0x08000, CRC(47968e51) SHA1(9b01b2c6a14dd80327a8f66a7f1994471a4bc38e) )

	ROM_REGION( 0x10000, "audiocpu", 0 )    /* Sound CPU Code */
	ROM_LOAD( "602a01.6d",  0x00000, 0x08000, CRC(deafe5f1) SHA1(4cfbe2841233b1222c22160af7287b7a7821c3a0) )

	ROM_REGION( 0x200000 * 2, "gfx1", 0 )   /* x2, do not dispose, zooming sprites */
	ROM_LOAD( "602a25.12e", 0x000000, 0x20000, CRC(0eacf1f9) SHA1(b4dcd457e68175ffee3da4aff23a241fe33eb500) )
	ROM_LOAD( "602a26.14e", 0x020000, 0x20000, CRC(2182edaf) SHA1(5ae4223a76b3c0be8f66458707f2e6f63fba0b13) )
	ROM_LOAD( "602a27.15e", 0x040000, 0x20000, CRC(b22f08e9) SHA1(1ba99bc4e00e206507e9bfafc989208d6ae6f8a3) )
	ROM_LOAD( "602a28.17e", 0x060000, 0x20000, CRC(5f6741fa) SHA1(9c81634f502da8682673b3b87efe0497af8abbd7) )
	ROM_LOAD( "602a21.6e",  0x080000, 0x20000, CRC(8cab34f1) SHA1(264df01460f44cd5ccdf3c8bd2d3f327874b69ea) )
	ROM_LOAD( "602a22.7e",  0x0a0000, 0x20000, CRC(e40303cb) SHA1(da943437ea2e208ea477f35bb05f77412ecdf9ac) )
	ROM_LOAD( "602a23.9e",  0x0c0000, 0x20000, CRC(75077681) SHA1(32ad10e9e32779c36bb50b402f5c6d941e293942) )
	ROM_LOAD( "602a24.10e", 0x0e0000, 0x20000, CRC(583dadad) SHA1(181ebe87095d739a5903c17ec851864e2275f571) )
	ROM_LOAD( "602a17.12c", 0x100000, 0x20000, CRC(31612199) SHA1(dff58ec3f7d98bfa7e9405f0f23647ff4ecfee62) )
	ROM_LOAD( "602a18.14c", 0x120000, 0x20000, CRC(3f061a67) SHA1(be57c38410c5635311d26afc44b3065e42fa12b7) )
	ROM_LOAD( "602a19.15c", 0x140000, 0x20000, CRC(5915dbc5) SHA1(61ab123c8a4128a18d7eb2cae99ad58203f03ffc) )
	ROM_LOAD( "602a20.17c", 0x160000, 0x20000, CRC(f87e4ef5) SHA1(4c2f0d036925a7ccd32aef3ca12b960a27247bc3) )
	ROM_LOAD( "602a13.6c",  0x180000, 0x20000, CRC(5d3589b8) SHA1(d146cb8511cfe825bdfe8296c7758545542a0faa) )
	ROM_LOAD( "602a14.7c",  0x1a0000, 0x20000, CRC(e3a75f6c) SHA1(80b20323e3560316ffbdafe4fd2f81326e103045) )
	ROM_LOAD( "602a15.9c",  0x1c0000, 0x20000, CRC(0d493c9f) SHA1(02690a1963cadd469bd67cb362384923916900a1) )
	ROM_LOAD( "602a16.10c", 0x1e0000, 0x20000, CRC(b08770b3) SHA1(41871e9261d08fd372b7deb72d939973fb694b54) )

	ROM_REGION( 0x18000, "gfx2", 0 )
	ROM_LOAD( "602a31.26g", 0x000000, 0x08000, CRC(01fa40dd) SHA1(2b8aa97f5116f39ae6a8e46f109853d70e370884) )   // layers
	ROM_LOAD( "602a30.24g", 0x008000, 0x08000, CRC(be5c4138) SHA1(7aee2ee17ef3e37399a60d9b019cfa733acbf07b) )
	ROM_LOAD( "602a29.23g", 0x010000, 0x08000, CRC(f1a8d33e) SHA1(ed6531f2fd4ad6835a879e9a5600387d8cad6d17) )

	ROM_REGION( 0x0c000, "gfx3", 0 )    /* road */
	ROM_LOAD( "602a04.11e", 0x000000, 0x08000, CRC(ade9f359) SHA1(58db6be6217ed697827015e50e99e58602042a4c) )
	ROM_LOAD( "602a05.13e", 0x008000, 0x04000, CRC(f22b7f2b) SHA1(857389c57552c4e2237cb599f4c68c381430475e) )   // may also exist as 32KB with one half empty

	ROM_REGION( 0x40000, "k007232", 0 )  /* Samples (Channel A 0x20000=Channel B) */
	ROM_LOAD( "602a03.10a", 0x00000, 0x20000, CRC(31392b01) SHA1(0424747bc2015c9c93afd20e6a23083c0dcc4fb7) )
	ROM_LOAD( "602a02.8a",  0x20000, 0x20000, CRC(e2be10ae) SHA1(109c31bf7252c83a062d259143cd8299681db778) )

	ROM_REGION( 0x04000, "user1", 0 )   /* extra data for road effects? */
	ROM_LOAD( "602a12.1a",  0x000000, 0x04000, CRC(77b9383d) SHA1(7cb970889677704d6324bb64aafc05326c4503ad) )
ROM_END

ROM_START( weclemana )
	ROM_REGION( 0x40000, "maincpu", 0 ) /* Main CPU Code */
	// I doubt these labels are correct, or one set of roms is bad (17h and 23h differ slightly from parent)
	ROM_LOAD16_BYTE( "602f08.17h", 0x00000, 0x10000, CRC(43241265) SHA1(3da1ed0d15b03845c07f07ec6838ce160d81633d) ) // sldh
	ROM_LOAD16_BYTE( "602f11.23h", 0x00001, 0x10000, CRC(3ea7dae0) SHA1(d33d67f4cc65a7680e5f43407136b75512a10230) ) // sldh
	ROM_LOAD16_BYTE( "602a09.18h", 0x20000, 0x10000, CRC(8a9d756f) SHA1(12605e86ce29e6300b5400720baac7b0293d9e66) )
	ROM_LOAD16_BYTE( "602a10.22h", 0x20001, 0x10000, CRC(569f5001) SHA1(ec2dd331a279083cf847fbbe71c017038a1d562a) )

	ROM_REGION( 0x10000, "sub", 0 ) /* Sub CPU Code */
	ROM_LOAD16_BYTE( "602a06.18a", 0x00000, 0x08000, CRC(e12c0d11) SHA1(991afd48bf1b2c303b975ce80c754e5972c39111) )
	ROM_LOAD16_BYTE( "602a07.20a", 0x00001, 0x08000, CRC(47968e51) SHA1(9b01b2c6a14dd80327a8f66a7f1994471a4bc38e) )

	ROM_REGION( 0x10000, "audiocpu", 0 )    /* Sound CPU Code */
	ROM_LOAD( "602a01.6d",  0x00000, 0x08000, CRC(deafe5f1) SHA1(4cfbe2841233b1222c22160af7287b7a7821c3a0) )

	ROM_REGION( 0x200000 * 2, "gfx1", 0 )   /* x2, do not dispose, zooming sprites */
	ROM_LOAD( "602a25.12e", 0x000000, 0x20000, CRC(0eacf1f9) SHA1(b4dcd457e68175ffee3da4aff23a241fe33eb500) )
	ROM_LOAD( "602a26.14e", 0x020000, 0x20000, CRC(2182edaf) SHA1(5ae4223a76b3c0be8f66458707f2e6f63fba0b13) )
	ROM_LOAD( "602a27.15e", 0x040000, 0x20000, CRC(b22f08e9) SHA1(1ba99bc4e00e206507e9bfafc989208d6ae6f8a3) )
	ROM_LOAD( "602a28.17e", 0x060000, 0x20000, CRC(5f6741fa) SHA1(9c81634f502da8682673b3b87efe0497af8abbd7) )
	ROM_LOAD( "602a21.6e",  0x080000, 0x20000, CRC(8cab34f1) SHA1(264df01460f44cd5ccdf3c8bd2d3f327874b69ea) )
	ROM_LOAD( "602a22.7e",  0x0a0000, 0x20000, CRC(e40303cb) SHA1(da943437ea2e208ea477f35bb05f77412ecdf9ac) )
	ROM_LOAD( "602a23.9e",  0x0c0000, 0x20000, CRC(75077681) SHA1(32ad10e9e32779c36bb50b402f5c6d941e293942) )
	ROM_LOAD( "602a24.10e", 0x0e0000, 0x20000, CRC(583dadad) SHA1(181ebe87095d739a5903c17ec851864e2275f571) )
	ROM_LOAD( "602a17.12c", 0x100000, 0x20000, CRC(31612199) SHA1(dff58ec3f7d98bfa7e9405f0f23647ff4ecfee62) )
	ROM_LOAD( "602a18.14c", 0x120000, 0x20000, CRC(3f061a67) SHA1(be57c38410c5635311d26afc44b3065e42fa12b7) )
	ROM_LOAD( "602a19.15c", 0x140000, 0x20000, CRC(5915dbc5) SHA1(61ab123c8a4128a18d7eb2cae99ad58203f03ffc) )
	ROM_LOAD( "602a20.17c", 0x160000, 0x20000, CRC(f87e4ef5) SHA1(4c2f0d036925a7ccd32aef3ca12b960a27247bc3) )
	ROM_LOAD( "602a13.6c",  0x180000, 0x20000, CRC(5d3589b8) SHA1(d146cb8511cfe825bdfe8296c7758545542a0faa) )
	ROM_LOAD( "602a14.7c",  0x1a0000, 0x20000, CRC(e3a75f6c) SHA1(80b20323e3560316ffbdafe4fd2f81326e103045) )
	ROM_LOAD( "602a15.9c",  0x1c0000, 0x20000, CRC(0d493c9f) SHA1(02690a1963cadd469bd67cb362384923916900a1) )
	ROM_LOAD( "602a16.10c", 0x1e0000, 0x20000, CRC(b08770b3) SHA1(41871e9261d08fd372b7deb72d939973fb694b54) )

	ROM_REGION( 0x18000, "gfx2", 0 )
	ROM_LOAD( "602a31.26g", 0x000000, 0x08000, CRC(01fa40dd) SHA1(2b8aa97f5116f39ae6a8e46f109853d70e370884) )   // layers
	ROM_LOAD( "602a30.24g", 0x008000, 0x08000, CRC(be5c4138) SHA1(7aee2ee17ef3e37399a60d9b019cfa733acbf07b) )
	ROM_LOAD( "602a29.23g", 0x010000, 0x08000, CRC(f1a8d33e) SHA1(ed6531f2fd4ad6835a879e9a5600387d8cad6d17) )

	ROM_REGION( 0x0c000, "gfx3", 0 )    /* road */
	ROM_LOAD( "602a04.11e", 0x000000, 0x08000, CRC(ade9f359) SHA1(58db6be6217ed697827015e50e99e58602042a4c) )
	ROM_LOAD( "602a05.13e", 0x008000, 0x04000, CRC(f22b7f2b) SHA1(857389c57552c4e2237cb599f4c68c381430475e) )   // may also exist as 32KB with one half empty

	ROM_REGION( 0x40000, "k007232", 0 )  /* Samples (Channel A 0x20000=Channel B) */
	ROM_LOAD( "602a03.10a", 0x00000, 0x20000, CRC(31392b01) SHA1(0424747bc2015c9c93afd20e6a23083c0dcc4fb7) )
	ROM_LOAD( "602a02.8a",  0x20000, 0x20000, CRC(e2be10ae) SHA1(109c31bf7252c83a062d259143cd8299681db778) )

	ROM_REGION( 0x04000, "user1", 0 )   /* extra data for road effects? */
	ROM_LOAD( "602a12.1a",  0x000000, 0x04000, CRC(77b9383d) SHA1(7cb970889677704d6324bb64aafc05326c4503ad) )
ROM_END
/*
early set V.1.26
rom labels faded out, all other roms match
*/
ROM_START( weclemanb )
	ROM_REGION( 0x40000, "maincpu", 0 ) /* Main CPU Code */
	ROM_LOAD16_BYTE( "17h", 0x00000, 0x10000, CRC(66901326) SHA1(672aab497e9b94843451e016de6ca6d3c358362e) )
	ROM_LOAD16_BYTE( "23h", 0x00001, 0x10000, CRC(d9d492f4) SHA1(12c177fa5cc541be86431f314e96a4f3a74f95c6) )
	ROM_LOAD16_BYTE( "602a09.18h", 0x20000, 0x10000, CRC(8a9d756f) SHA1(12605e86ce29e6300b5400720baac7b0293d9e66) )
	ROM_LOAD16_BYTE( "602a10.22h", 0x20001, 0x10000, CRC(569f5001) SHA1(ec2dd331a279083cf847fbbe71c017038a1d562a) )

	ROM_REGION( 0x10000, "sub", 0 ) /* Sub CPU Code */
	ROM_LOAD16_BYTE( "602a06.18a", 0x00000, 0x08000, CRC(e12c0d11) SHA1(991afd48bf1b2c303b975ce80c754e5972c39111) )
	ROM_LOAD16_BYTE( "602a07.20a", 0x00001, 0x08000, CRC(47968e51) SHA1(9b01b2c6a14dd80327a8f66a7f1994471a4bc38e) )

	ROM_REGION( 0x10000, "audiocpu", 0 )    /* Sound CPU Code */
	ROM_LOAD( "602a01.6d",  0x00000, 0x08000, CRC(deafe5f1) SHA1(4cfbe2841233b1222c22160af7287b7a7821c3a0) )

	ROM_REGION( 0x200000 * 2, "gfx1", 0 )   /* x2, do not dispose, zooming sprites */
	ROM_LOAD( "602a25.12e", 0x000000, 0x20000, CRC(0eacf1f9) SHA1(b4dcd457e68175ffee3da4aff23a241fe33eb500) )
	ROM_LOAD( "602a26.14e", 0x020000, 0x20000, CRC(2182edaf) SHA1(5ae4223a76b3c0be8f66458707f2e6f63fba0b13) )
	ROM_LOAD( "602a27.15e", 0x040000, 0x20000, CRC(b22f08e9) SHA1(1ba99bc4e00e206507e9bfafc989208d6ae6f8a3) )
	ROM_LOAD( "602a28.17e", 0x060000, 0x20000, CRC(5f6741fa) SHA1(9c81634f502da8682673b3b87efe0497af8abbd7) )
	ROM_LOAD( "602a21.6e",  0x080000, 0x20000, CRC(8cab34f1) SHA1(264df01460f44cd5ccdf3c8bd2d3f327874b69ea) )
	ROM_LOAD( "602a22.7e",  0x0a0000, 0x20000, CRC(e40303cb) SHA1(da943437ea2e208ea477f35bb05f77412ecdf9ac) )
	ROM_LOAD( "602a23.9e",  0x0c0000, 0x20000, CRC(75077681) SHA1(32ad10e9e32779c36bb50b402f5c6d941e293942) )
	ROM_LOAD( "602a24.10e", 0x0e0000, 0x20000, CRC(583dadad) SHA1(181ebe87095d739a5903c17ec851864e2275f571) )
	ROM_LOAD( "602a17.12c", 0x100000, 0x20000, CRC(31612199) SHA1(dff58ec3f7d98bfa7e9405f0f23647ff4ecfee62) )
	ROM_LOAD( "602a18.14c", 0x120000, 0x20000, CRC(3f061a67) SHA1(be57c38410c5635311d26afc44b3065e42fa12b7) )
	ROM_LOAD( "602a19.15c", 0x140000, 0x20000, CRC(5915dbc5) SHA1(61ab123c8a4128a18d7eb2cae99ad58203f03ffc) )
	ROM_LOAD( "602a20.17c", 0x160000, 0x20000, CRC(f87e4ef5) SHA1(4c2f0d036925a7ccd32aef3ca12b960a27247bc3) )
	ROM_LOAD( "602a13.6c",  0x180000, 0x20000, CRC(5d3589b8) SHA1(d146cb8511cfe825bdfe8296c7758545542a0faa) )
	ROM_LOAD( "602a14.7c",  0x1a0000, 0x20000, CRC(e3a75f6c) SHA1(80b20323e3560316ffbdafe4fd2f81326e103045) )
	ROM_LOAD( "602a15.9c",  0x1c0000, 0x20000, CRC(0d493c9f) SHA1(02690a1963cadd469bd67cb362384923916900a1) )
	ROM_LOAD( "602a16.10c", 0x1e0000, 0x20000, CRC(b08770b3) SHA1(41871e9261d08fd372b7deb72d939973fb694b54) )

	ROM_REGION( 0x18000, "gfx2", 0 )
	ROM_LOAD( "602a31.26g", 0x000000, 0x08000, CRC(01fa40dd) SHA1(2b8aa97f5116f39ae6a8e46f109853d70e370884) )   // layers
	ROM_LOAD( "602a30.24g", 0x008000, 0x08000, CRC(be5c4138) SHA1(7aee2ee17ef3e37399a60d9b019cfa733acbf07b) )
	ROM_LOAD( "602a29.23g", 0x010000, 0x08000, CRC(f1a8d33e) SHA1(ed6531f2fd4ad6835a879e9a5600387d8cad6d17) )

	ROM_REGION( 0x0c000, "gfx3", 0 )    /* road */
	ROM_LOAD( "602a04.11e", 0x000000, 0x08000, CRC(ade9f359) SHA1(58db6be6217ed697827015e50e99e58602042a4c) )
	ROM_LOAD( "602a05.13e", 0x008000, 0x04000, CRC(f22b7f2b) SHA1(857389c57552c4e2237cb599f4c68c381430475e) )   // may also exist as 32KB with one half empty

	ROM_REGION( 0x40000, "k007232", 0 )  /* Samples (Channel A 0x20000=Channel B) */
	ROM_LOAD( "602a03.10a", 0x00000, 0x20000, CRC(31392b01) SHA1(0424747bc2015c9c93afd20e6a23083c0dcc4fb7) )
	ROM_LOAD( "602a02.8a",  0x20000, 0x20000, CRC(e2be10ae) SHA1(109c31bf7252c83a062d259143cd8299681db778) )

	ROM_REGION( 0x04000, "user1", 0 )   /* extra data for road effects? */
	ROM_LOAD( "602a12.1a",  0x000000, 0x04000, CRC(77b9383d) SHA1(7cb970889677704d6324bb64aafc05326c4503ad) )
ROM_END

void wecleman_state::wecleman_unpack_sprites()
{
	const char *region       = "gfx1";  // sprites

	const uint32_t len = memregion(region)->bytes();
	uint8_t *src     = memregion(region)->base() + len / 2 - 1;
	uint8_t *dst     = memregion(region)->base() + len - 1;

	while(dst > src)
	{
		uint8_t data = *src--;
		if( (data&0xf0) == 0xf0 ) data &= 0x0f;
		if( (data&0x0f) == 0x0f ) data &= 0xf0;
		*dst-- = data & 0xF;    *dst-- = data >> 4;
	}
}

void wecleman_state::bitswap(uint8_t *src,size_t len,int _14,int _13,int _12,int _11,int _10,int _f,int _e,int _d,int _c,int _b,int _a,int _9,int _8,int _7,int _6,int _5,int _4,int _3,int _2,int _1,int _0)
{
	std::vector<uint8_t> buffer(len);
	int i;

	memcpy(&buffer[0],src,len);
	for (i = 0;i < len;i++)
	{
		src[i] =
			buffer[BITSWAP24(i,23,22,21,_14,_13,_12,_11,_10,_f,_e,_d,_c,_b,_a,_9,_8,_7,_6,_5,_4,_3,_2,_1,_0)];
	}
}

/* Unpack sprites data and do some patching */
DRIVER_INIT_MEMBER(wecleman_state,wecleman)
{
	int i, len;
	uint8_t *RAM;
//  uint16_t *RAM1 = (uint16_t *) memregion("maincpu")->base();   /* Main CPU patches */
//  RAM1[0x08c2/2] = 0x601e;    // faster self test

	/* Decode GFX Roms - Compensate for the address lines scrambling */

	/*  Sprites - decrypting the sprites nearly KILLED ME!
	    It's been the main cause of the delay of this driver ...
	    I hope you'll appreciate this effort!  */

	/* let's swap even and odd *pixels* of the sprites */
	RAM = memregion("gfx1")->base();
	len = memregion("gfx1")->bytes();
	for (i = 0; i < len; i ++)
	{
		/* TODO: could be wrong, colors have to be fixed.       */
		/* The only certain thing is that 87 must convert to f0 */
		/* otherwise stray lines appear, made of pens 7 & 8     */
		RAM[i] = BITSWAP8(RAM[i],7,0,1,2,3,4,5,6);
	}

	bitswap(memregion("gfx1")->base(), memregion("gfx1")->bytes(),
			0,1,20,19,18,17,14,9,16,6,4,7,8,15,10,11,13,5,12,3,2);

	/* Now we can unpack each nibble of the sprites into a pixel (one byte) */
	wecleman_unpack_sprites();

	/* Bg & Fg & Txt */
	bitswap(memregion("gfx2")->base(), memregion("gfx2")->bytes(),
			20,19,18,17,16,15,12,7,14,4,2,5,6,13,8,9,11,3,10,1,0);

	/* Road */
	bitswap(memregion("gfx3")->base(), memregion("gfx3")->bytes(),
			20,19,18,17,16,15,14,7,12,4,2,5,6,13,8,9,11,3,10,1,0);

	m_spr_color_offs = 0x40;
	m_sound_hw_type = 0;
}


/***************************************************************************
                            Hot Chase ROM Definitions
***************************************************************************/

ROM_START( hotchase )
	ROM_REGION( 0x40000, "maincpu", 0 ) /* Main Code */
	ROM_LOAD16_BYTE( "763k05", 0x000000, 0x010000, CRC(f34fef0b) SHA1(9edaf6da988348cb32d5686fe7a67fb92b1c9777) )
	ROM_LOAD16_BYTE( "763k04", 0x000001, 0x010000, CRC(60f73178) SHA1(49c919d09fa464b205d7eccce337349e3a633a14) )
	ROM_LOAD16_BYTE( "763k03", 0x020000, 0x010000, CRC(28e3a444) SHA1(106b22a3cbe8301eac2e46674a267b96e72ac72f) )
	ROM_LOAD16_BYTE( "763k02", 0x020001, 0x010000, CRC(9510f961) SHA1(45b1920cab08a0dacd044c851d4e7f0cb5772b46) )

	ROM_REGION( 0x20000, "sub", 0 ) /* Sub Code */
	ROM_LOAD16_BYTE( "763k07", 0x000000, 0x010000, CRC(ae12fa90) SHA1(7f76f09916fe152411b5af3c504ee7be07497ef4) )
	ROM_LOAD16_BYTE( "763k06", 0x000001, 0x010000, CRC(b77e0c07) SHA1(98bf492ac889d31419df706029fdf3d51b85c936) )

	ROM_REGION( 0x10000, "audiocpu", 0 )    /* Sound Code */
	ROM_LOAD( "763f01", 0x8000, 0x8000, CRC(4fddd061) SHA1(ff0aa18605612f6102107a6be1f93ae4c5edc84f) )

	ROM_REGION( 0x300000 * 2, "gfx1", 0 )   /* x2, do not dispose, zooming sprites */
	ROM_LOAD16_WORD_SWAP( "763e17", 0x000000, 0x080000, CRC(8db4e0aa) SHA1(376cb3cae110998f2f9df7e6cdd35c06732fea69) )
	ROM_LOAD16_WORD_SWAP( "763e20", 0x080000, 0x080000, CRC(a22c6fce) SHA1(174fb9c1706c092947bcce386831acd33a237046) )
	ROM_LOAD16_WORD_SWAP( "763e18", 0x100000, 0x080000, CRC(50920d01) SHA1(313c7ecbd154b3f4c96f25c29a7734a9b3facea4) )
	ROM_LOAD16_WORD_SWAP( "763e21", 0x180000, 0x080000, CRC(77e0e93e) SHA1(c8e415438a1f5ad79b10fd3ad5cb22de0d562e5d) )
	ROM_LOAD16_WORD_SWAP( "763e19", 0x200000, 0x080000, CRC(a2622e56) SHA1(0a0ed9713882b987518e6f06a02dba417c1f4f32) )
	ROM_LOAD16_WORD_SWAP( "763e22", 0x280000, 0x080000, CRC(967c49d1) SHA1(01979d216a9fd8085298445ac5f7870d1598db74) )

	ROM_REGION( 0x20000, "k051316_1", 0 )    /* bg */
	ROM_LOAD( "763e14", 0x000000, 0x020000, CRC(60392aa1) SHA1(8499eb40a246587e24f6fd00af2eaa6d75ee6363) )

	ROM_REGION( 0x08000, "k051316_2", 0 )    /* fg */
	/* first half empty - PCB silkscreen reads "27256/27512" */
	ROM_LOAD( "763a13", 0x000000, 0x008000, CRC(8bed8e0d) SHA1(ccff330abc23fe499e76c16cab5783c3daf155dd) )
	ROM_CONTINUE( 0x000000, 0x008000 )

	ROM_REGION( 0x20000, "gfx4", 0 )    /* road */
	ROM_LOAD( "763e15", 0x000000, 0x020000, CRC(7110aa43) SHA1(639dc002cc1580f0530bb5bb17f574e2258d5954) )

	ROM_REGION( 0x40000, "k007232_1", 0 ) /* Samples, 2 banks */
	ROM_LOAD( "763e11", 0x000000, 0x040000, CRC(9d99a5a7) SHA1(96e37bbb259e0a91d124c26b6b1a9b70de2e19a4) )

	ROM_REGION( 0x40000, "k007232_2", 0 ) /* Samples, 2 banks */
	ROM_LOAD( "763e10", 0x000000, 0x040000, CRC(ca409210) SHA1(703d7619c4bd33d2ff5fad127d98c82906fede33) )

	ROM_REGION( 0x100000, "k007232_3", 0 )    /* Samples, 4 banks for each ROM */
	ROM_LOAD( "763e08", 0x000000, 0x080000, CRC(054a9a63) SHA1(45d7926c9e7af47c041ba9b733e334bccd730a6d) )
	ROM_LOAD( "763e09", 0x080000, 0x080000, CRC(c39857db) SHA1(64b135a9ccf9e1dd50789cdd5c6bc03da8decfd0) )

	ROM_REGION( 0x08000, "user1", 0 )   /* extra data for road effects? */
	ROM_LOAD( "763a12", 0x000000, 0x008000, CRC(05f1e553) SHA1(8aaeb7374bd93038c24e6470398936f22cabb0fe) )
ROM_END

/*

Hot Chase
Konami 1988



         E08D E08B    E09D E09B  E10D E10B
         E08A E08C    E09A E09C  E10A E10C

GX763 350861

               E09      E10        E11
               E08      07232      07232
               07232   3.579MHz         2128
                               6809     P01.R10
      SW1
      SW2                               2128  2128
               6264 6264                6264  6264
                                        R02.P14 R03.R14
                            07770       R04.P16 R05.R16
      2018-45  D06.E18 D07.H18   10MHz
      2018-45  68000-10         07641   68000-10

GX763 350860

 051316 PSAC    051316 PSAC  A13.H5 A14.J5
                                                2018-45 2018-R6
                    2018-45
                    2018-45                     07558
 051316 PSAC        2018-45                            A12.R13

                             A15.H14

    A23.B17                            07634
                                                     07635
               2018-45 2018-45
               2018-45 2018-45         07557
               2018-45 2018-45                       25.2MHz
               2018-45 2018-45


Left EPROM board

                                   E19A.A8 E19B.A7 E19C.A6 E19D.A5
E22E.B12 E22F.B11 E22G.B10 E22H.B9 E19E.B8 E19F.B7 E19G.B6 E19H.B5
                                   E22A.D9 E22B.D7 E22C.D6 E22D.D5

Right EPROM board

E21E E21F E21G E21H E17A E17B E17C E17D E18A E18B E18C E18D
E20E E20F E20G E20H E17E E17F E17G E17H E18E E18F E18G E18H
                    E20A E20B E20C E20D E21A E21B E21C E21D

*/

// uses EPROM sub-boards in place of some of the MASK roms, different program too
ROM_START( hotchasea )
	ROM_REGION( 0x40000, "maincpu", 0 ) /* Main Code */
	ROM_LOAD16_BYTE( "763r05.r16", 0x000000, 0x010000, CRC(c880d5e4) SHA1(3c3ab3ad5496cfbc8de76620eedc06601ee7a8c7) )
	ROM_LOAD16_BYTE( "763r04.p16", 0x000001, 0x010000, CRC(b732ee2c) SHA1(b3d73cf5039980ac74927eef656326515fd2026b) )
	ROM_LOAD16_BYTE( "763r03.r14", 0x020000, 0x010000, CRC(13dd71de) SHA1(4b86b81ef79e0e92a1e458010b0b9574183a9c29) )
	ROM_LOAD16_BYTE( "763r02.p14", 0x020001, 0x010000, CRC(6cd1a18e) SHA1(0ddfe6a46e95052534325f228b7f0faba121950e) )

	ROM_REGION( 0x20000, "sub", 0 ) /* Sub Code */
	ROM_LOAD16_BYTE( "763d07.h18", 0x000000, 0x010000, CRC(ae12fa90) SHA1(7f76f09916fe152411b5af3c504ee7be07497ef4) )
	ROM_LOAD16_BYTE( "763d06.e18", 0x000001, 0x010000, CRC(b77e0c07) SHA1(98bf492ac889d31419df706029fdf3d51b85c936) )

	ROM_REGION( 0x10000, "audiocpu", 0 )    /* Sound Code */
	ROM_LOAD( "763p01.r10", 0x8000, 0x8000, CRC(15dbca7b) SHA1(ac0c965b72a8579a3b60dbadfb942248d2cff2d8) )

	ROM_REGION( 0x300000 * 2, "gfx1", 0 )   /* x2, do not dispose, zooming sprites */
	ROM_LOAD16_BYTE( "763e17a", 0x000000, 0x010000, CRC(8542d7d7) SHA1(a7c8aa7d8e0cabdc5269eb7adff944aaa0f819b6) )
	ROM_LOAD16_BYTE( "763e17e", 0x000001, 0x010000, CRC(4b4d919c) SHA1(0364eb74da8db7238888274d12011de876662d5a) )
	ROM_LOAD16_BYTE( "763e17b", 0x020000, 0x010000, CRC(ba9d7e72) SHA1(3af618087dcc66552ffabaf655f97b20e597122c) )
	ROM_LOAD16_BYTE( "763e17f", 0x020001, 0x010000, CRC(582400bb) SHA1(9479e45087d908c9b20392dba2a752a7ec1482e2) )
	ROM_LOAD16_BYTE( "763e17c", 0x040000, 0x010000, CRC(0ed292f8) SHA1(8c161e73c7f27925377799f67585b888bade6d82) )
	ROM_LOAD16_BYTE( "763e17g", 0x040001, 0x010000, CRC(35b27ed7) SHA1(e17e7674ee210ff340482a16dce3439b55c29f72) )
	ROM_LOAD16_BYTE( "763e17d", 0x060000, 0x010000, CRC(0166d00d) SHA1(e58f6deabc5743f6610252242f97bd5e973316ae) )
	ROM_LOAD16_BYTE( "763e17h", 0x060001, 0x010000, CRC(e5b8e8e6) SHA1(ae1349977559ff24dcd1678d6fd3a3e118612d07) )
	ROM_LOAD16_BYTE( "763e20a", 0x080000, 0x010000, CRC(256fe63c) SHA1(414325f2ff9abc411e2401dddd216e1a4de8a01e) )
	ROM_LOAD16_BYTE( "763e20e", 0x080001, 0x010000, CRC(ee8ca7e1) SHA1(fee544d6508f4106176f39e3765961e9f80fe620) )
	ROM_LOAD16_BYTE( "763e20b", 0x0a0000, 0x010000, CRC(b6714c24) SHA1(88f6437e181f36b7e44f1c70872314d8c0cc30e7) )
	ROM_LOAD16_BYTE( "763e20f", 0x0a0001, 0x010000, CRC(9dbc4b21) SHA1(31559903707a4f8ba3b044e8aad928de38403dcf) )
	ROM_LOAD16_BYTE( "763e20c", 0x0c0000, 0x010000, CRC(5173ad9b) SHA1(afe82c69f7036c7595f1a56b22176ba202b00b5c) )
	ROM_LOAD16_BYTE( "763e20g", 0x0c0001, 0x010000, CRC(b8c77f99) SHA1(e3bea1481c5b1c4733130651f9cf18587d3efc46) )
	ROM_LOAD16_BYTE( "763e20d", 0x0e0000, 0x010000, CRC(4ebdba32) SHA1(ac7daa291c82f75b09faf7bc5f6257870cc46061) )
	ROM_LOAD16_BYTE( "763e20h", 0x0e0001, 0x010000, CRC(0a428654) SHA1(551026f6f57d38aedd3498cce33af7bb2cf07184) )
	ROM_LOAD16_BYTE( "763e18a", 0x100000, 0x010000, CRC(09748099) SHA1(1821e2067b9a50a0638c8d105c617f4030d61877) )
	ROM_LOAD16_BYTE( "763e18e", 0x100001, 0x010000, CRC(049d4fcf) SHA1(aed18297677a3bb0b7197f59ea329aef9b678c01) )
	ROM_LOAD16_BYTE( "763e18b", 0x120000, 0x010000, CRC(ed0c3369) SHA1(84f336546dee01fec31c9c256ee00a9f8448cea4) )
	ROM_LOAD16_BYTE( "763e18f", 0x120001, 0x010000, CRC(b596a9ce) SHA1(dea0fe1c3386b5f0d19df4467f42d077678ae220) )
	ROM_LOAD16_BYTE( "763e18c", 0x140000, 0x010000, CRC(5a278291) SHA1(05c529baa68ef5877a28901c6f221e3d3593735f) )
	ROM_LOAD16_BYTE( "763e18g", 0x140001, 0x010000, CRC(aa7263cd) SHA1(b2acf66c02faf7777c5cb947aaf8e038f29c0f2e) )
	ROM_LOAD16_BYTE( "763e18d", 0x160000, 0x010000, CRC(b0b79a71) SHA1(46d0f17b7a6e4fb94ac9b8335bc598339d7707a5) )
	ROM_LOAD16_BYTE( "763e18h", 0x160001, 0x010000, CRC(a18b9127) SHA1(890971d2922a59ff4beea00238e710c8d3e0f19d) )
	ROM_LOAD16_BYTE( "763e21a", 0x180000, 0x010000, CRC(60788c29) SHA1(4faaa192d07f6acac0e7d11676146ecd0e71541f) )
	ROM_LOAD16_BYTE( "763e21e", 0x180001, 0x010000, CRC(844799ff) SHA1(8dc3ae3bb30ecb4e921a5b2068d3cd9421577844) )
	ROM_LOAD16_BYTE( "763e21b", 0x1a0000, 0x010000, CRC(1eefed61) SHA1(9c09dbff073d63384bf1ec9df4db4833afa33826) )
	ROM_LOAD16_BYTE( "763e21f", 0x1a0001, 0x010000, CRC(3aacfb10) SHA1(fb3eebf1f0850ed2f8f02cd4b5b564524e391afd) )
	ROM_LOAD16_BYTE( "763e21c", 0x1c0000, 0x010000, CRC(97e48b37) SHA1(864c73f48d839c2afeecec99605be6111f450ddd) )
	ROM_LOAD16_BYTE( "763e21g", 0x1c0001, 0x010000, CRC(74fefb12) SHA1(7746918c3ea8981c9cb2ead79a252939ba8bde3f) )
	ROM_LOAD16_BYTE( "763e21d", 0x1e0000, 0x010000, CRC(dd41569e) SHA1(065ee2de9ad6980788807cb563feccef1c3d1b9d) )
	ROM_LOAD16_BYTE( "763e21h", 0x1e0001, 0x010000, CRC(7ea52bf6) SHA1(2be93f88ccdea989b05beca13ebbfb77626ea41f) )
	ROM_LOAD16_BYTE( "763e19a", 0x200000, 0x010000, CRC(8c912c46) SHA1(e314edc39c32471c6fa2969e7c5c771c19fda88c) )
	ROM_LOAD16_BYTE( "763e19e", 0x200001, 0x010000, CRC(0eb34787) SHA1(9b8145dae210a177585e672fce30339b39c3c0f3) )
	ROM_LOAD16_BYTE( "763e19b", 0x220000, 0x010000, CRC(79960729) SHA1(f5c20ed7683aad8a435c292fbd5a1acc2a97ecee) )
	ROM_LOAD16_BYTE( "763e19f", 0x220001, 0x010000, CRC(1764ec5f) SHA1(4f7a0a3667087523a1ccdfc2d0e54a520f1216b3) )
	ROM_LOAD16_BYTE( "763e19c", 0x240000, 0x010000, CRC(f13377ac) SHA1(89f8d730cb457cc9cf55049b7002514302b2b04f) )
	ROM_LOAD16_BYTE( "763e19g", 0x240001, 0x010000, CRC(f2102e89) SHA1(41ff5d8904618a77c7c3c78c52c6f1b9c5a318fd) )
	ROM_LOAD16_BYTE( "763e19d", 0x260000, 0x010000, CRC(0b2a19f4) SHA1(3689b2c1f6227224fbcecc0d2470048a99510794) )
	ROM_LOAD16_BYTE( "763e19h", 0x260001, 0x010000, CRC(cd6d08a5) SHA1(ce13a8bba84f24e7d1fb25254e2e95f591fe1d67) )
	ROM_LOAD16_BYTE( "763e22a", 0x280000, 0x010000, CRC(16eec250) SHA1(f50375f207575e9d280285aca493902afbb7d729) )
	ROM_LOAD16_BYTE( "763e22e", 0x280001, 0x010000, CRC(c184b1c0) SHA1(d765e6eb2631b77dff5331840ac2a99cf1250362) )
	ROM_LOAD16_BYTE( "763e22b", 0x2a0000, 0x010000, CRC(1afe4b0c) SHA1(ce5a855291b443c1e16dbf54730960600c754fee) )
	ROM_LOAD16_BYTE( "763e22f", 0x2a0001, 0x010000, CRC(61f27c98) SHA1(d80af1a3e424c8dbab4fd21d494a0580ab96cd8d) )
	ROM_LOAD16_BYTE( "763e22c", 0x2c0000, 0x010000, CRC(c19b4b63) SHA1(93708b8769c44d5b93dcd2928a0d1120cc52c6ee) )
	ROM_LOAD16_BYTE( "763e22g", 0x2c0001, 0x010000, CRC(5bcbaf29) SHA1(621aa19606a15abb1539c07a033b32fc374a2d6a) )
	ROM_LOAD16_BYTE( "763e22d", 0x2e0000, 0x010000, CRC(fd5b669d) SHA1(fd5d82886708187e53c204c574ee252fc8a793af) )
	ROM_LOAD16_BYTE( "763e22h", 0x2e0001, 0x010000, CRC(9a9f45d8) SHA1(24fa9425b00441fff124eae7b40df7e65c920323) )

	ROM_REGION( 0x20000, "k051316_1", 0 )    /* bg */
	ROM_LOAD( "763a14", 0x000000, 0x020000, CRC(60392aa1) SHA1(8499eb40a246587e24f6fd00af2eaa6d75ee6363) )

	ROM_REGION( 0x08000, "k051316_2", 0 )    /* fg */
	/* first half empty - PCB silkscreen reads "27256/27512" */
	ROM_LOAD( "763a13", 0x000000, 0x008000, CRC(8bed8e0d) SHA1(ccff330abc23fe499e76c16cab5783c3daf155dd) )
	ROM_CONTINUE( 0x000000, 0x008000 )

	ROM_REGION( 0x20000, "gfx4", 0 )    /* road */
	ROM_LOAD( "763a15", 0x000000, 0x020000, CRC(7110aa43) SHA1(639dc002cc1580f0530bb5bb17f574e2258d5954) )

	ROM_REGION( 0x40000, "k007232_1", 0 ) /* Samples, 2 banks */
	ROM_LOAD( "763e11a", 0x000000, 0x010000, CRC(a60a93c8) SHA1(ce319f2b30c82f66fee0bab65d091ef4bef58a89) )
	ROM_LOAD( "763e11b", 0x010000, 0x010000, CRC(7750feb5) SHA1(e0900b8af400a50a22907ffa514847003cef342d) )
	ROM_LOAD( "763e11c", 0x020000, 0x010000, CRC(78b89bf8) SHA1(b74427e363a486d4be003df39f4ca8d10b9d5fc9) )
	ROM_LOAD( "763e11d", 0x030000, 0x010000, CRC(5f38d054) SHA1(ce0c87a7b7c0806e09cce5d48a651f12f790dd27) )

	ROM_REGION( 0x40000, "k007232_2", 0 ) /* Samples, 2 banks */
	ROM_LOAD( "763e10a", 0x000000, 0x010000, CRC(2b1cbefc) SHA1(f23fb943c277a05f2aa4d25de692d1fb3bb752ac) )
	ROM_LOAD( "763e10b", 0x010000, 0x010000, CRC(8209c950) SHA1(944c2afb4cfc67bd243de499f5ca6a7982980f45) )
	ROM_LOAD( "763e10c", 0x020000, 0x010000, CRC(b91d6c07) SHA1(ef90457cb495750c5793cd1716d0dc44d33d0a95) )
	ROM_LOAD( "763e10d", 0x030000, 0x010000, CRC(5b465d20) SHA1(66f10b58873e738f5539b960468c7f92d07c28bc) )

	ROM_REGION( 0x100000, "k007232_3", 0 )    /* Samples, 4 banks for each ROM */
	ROM_LOAD( "763e08a", 0x000000, 0x020000, CRC(02e4e7ef) SHA1(1622e4d85a333acae6e5f9304a037389bfeb71dc) )
	ROM_LOAD( "763e08b", 0x020000, 0x020000, CRC(94edde2f) SHA1(b124f83f271dab710d5ecb67a70cac7b4b41931c) )
	ROM_LOAD( "763e08c", 0x040000, 0x020000, CRC(b1ab1529) SHA1(962ad45fdccf6431e05eaec65d1b2f7842bddc02) )
	ROM_LOAD( "763e08d", 0x060000, 0x020000, CRC(ee8d14db) SHA1(098ba4f27b8cbb0ce017b28e5b69d6a3d2efa1df) )

	ROM_LOAD( "763e09a", 0x080000, 0x020000, CRC(1e6628ec) SHA1(9d24da1d32cb39dcbe3d0633045054d398ca8eb8) )
	ROM_LOAD( "763e09b", 0x0a0000, 0x020000, CRC(f0c2feb8) SHA1(9454d45a97dc2e823baf68dce85acce8e82a18f2) )
	ROM_LOAD( "763e09c", 0x0c0000, 0x020000, CRC(a0ade3e4) SHA1(1c94cede76f9350769a589625fadaee855c38866) )
	ROM_LOAD( "763e09d", 0x0e0000, 0x020000, CRC(c74e484d) SHA1(dd7ef64c30443847c638291f6cd2b45a5f0b2310) )

	ROM_REGION( 0x08000, "user1", 0 )   /* extra data for road effects? */
	ROM_LOAD( "763a12", 0x000000, 0x008000, CRC(05f1e553) SHA1(8aaeb7374bd93038c24e6470398936f22cabb0fe) )

	ROM_REGION( 0x200, "user2", 0 )
	ROM_LOAD( "763a23.b17", 0x00000, 0x200, CRC(81c30352) SHA1(20700aed065929835ef5c3b564a6f531f0a8fedf) )
ROM_END


/*      Important: you must leave extra space when listing sprite ROMs
    in a ROM module definition.  This routine unpacks each sprite nibble
    into a byte, doubling the memory consumption. */

void wecleman_state::hotchase_sprite_decode( int num16_banks, int bank_size )
{
	uint8_t *base;
	int i;

	base = memregion("gfx1")->base(); // sprites
	std::vector<uint8_t> temp( bank_size );

	for( i = num16_banks; i >0; i-- ){
		uint8_t *finish   = base + 2*bank_size*i;
		uint8_t *dest     = finish - 2*bank_size;

		uint8_t *p1 = &temp[0];
		uint8_t *p2 = &temp[bank_size/2];

		uint8_t data;

		memcpy (&temp[0], base+bank_size*(i-1), bank_size);

		do {
			data = *p1++;
			if( (data&0xf0) == 0xf0 ) data &= 0x0f;
			if( (data&0x0f) == 0x0f ) data &= 0xf0;
			*dest++ = data >> 4;
			*dest++ = data & 0xF;
			data = *p1++;
			if( (data&0xf0) == 0xf0 ) data &= 0x0f;
			if( (data&0x0f) == 0x0f ) data &= 0xf0;
			*dest++ = data >> 4;
			*dest++ = data & 0xF;


			data = *p2++;
			if( (data&0xf0) == 0xf0 ) data &= 0x0f;
			if( (data&0x0f) == 0x0f ) data &= 0xf0;
			*dest++ = data >> 4;
			*dest++ = data & 0xF;
			data = *p2++;
			if( (data&0xf0) == 0xf0 ) data &= 0x0f;
			if( (data&0x0f) == 0x0f ) data &= 0xf0;
			*dest++ = data >> 4;
			*dest++ = data & 0xF;
		} while( dest<finish );
	}
}

/* Unpack sprites data and do some patching */
DRIVER_INIT_MEMBER(wecleman_state,hotchase)
{
//  uint16_t *RAM1 = (uint16_t) memregion("maincpu")->base(); /* Main CPU patches */
//  RAM[0x1140/2] = 0x0015; RAM[0x195c/2] = 0x601A; // faster self test

	/* Now we can unpack each nibble of the sprites into a pixel (one byte) */
	hotchase_sprite_decode(3,0x80000*2);  // num banks, bank len

	m_spr_color_offs = 0;
	m_sound_hw_type = 1;
}


/***************************************************************************
                                Game driver(s)
***************************************************************************/

GAMEL( 1986, wecleman,  0,        wecleman, wecleman, wecleman_state, wecleman, ROT0, "Konami", "WEC Le Mans 24 (v2.00, set 1)", 0, layout_wecleman )
GAMEL( 1986, weclemana, wecleman, wecleman, wecleman, wecleman_state, wecleman, ROT0, "Konami", "WEC Le Mans 24 (v2.00, set 2)", 0, layout_wecleman ) // 1988 release (maybe date hacked?)
GAMEL( 1986, weclemanb, wecleman, wecleman, wecleman, wecleman_state, wecleman, ROT0, "Konami", "WEC Le Mans 24 (v1.26)", 0, layout_wecleman )
// a version 1.21 is known to exist too, see https://www.youtube.com/watch?v=4l8vYJi1OeU

GAMEL( 1988, hotchase,  0,        hotchase, hotchase, wecleman_state, hotchase, ROT0, "Konami", "Hot Chase (set 1)", 0, layout_wecleman )
GAMEL( 1988, hotchasea, hotchase, hotchase, hotchase, wecleman_state, hotchase, ROT0, "Konami", "Hot Chase (set 2)", 0, layout_wecleman )
