#include <nuttx/config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

#include "vela_pet_demo_input.h"
#include "vela_pet_service.h"
#include "velapet_ui.h"

#ifdef CONFIG_FS_LITTLEFS
/****************************************************************************
 * Name: vela_pet_mkfs
 *
 * Description:
 *   Format the NOR data partition.  Deliberately not done during boot: the
 *   board must stay bootable even when the partition cannot be mounted, and
 *   a formatting failure has to be visible.  Run "velapet mkfs" from NSH and
 *   reboot afterwards.
 *
 ****************************************************************************/

static int vela_pet_mkfs(void)
{
  int ret;

  printf("Formatting /dev/config0 as littlefs; this erases the whole data "
         "partition.\n");

  if (mkdir("/mnt", 0755) < 0 && errno != EEXIST)
    {
      printf("mkfs: mkdir /mnt failed: %d\n", errno);
      return 1;
    }

  ret = nx_mount("/dev/config0", "/mnt", "littlefs", 0, "forceformat");
  if (ret < 0)
    {
      printf("mkfs: format failed: %d\n", ret);
      return 1;
    }

  ret = nx_umount2("/mnt", 0);
  if (ret < 0)
    {
      printf("mkfs: umount failed: %d\n", ret);
      return 1;
    }

  printf("mkfs: done.  Reboot the board so /data mounts it.\n");
  return 0;
}
#endif

int main(int argc, char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;

  if (argc > 1 && strcmp(argv[1], "mkfs") == 0)
    {
#ifdef CONFIG_FS_LITTLEFS
      return vela_pet_mkfs();
#else
      printf("mkfs: littlefs support is not enabled in this build\n");
      return 1;
#endif
    }

  if (lv_is_initialized())
    {
      return -1;
    }

  lv_init();

  lv_nuttx_dsc_init(&info);

#ifdef CONFIG_LV_USE_NUTTX_LCD
  info.fb_path = "/dev/lcd0";
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
  info.input_path = "/dev/input0";
#endif

  lv_nuttx_init(&info, &result);
  if (result.disp == NULL)
    {
      return 1;
    }

  /* velapet_ui_init() calls vela_pet_init() itself. */

  if (velapet_ui_init() != 0)
    {
      lv_nuttx_deinit(&result);
      lv_deinit();
      return 1;
    }

  vela_pet_service_start();

#ifdef CONFIG_EXAMPLES_VELAPET_DEMO_INPUT
  /* Simulated step source so the growth loop can be demoed without hardware
   * step data.  See src/vela_pet_demo_input.c. */

  velapet_demo_input_start();
#endif

  for (;;)
    {
      uint32_t idle = lv_timer_handler();

      /* Minimum sleep of 1ms */

      idle = idle ? idle : 1;
      usleep(idle * 1000);
    }

  return 0;
}
