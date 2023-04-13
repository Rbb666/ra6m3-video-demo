#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <rthw.h>
#include <rtthread.h>

#include "avifile.h"
#include "drv_jpeg.h"

#include "hal_data.h"
#include "pwm_audio.h"

#include <dfs_file.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statfs.h>

#define USEING_AUDIO
/**
 * TODO: how to recognize each stream id
 */
#define T_vids _REV(0x30306463)
#define T_auds _REV(0x30317762)

extern AVI_TypeDef AVI_file;
extern rt_sem_t _SemaphoreVideo;

static uint32_t _REV(uint32_t value)
{
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 |
           (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24;
}

static uint32_t read_frame(int fd, uint8_t *buffer, uint32_t length, uint32_t *fourcc)
{
    AVI_CHUNK_HEAD head;
    read(fd, &head, sizeof(AVI_CHUNK_HEAD));
    if (head.FourCC)
    {
        /* code */
    }
    *fourcc = head.FourCC;
    if (head.size % 2)
    {
        head.size++; // ???1
    }
    if (length < head.size)
    {
        rt_kprintf("frame size too large\n");
        return 0;
    }
    
    read(fd, buffer, head.size);
    return head.size;
}

static void audio_init(void)
{
    pwm_audio_config_t pac;
    pac.duty_resolution    = 10;
    pac.gpio_num_left      = 1;
    pac.gpio_num_right     = -1;
    pac.ringbuf_len        = 1024 * 8;

    pwm_audio_init(&pac);
    
    // set default volume
    // volume:-16 -- +16
    pwm_audio_set_volume(-15);
}

int audio_volume_set(int argc, const char *argv[])
{
    if (argc != 2)
    {
        rt_kprintf("param not right!!\n");
        return RT_ERROR;
    }

    pwm_audio_set_volume(atoi(argv[1]));
    return RT_EOK;
}
MSH_CMD_EXPORT(audio_volume_set, audio_volume_set);

void video_entry(void *paramer)
{
    int ret;
    size_t BytesRD;
    uint32_t Strsize;
    uint32_t Strtype;
    uint8_t *pbuffer;
    uint32_t buffer_size = 30 * 1024;
    uint32_t alltime;
//    uint32_t cur_time;

    int fd = -1;
    fd = open(paramer, O_WRONLY);

    pbuffer = rt_malloc(buffer_size);
    if (pbuffer == NULL)
    {
        rt_kprintf("Cannot alloc memory for palyer\n");
        close(fd);
        return;
    }
    
    BytesRD = read(fd, pbuffer, 20480);    
    ret = AVI_Parser(pbuffer, BytesRD);
    if (0 > ret)
    {
        rt_kprintf("parse failed (%d)\n", ret);
        return;
    }

#ifdef USEING_AUDIO
    // Audio Init
    audio_init();
    pwm_audio_set_param(AVI_file.auds_sample_rate, AVI_file.auds_bits, AVI_file.auds_channels);
#endif

    uint16_t img_width = AVI_file.vids_width;
    uint16_t img_height = AVI_file.vids_height;
    rt_kprintf("img_width:%d, img_height:%d\n", img_width, img_height);

    // vido info
    alltime = (AVI_file.avi_hd.avih.us_per_frame / 1000) * AVI_file.avi_hd.avih.total_frames;
    alltime /= 1000; // s
    rt_kprintf("video total time:%02d:%02d:%02d\n", alltime / 3600, (alltime % 3600) / 60, alltime % 60);

    lseek(fd, AVI_file.movi_start, SEEK_SET);
    Strsize = read_frame(fd, pbuffer, buffer_size, &Strtype);
    BytesRD = Strsize + 8;

    static uint32_t last_frame = 0;
    last_frame = rt_tick_get();

    rt_kprintf("\npbuffer addr:0x%#x\n", pbuffer);

    while (1)
    {
        if (BytesRD >= AVI_file.movi_size)
        {
            rt_kprintf("play end\n");
            goto EXIT;
        }
        if (Strtype == T_vids)
        {
            extern int JPEG_X_Draw(void *p, int x0, int y0);
            JPEG_X_Draw(pbuffer, 0, 0);
        }
        else if (Strtype == T_auds)   //????
        {
            size_t cnt;
            pwm_audio_write((uint8_t *)pbuffer, Strsize, &cnt, 500);
        }
        else
        {
            rt_kprintf("unknow frame\n");
            break;
        }

        Strsize = read_frame(fd, pbuffer, buffer_size, &Strtype); // ????

        // rt_kprintf("type=%x, size=%d\n", Strtype, Strsize);
        BytesRD += Strsize + 8;
    }
EXIT:
    rt_free(pbuffer);
    close(fd);
}

int avi_player_init(int argc, const char *argv[])
{
    if (argc != 2)
    {
        rt_kprintf("please check inputer paramer\n");
        return RT_ERROR;
    }

    rt_thread_t tid = rt_thread_create("avi_td",
                                       video_entry,
                                       (char *)argv[1],
                                       1 * 1024,
                                       18, 10);
    if (tid)
        rt_thread_startup(tid);

    return RT_EOK;
}
