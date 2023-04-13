#include <rtthread.h>
#include <drv_spi.h>
#include "hal_data.h"

//#ifdef RT_USING_DFS
//#include <dfs_fs.h>
//#include <dfs_romfs.h>
//int mnt_init(void)
//{
//    rt_thread_mdelay(200);

//    if (dfs_mount(RT_NULL, "/", "rom", 0,  &(romfs_root)) != 0)
//    {
//        rt_kprintf("ROM file system initializate failed!\n");
//    }

//    rt_kprintf("ROM file system initializated!\n");
//    return 0;
//}
//INIT_ENV_EXPORT(mnt_init);
//#endif

#if defined(RT_USING_SPI_MSD) && defined(RT_USING_DFS_ELMFAT)
#include <spi_msd.h>
#include <dfs_fs.h>
int mnt_init(void)
{
    struct rt_spi_device *sd_dev;
    sd_dev = (struct rt_spi_device *)rt_malloc(sizeof(struct rt_spi_device));
    
    uint32_t cs_pin = BSP_IO_PORT_02_PIN_05;

    rt_hw_spi_device_attach(sd_dev, "spi10", "spi1", (void *)cs_pin);
    msd_init("sd0", "spi10");
    if (dfs_mount("sd0", "/", "elm", 0, 0) == 0)
    {
        rt_kprintf("Mount \"/dev/sd0\" on \"/\"\n");
    }
    return 0;
}
INIT_ENV_EXPORT(mnt_init);
#endif
