#include "lv_draw_sw.h"
#include "lv_gpu_d2_ra6m3.h"
#include "hal_data.h"

#define DRV_DEBUG
#define DBG_TAG              "drv.gpu"
#ifdef DRV_DEBUG
    #define DBG_LVL         DBG_LOG
#else
    #define DBG_LVL         DBG_INFO
#endif /* DRV_DEBUG */
#include <rtdbg.h>

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define M_PI       3.1415926

#define LOG_ERRORS
#ifdef LOG_ERRORS
    #define STRINGIFY(x)                #x
    #define TOSTRING(x)                 STRINGIFY(x)

    #define ERROR_LIST_SIZE             (4)
    #define D2_EXEC(a)                  lv_port_gpu_log_error(a, __func__,  __LINE__)
#else
    /* here is error logging not enabled */
    #define D2_EXEC(a)                  a;
#endif

typedef struct
{
    d2_s32 error;
    const char *func;
    int line;
} log_error_entry;

#ifdef LOG_ERRORS
    static log_error_entry log_error_list[ERROR_LIST_SIZE];
    static int error_list_index;
    static int error_count;
#endif

#if LV_COLOR_SCREEN_TRANSP == 0
#define FILL_NORMAL_MASK_PX(color)                                                  \
    if(*mask == LV_OPA_COVER) *disp_buf_first = color;                              \
    else *disp_buf_first = lv_color_mix(color, *disp_buf_first, *mask);             \
    mask++;                                                                         \
    disp_buf_first++;

#else
#define FILL_NORMAL_MASK_PX(color)                                                  \
    if(*mask == LV_OPA_COVER) *disp_buf_first = color;                              \
    else if(disp->driver->screen_transp) lv_color_mix_with_alpha(*disp_buf_first, disp_buf_first->ch.alpha, color, *mask, disp_buf_first, &disp_buf_first->ch.alpha);   \
    else *disp_buf_first = lv_color_mix(color, *disp_buf_first, *mask);             \
    mask++;                                                                         \
    disp_buf_first++;
#endif

#ifdef LOG_ERRORS
    static void lv_port_gpu_log_error(d2_s32 status, const char *func, int line);
#endif

static d2_device *_d2_handle;
static d2_renderbuffer *renderbuffer;

static d2_s32 src_cf_val, dst_cf_val;
static lv_draw_img_dsc_t img_dsc;
static bool color_key_enabled, alpha_enabled, blend_enabled, colorize_enabled;

static d2_s32 lv_port_gpu_cf_lv_to_d2(lv_img_cf_t cf)
{
    d2_s32 d2_cf;

#if (DLG_LVGL_CF == 1)
    switch (cf & ~(1 << 5))
    {
#else
    switch (cf)
    {
#endif /* (DLG_LVGL_CF == 1) */
    case LV_IMG_CF_TRUE_COLOR:
        d2_cf = d2_mode_rgb565;
        break;
    case LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED:
        d2_cf = d2_mode_rgb565;
        break;
    case LV_IMG_CF_ALPHA_1BIT:
        d2_cf = d2_mode_alpha1;
        break;
    case LV_IMG_CF_ALPHA_2BIT:
        d2_cf = d2_mode_alpha2;
        break;
    case LV_IMG_CF_ALPHA_4BIT:
        d2_cf = d2_mode_alpha4;
        break;
    case LV_IMG_CF_ALPHA_8BIT:
        d2_cf = d2_mode_alpha8;
        break;
    case LV_IMG_CF_INDEXED_1BIT:
        d2_cf = d2_mode_i1 | d2_mode_clut;
        break;
    case LV_IMG_CF_INDEXED_2BIT:
        d2_cf = d2_mode_i2 | d2_mode_clut;
        break;
    case LV_IMG_CF_INDEXED_4BIT:
        d2_cf = d2_mode_i4 | d2_mode_clut;
        break;
    case LV_IMG_CF_INDEXED_8BIT:
        d2_cf = d2_mode_i8 | d2_mode_clut;
        break;
#if (DLG_LVGL_CF == 1)
    case LV_IMG_CF_RGB565:
        d2_cf = d2_mode_rgb565;
        break;
    case LV_IMG_CF_RGB888:
        d2_cf = d2_mode_rgb888;
        break;
    case LV_IMG_CF_RGBA8888:
        d2_cf = d2_mode_rgba8888;
        break;
#endif /* DLG_LVGL_CF */
    default:
        return -1;
    }

#if (DLG_LVGL_CF == 1)
    return d2_cf | (cf & (1 << 5) ? d2_mode_rle : 0);
#else
    return d2_cf;
#endif /* (DLG_LVGL_CF == 1) */
}

static bool lv_port_gpu_cf_fb_valid(d2_s32 cf)
{
    if ((cf & (d2_mode_rle | d2_mode_clut)) || cf < 0)
    {
        return false;
    }

    switch (cf)
    {
    case d2_mode_alpha8:
    case d2_mode_rgb565:
    case d2_mode_argb8888:
    case d2_mode_argb4444:
    case d2_mode_rgba8888:
    case d2_mode_rgba4444:
        return true;
    default:
        return false;
    }
}

static bool lv_port_gpu_cf_has_alpha(d2_s32 cf)
{
    switch (cf & ~(d2_mode_clut | d2_mode_rle))
    {
    case d2_mode_argb8888:
    case d2_mode_rgba8888:
    case d2_mode_argb4444:
    case d2_mode_rgba4444:
    case d2_mode_argb1555:
    case d2_mode_rgba5551:
    case d2_mode_ai44:
    case d2_mode_i8:
    case d2_mode_i4:
    case d2_mode_i2:
    case d2_mode_i1:
    case d2_mode_alpha8:
    case d2_mode_alpha4:
    case d2_mode_alpha2:
    case d2_mode_alpha1:
        return true;
    default:
        return false;
    }
}

static bool lv_port_gpu_cf_is_alpha(d2_s32 cf)
{
    switch (cf & ~d2_mode_rle)
    {
    case d2_mode_alpha8:
    case d2_mode_alpha4:
    case d2_mode_alpha2:
    case d2_mode_alpha1:
        return true;
    default:
        return false;
    }
}

static d2_color lv_port_gpu_color_lv_to_d2(lv_color_t color)
{
    uint8_t alpha, red, green, blue;

    alpha = 0xFF;
    red = color.ch.red << 3 | color.ch.red >> 2;
    green = color.ch.green << 2 | color.ch.green >> 4;
    blue = color.ch.blue << 3 | color.ch.blue >> 2;

    return (alpha) << 24UL
           | (red) << 0UL
           | (green) << 8UL
           | (blue) << 16UL;
}

static void lv_port_gpu_get_recolor_consts(d2_color *cl, d2_color *ch)
{
    d2_color c = lv_port_gpu_color_lv_to_d2(img_dsc.recolor);
    d2_alpha r, g, b, opa = img_dsc.recolor_opa > LV_OPA_MAX ? LV_OPA_COVER : img_dsc.recolor_opa;

    r = ((uint32_t)((uint8_t)(c >> 0UL)) * opa) / 255;
    g = ((uint32_t)((uint8_t)(c >> 8UL)) * opa) / 255;
    b = ((uint32_t)((uint8_t)(c >> 16UL)) * opa) / 255;
    *cl = r << 0UL | g << 8UL | b << 16UL;

    r += 255 - opa;
    g += 255 - opa;
    b += 255 - opa;
    *ch = r << 0UL | g << 8UL | b << 16UL;
}

static int lv_port_gpu_handle_indexed_color(const lv_color_t **src, const d2_color **clut, d2_s32 cf)
{
    int clut_len = 0;

    if (cf & d2_mode_clut)
    {
        /* Calculate CLUT length in entries */
        switch (cf & ~(d2_mode_clut | d2_mode_rle))
        {
        case d2_mode_i1:
            clut_len = 2;
            break;
        case d2_mode_i2:
            clut_len = 4;
            break;
        case d2_mode_i4:
            clut_len = 16;
            break;
        case d2_mode_i8:
            clut_len = 256;
            break;
        case d2_mode_ai44:
            clut_len = 16;
            break;
        default:
            return 0;
        }

        *clut = (const d2_color *)*src;
        *src = (const lv_color_t *)((const uint32_t *)*src + clut_len);
    }
    return clut_len;
}

static int lv_port_gpu_cf_bpp(d2_s32 cf)
{
    switch (cf & ~(d2_mode_clut | d2_mode_rle))
    {
    case d2_mode_argb8888:
        return 32;
    case d2_mode_rgba8888:
        return 32;
    case d2_mode_rgb888:
        return 32;
    case d2_mode_argb4444:
        return 16;
    case d2_mode_rgba4444:
        return 16;
    case d2_mode_argb1555:
        return 16;
    case d2_mode_rgba5551:
        return 16;
    case d2_mode_rgb565:
        return 16;
    case d2_mode_ai44:
        return 8;
    case d2_mode_i8:
        return 8;
    case d2_mode_i4:
        return 4;
    case d2_mode_i2:
        return 2;
    case d2_mode_i1:
        return 1;
    case d2_mode_alpha8:
        return 8;
    case d2_mode_alpha4:
        return 4;
    case d2_mode_alpha2:
        return 2;
    case d2_mode_alpha1:
        return 1;
    default:
        return 0;
    }
}

static d2_s32 lv_port_gpu_cf_get_default(void)
{
    return d2_mode_rgb565;
}

static void lv_port_gpu_config_blit_clear(void)
{
        alpha_enabled = false;
        color_key_enabled = false;
        blend_enabled = true;
        colorize_enabled = false;

        lv_draw_img_dsc_init(&img_dsc);

        src_cf_val = lv_port_gpu_cf_get_default();
        dst_cf_val = lv_port_gpu_cf_get_default();
}

void lv_port_gpu_init(void)
{
    lv_port_gpu_config_blit_clear();
}

static void lv_port_gpu_rotate_point(int *x, int *y, float cos_angle, float sin_angle, int pivot_x, int pivot_y)
{
    float fx, fy;

    *x -= pivot_x;
    *y -= pivot_y;

    fx = ((float) * x) / 16.0f;
    fy = ((float) * y) / 16.0f;

    *x = (int)(((fx * cos_angle) - (fy * sin_angle)) * 16.0f);
    *y = (int)(((fx * sin_angle) + (fy * cos_angle)) * 16.0f);

    *x += pivot_x;
    *y += pivot_y;
}

static int lv_port_gpu_hw_init(void)
{
    if (_d2_handle != NULL)
    {
        return RT_ERROR;
    }

    _d2_handle = d2_opendevice(0);

    if (_d2_handle == NULL)
        return RT_ERROR;

    /* set blocksize for default displaylist */
    if (d2_setdlistblocksize(_d2_handle, 25) != D2_OK)
    {
        LOG_E("Could NOT d2_setdlistblocksize\n");
        d2_closedevice(_d2_handle);

        return RT_ERROR;
    }

    /* bind the hardware */
    if (d2_inithw(_d2_handle, 0) != D2_OK)
    {
        LOG_E("Could NOT d2_inithw\n");
        d2_closedevice(_d2_handle);

        return RT_ERROR;
    }

    renderbuffer = d2_newrenderbuffer(_d2_handle, 20, 20);
    if (!renderbuffer)
    {
        LOG_E("NO renderbuffer\n");
        d2_closedevice(_d2_handle);

        return RT_ERROR;
    }

    return RT_EOK;
}

static void lv_port_gpu_hw_deinit(void)
{
    if (_d2_handle == NULL)
            return;

    D2_EXEC(d2_freerenderbuffer(_d2_handle, renderbuffer));

    D2_EXEC(d2_closedevice(_d2_handle));

    renderbuffer = NULL;
    _d2_handle = NULL;
}

void lv_port_gpu_flush(void)
{
    lv_port_gpu_hw_deinit();
}

static void lv_port_gpu_start_render(void)
{
    lv_port_gpu_hw_init();

    D2_EXEC(d2_selectrenderbuffer(_d2_handle, renderbuffer));
}

static void lv_port_gpu_complete_render(void)
{
    D2_EXEC(d2_flushframe(_d2_handle));
}

static void lv_port_gpu_execute_render(void)
{
    if (_d2_handle)
    {
        D2_EXEC(d2_executerenderbuffer(_d2_handle, renderbuffer, 0));
    }
}

void lv_port_gpu_wait(lv_disp_drv_t *disp_drv)
{
    lv_port_gpu_complete_render();
}

void lv_port_gpu_fill(lv_color_t *dest_buf, lv_coord_t dst_width,
                      const lv_area_t *fill_area, lv_color_t color, lv_opa_t opa)
{
    lv_port_gpu_start_render();

    D2_EXEC(d2_framebuffer(_d2_handle, &fb_background[0], MAX(dst_width, 2), MAX(dst_width, 2),
                           MAX(fill_area->y2 + 1, 2), d2_mode_rgb565));
    D2_EXEC(d2_cliprect(_d2_handle, 0, 0, dst_width - 1, fill_area->y2));

    D2_EXEC(d2_setalpha(_d2_handle, opa > LV_OPA_MAX ? 0xFF : opa));
    D2_EXEC(d2_setcolor(_d2_handle, 0, lv_port_gpu_color_lv_to_d2(color)));
    D2_EXEC(d2_renderbox(_d2_handle, D2_FIX4(fill_area->x1), D2_FIX4(fill_area->y1),
                         D2_FIX4(lv_area_get_width(fill_area)), D2_FIX4(lv_area_get_height(fill_area))));

    lv_port_gpu_execute_render();
}

bool lv_port_gpu_config_blit(lv_disp_drv_t *disp_drv, const lv_draw_img_dsc_t *draw_dsc,  lv_img_cf_t dst_cf,
                             lv_img_cf_t src_cf, bool alpha_en, bool color_key_en, bool blend_en, bool colorize_en)
{
    d2_s32 d2_src_cf, d2_dst_cf;

    if (blend_en && draw_dsc->blend_mode != LV_BLEND_MODE_NORMAL
            && draw_dsc->blend_mode != LV_BLEND_MODE_ADDITIVE)
    {
        return false;
    }

    d2_src_cf = lv_port_gpu_cf_lv_to_d2(src_cf);
    d2_dst_cf = lv_port_gpu_cf_lv_to_d2(dst_cf);
    if (d2_src_cf < 0 || !lv_port_gpu_cf_fb_valid(d2_dst_cf))
    {
        return false;
    }
    src_cf_val = d2_src_cf;
    dst_cf_val = d2_dst_cf;

    img_dsc = *draw_dsc;

    /* Disable alpha if alpha channel does not exist */
    alpha_enabled = lv_port_gpu_cf_has_alpha(src_cf_val) ? alpha_en : 0;
    color_key_enabled = color_key_en;
    blend_enabled = blend_en;
    colorize_enabled = colorize_en | lv_port_gpu_cf_is_alpha(src_cf_val);

    return true;
}

static void lv_port_gpu_blit_internal(lv_disp_drv_t *disp_drv, const lv_area_t *dest_area, const lv_color_t *src_buf,
                                      const lv_area_t *src_area, d2_u32 flags)
{
    const lv_area_t *img_area = src_area;
    lv_area_t img_area_scaled;
    lv_coord_t w, h, img_w, img_h;
    d2_s32 pitch;
    int bpp = lv_port_gpu_cf_bpp(src_cf_val);

    D2_EXEC(d2_cliprect(_d2_handle, dest_area->x1, dest_area->y1, dest_area->x2, dest_area->y2));

    pitch = w = lv_area_get_width(src_area);
    h = lv_area_get_height(src_area);

    if (img_dsc.zoom != LV_IMG_ZOOM_NONE)
    {
        img_area_scaled.x1 = src_area->x1 + ((((int32_t)0 - img_dsc.pivot.x) * img_dsc.zoom) >> 8) + img_dsc.pivot.x;
        img_area_scaled.x2 = src_area->x1 + ((((int32_t)w - img_dsc.pivot.x) * img_dsc.zoom) >> 8) + img_dsc.pivot.x;
        img_area_scaled.y1 = src_area->y1 + ((((int32_t)0 - img_dsc.pivot.y) * img_dsc.zoom) >> 8) + img_dsc.pivot.y;
        img_area_scaled.y2 = src_area->y1 + ((((int32_t)h - img_dsc.pivot.y) * img_dsc.zoom) >> 8) + img_dsc.pivot.y;
        img_area = &img_area_scaled;
    }

    img_w = lv_area_get_width(img_area);
    img_h = lv_area_get_height(img_area);


    if (0 < bpp && bpp < 8)
    {
        pitch = (w + (8 - bpp)) & (~(8 - bpp));
    }

    if (img_dsc.angle == 0)
    {
        D2_EXEC(d2_setblitsrc(_d2_handle, (void *) src_buf, pitch, w, h, src_cf_val));

        D2_EXEC(d2_blitcopy(_d2_handle, w, h, 0, 0,
                            D2_FIX4(img_w), D2_FIX4(img_h), D2_FIX4(img_area->x1), D2_FIX4(img_area->y1), flags));
    }
    else
    {
        int x, y, x1, y1, x2, y2, x3, y3, x4, y4, dxu, dxv, dyu, dyv, xx, xy, yx, yy;
        int pivot_scaled_x, pivot_scaled_y;
        int tex_offset = (flags & d2_bf_filter) ? -32767 : 0;
        d2_u8 amode, cmode = d2_to_copy;
        float angle = ((float)img_dsc.angle / 10) * M_PI / 180;
        float cos_angle = cosf(angle);
        float sin_angle = sinf(angle);
        d2_u8 fillmode_backup;

        /* setup texture params */
        fillmode_backup = d2_getfillmode(_d2_handle);
        D2_EXEC(d2_setfillmode(_d2_handle, d2_fm_texture));
        D2_EXEC(d2_settexture(_d2_handle, (void *) src_buf, pitch, w, h, src_cf_val));
        D2_EXEC(d2_settexturemode(_d2_handle, flags & (d2_bf_filter | d2_bf_wrap)));
        amode = flags & d2_bf_usealpha ? d2_to_copy : d2_to_one;
        cmode = flags & d2_bf_colorize2 ? d2_to_blend : d2_to_copy;
        D2_EXEC(d2_settextureoperation(_d2_handle, amode, cmode, cmode, cmode));
        if (flags & d2_bf_colorize2)
        {
            d2_color cl = d2_getcolor(_d2_handle, 0), ch = d2_getcolor(_d2_handle, 1);
            D2_EXEC(d2_settexopparam(_d2_handle, d2_cc_red, (uint8_t)(cl >> 0UL),
                                     (uint8_t)(ch >> 0UL)));
            D2_EXEC(d2_settexopparam(_d2_handle, d2_cc_green, (uint8_t)(cl >> 8UL),
                                     (uint8_t)(ch >> 8UL)));
            D2_EXEC(d2_settexopparam(_d2_handle, d2_cc_blue, (uint8_t)(cl >> 16UL),
                                     (uint8_t)(ch >> 16UL)));
        }

        x = D2_FIX4(img_area->x1);
        y = D2_FIX4(img_area->y1);

        /* define quad points */
        x1 = D2_FIX4(0);
        y1 = D2_FIX4(0);
        x2 = D2_FIX4(img_w);
        y2 = D2_FIX4(0);
        x3 = D2_FIX4(img_w);
        y3 = D2_FIX4(img_h);
        x4 = D2_FIX4(0);
        y4 = D2_FIX4(img_h);

        /* rotate points for quad */
        pivot_scaled_x = (img_dsc.pivot.x * img_dsc.zoom) >> 4;
        pivot_scaled_y = (img_dsc.pivot.y * img_dsc.zoom) >> 4;

        lv_port_gpu_rotate_point(&x1, &y1, cos_angle, sin_angle, pivot_scaled_x, pivot_scaled_y);
        lv_port_gpu_rotate_point(&x2, &y2, cos_angle, sin_angle, pivot_scaled_x, pivot_scaled_y);
        lv_port_gpu_rotate_point(&x3, &y3, cos_angle, sin_angle, pivot_scaled_x, pivot_scaled_y);
        lv_port_gpu_rotate_point(&x4, &y4, cos_angle, sin_angle, pivot_scaled_x, pivot_scaled_y);

        /* compute texture increments */
        xx = (int)(cos_angle * 65536.0f);
        xy = (int)(sin_angle * 65536.0f);
        yx = (int)(-sin_angle * 65536.0f);
        yy = (int)(cos_angle * 65536.0f);
        dxu = ((D2_FIX16(w) / D2_FIX4(img_w)) * xx) >> 12;
        dxv = ((D2_FIX16(w) / D2_FIX4(img_w)) * xy) >> 12;
        dyu = ((D2_FIX16(h) / D2_FIX4(img_h)) * yx) >> 12;
        dyv = ((D2_FIX16(h) / D2_FIX4(img_h)) * yy) >> 12;

        /* map texture exactly to rotated quad, so texel center is always (0/0) top-left */
        D2_EXEC(d2_settexelcenter(_d2_handle, 0, 0));
        D2_EXEC(d2_settexturemapping(_d2_handle, (d2_point)(x + x1), (d2_point)(y + y1),
                                     tex_offset, tex_offset, dxu, dxv, dyu, dyv));

        int minx = MAX(dest_area->x1, D2_INT4(x + MIN(x1, MIN(x2, MIN(x3, x4)))));
        int maxx = MIN(dest_area->x2, D2_INT4(x + MAX(x1, MAX(x2, MAX(x3, x4)))));
        int slice = (flags & d2_bf_filter) ? 6 : 8;

        /* Perform render operation in slices to acheive better performance */
        for (int posx = minx; posx < maxx; posx += slice)
        {
            D2_EXEC(d2_cliprect(_d2_handle, posx, dest_area->y1, MIN(posx + slice - 1, maxx), dest_area->y2));
            D2_EXEC(d2_renderquad(_d2_handle, (d2_point)(x + x1), (d2_point)(y + y1),
                                  (d2_point)(x + x2), (d2_point)(y + y2),
                                  (d2_point)(x + x3), (d2_point)(y + y3),
                                  (d2_point)(x + x4), (d2_point)(y + y4), 0));
        }
        D2_EXEC(d2_setfillmode(_d2_handle, fillmode_backup));
    }
}

void lv_port_ra_gpu_blit(lv_disp_drv_t *disp_drv, lv_color_t *dst, const lv_area_t *dst_area, lv_coord_t dest_stride,
                      const lv_color_t *src, const lv_area_t *src_area, lv_opa_t opa)
{
    d2_u32 flags = 0;
    const d2_color *clut = NULL;
    int clut_len = 0;

    clut_len = lv_port_gpu_handle_indexed_color(&src, &clut, src_cf_val);

    lv_port_gpu_start_render();

    D2_EXEC(d2_framebuffer(_d2_handle, &fb_background[0], MAX(dest_stride, 2),
                           MAX(dst_area->x2 + 1, 2), MAX(dst_area->y2 + 1, 2), dst_cf_val));

    flags |= alpha_enabled ? d2_bf_usealpha : 0;

    D2_EXEC(d2_setalpha(_d2_handle, opa > LV_OPA_MAX ? LV_OPA_COVER : opa));

    if (clut)
    {
        D2_EXEC(d2_writetexclut_direct(_d2_handle, clut, 0, clut_len));
    }

    flags |= color_key_enabled ? d2_bf_usealpha : 0;
    D2_EXEC(d2_setcolorkey(_d2_handle, color_key_enabled, lv_port_gpu_color_lv_to_d2(disp_drv->color_chroma_key)));

    flags |= (colorize_enabled || img_dsc.recolor_opa != LV_OPA_TRANSP) ? d2_bf_colorize2 : 0;
    if (colorize_enabled)
    {
        D2_EXEC(d2_setcolor(_d2_handle, 0, lv_port_gpu_color_lv_to_d2(img_dsc.recolor)));
        D2_EXEC(d2_setcolor(_d2_handle, 1, lv_port_gpu_color_lv_to_d2(img_dsc.recolor)));
    }
    else if (img_dsc.recolor_opa != LV_OPA_TRANSP)
    {
        d2_color cl = 0, ch = 0;
        lv_port_gpu_get_recolor_consts(&cl, &ch);
        D2_EXEC(d2_setcolor(_d2_handle, 0, cl));
        D2_EXEC(d2_setcolor(_d2_handle, 1, ch));
    }

    flags |= ((img_dsc.angle || img_dsc.zoom != LV_IMG_ZOOM_NONE) && img_dsc.antialias) ? d2_bf_filter : 0;

    if (blend_enabled)
    {
        D2_EXEC(d2_setblendmode(_d2_handle, d2_bm_alpha,
                                img_dsc.blend_mode != LV_BLEND_MODE_NORMAL ? d2_bm_one : d2_bm_one_minus_alpha));
        D2_EXEC(d2_setalphablendmode(_d2_handle, d2_bm_one, d2_bm_one_minus_alpha));
    }
    else
    {
        D2_EXEC(d2_setblendmode(_d2_handle, d2_bm_one, d2_bm_zero));
        D2_EXEC(d2_setalphablendmode(_d2_handle, d2_bm_one, d2_bm_zero));
    }

    lv_port_gpu_blit_internal(disp_drv, dst_area, src, src_area, flags);

    lv_port_gpu_execute_render();
}

LV_ATTRIBUTE_FAST_MEM static void fill_normal(const lv_area_t *disp_area, lv_color_t *disp_buf,
                            const lv_area_t *draw_area, lv_color_t color, lv_opa_t opa,
                            const lv_opa_t *mask, lv_draw_mask_res_t mask_res)
{

    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    uint32_t hor_res = (uint32_t) lv_disp_get_hor_res(disp);

    /*Get the width of the `disp_area` it will be used to go to the next line*/
    int32_t disp_w = lv_area_get_width(disp_area);

    /*Simple fill (maybe with opacity), no masking*/
    if (mask_res == LV_DRAW_MASK_RES_FULL_COVER)
    {
        lv_port_gpu_fill(disp_buf, disp_w, draw_area, color, opa);
        return;
    }
    /*Masked*/
    else if (lv_area_get_size(draw_area) > hor_res)
    {
        lv_draw_img_dsc_t dsc;
        lv_draw_img_dsc_init(&dsc);
        dsc.recolor = color;

        if (lv_port_gpu_config_blit(disp->driver, &dsc, LV_IMG_CF_TRUE_COLOR, LV_IMG_CF_ALPHA_8BIT, true, false, true, true))
        {
            lv_port_gpu_wait(disp->driver);
            lv_port_ra_gpu_blit(disp->driver, disp_buf, draw_area, disp_w, (lv_color_t *)mask, draw_area, opa);
            return;
        }
    }
}

LV_ATTRIBUTE_FAST_MEM void lv_draw_ra6m3_2d_blend(lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc)
{
    lv_area_t blend_area;

    /*Do not draw transparent things*/
    if (dsc->opa < LV_OPA_MIN) return;
    if (dsc->mask_res == LV_DRAW_MASK_RES_TRANSP) return;
    
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    lv_port_gpu_wait(disp->driver);

    /*Let's get the blend area which is the intersection of the area to fill and the clip area.*/
    if(!_lv_area_intersect(&blend_area, dsc->blend_area, draw_ctx->clip_area)) return; /*Fully clipped, nothing to do*/
    
    bool done = false;

    if(dsc->mask_buf == NULL && dsc->blend_mode == LV_BLEND_MODE_NORMAL && lv_area_get_size(&blend_area) > 100) 
    {
        lv_coord_t dest_stride = lv_area_get_width(draw_ctx->buf_area);

        lv_color_t *dest_buf = draw_ctx->buf;
        dest_buf += dest_stride * (blend_area.y1 - draw_ctx->buf_area->y1) + (blend_area.x1 - draw_ctx->buf_area->x1);
        
        const lv_color_t *src_buf = dsc->src_buf;
        
        if(src_buf) {
            lv_draw_sw_blend_basic(draw_ctx, dsc);
            lv_coord_t src_stride;
            src_stride = lv_area_get_width(dsc->blend_area);
            src_buf += src_stride * (blend_area.y1 - dsc->blend_area->y1) + (blend_area.x1 - dsc->blend_area->x1);
            lv_area_move(&blend_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);
            
            done = true;
        }
        else if(dsc->opa >= LV_OPA_MAX) {
            lv_area_move(&blend_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);
            lv_port_gpu_fill(dest_buf, dest_stride, &blend_area, dsc->color, dsc->opa);
//            fill_normal(&blend_area, dest_buf, &blend_area, dsc->color, dsc->opa, dsc->mask_buf, dsc->mask_res);
            done = true;
        }
    }
}

LV_ATTRIBUTE_FAST_MEM void lv_draw_ra6m3_2d_blend_map(lv_color_t *dest_buf, const lv_area_t *dest_area, lv_coord_t dest_stride,
                                                      const lv_color_t *src_buf, lv_coord_t src_stride, lv_opa_t opa)
{
    
}

void lv_draw_ra6m3_2d_ctx_init(lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx)
{
    lv_draw_sw_init_ctx(drv, draw_ctx);

    lv_draw_ra6m3_dma2d_ctx_t * ra_2d_draw_ctx = (lv_draw_sw_ctx_t *)draw_ctx;

    ra_2d_draw_ctx->blend = lv_draw_ra6m3_2d_blend;
    //    dma2d_draw_ctx->base_draw.draw_img_decoded = lv_draw_ra6m3_2d_img_decoded;
    //ra_2d_draw_ctx->base_draw.wait_for_finish = lv_draw_ra6m3_2d_wait_cb;
    //ra_2d_draw_ctx->base_draw.buffer_copy = lv_draw_ra6m3_2d_buffer_copy;

}

void lv_draw_ra6m3_2d_ctx_deinit(lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx)
{
    LV_UNUSED(drv);
    LV_UNUSED(draw_ctx);
}

#ifdef LOG_ERRORS
static void lv_port_gpu_log_error(d2_s32 status, const char *func, int line)
{
    if (status)
    {
        log_error_list[error_list_index].error = status;
        log_error_list[error_list_index].func = func;
        log_error_list[error_list_index].line = line;
        rt_kprintf("%s\r\n", d2_geterrorstring(_d2_handle));
        rt_kprintf("%d:\t%d - %s : %d\r\n", error_count,
                   log_error_list[error_list_index].error,
                   log_error_list[error_list_index].func,
                   log_error_list[error_list_index].line);

        error_count++;
        error_list_index++;
        if (error_list_index >= ERROR_LIST_SIZE)
        {
            error_list_index = 0;
        }
    }
}
#endif /* LOG_ERRORS */