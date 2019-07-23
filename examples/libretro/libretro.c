#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#define AGNES_IMPLEMENTATION
#include "../../agnes.h"

#include "libretro.h"

static uint32_t *frame_buf;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
agnes_t *agnes;

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

void retro_init(void)
{
   frame_buf = calloc(AGNES_SCREEN_WIDTH * AGNES_SCREEN_HEIGHT, sizeof(uint32_t));
   agnes = agnes_make();
}

void retro_deinit(void)
{
   free(frame_buf);
   frame_buf = NULL;
   agnes_destroy(agnes);
   agnes = NULL;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   log_cb(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "agnes";
   info->library_version  = "0.0.1";
   info->need_fullpath    = false;
   info->valid_extensions = "nes";
}

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   float aspect = (float)AGNES_SCREEN_WIDTH / (float)AGNES_SCREEN_HEIGHT;

   info->timing = (struct retro_system_timing) {
      .fps = 60.0,
      .sample_rate = 0.0,
   };

   info->geometry = (struct retro_game_geometry) {
      .base_width   = AGNES_SCREEN_WIDTH,
      .base_height  = AGNES_SCREEN_HEIGHT,
      .max_width    = AGNES_SCREEN_WIDTH,
      .max_height   = AGNES_SCREEN_HEIGHT,
      .aspect_ratio = aspect,
   };
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   bool no_content = false;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      log_cb = logging.log;
   else
      log_cb = fallback_log;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_reset(void)
{
   // Nothing.
}

static void agnes_libretro_input_gamepad(agnes_input_t* gamepad, int index) {
   gamepad->up = input_state_cb(index, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
   gamepad->down = input_state_cb(index, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
   gamepad->left = input_state_cb(index, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
   gamepad->right = input_state_cb(index, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
   gamepad->a = input_state_cb(index, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
   gamepad->b = input_state_cb(index, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
   gamepad->start = input_state_cb(index, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
   gamepad->select = input_state_cb(index, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);
}

static void update_input(void)
{
   input_poll_cb();

   agnes_input_t input1, input2;
   agnes_libretro_input_gamepad(&input1, 0);
   agnes_libretro_input_gamepad(&input2, 1);
   agnes_set_input(agnes, &input1, &input2);
}

static void render_checkered(void)
{
   uint32_t *buf    = frame_buf;
   unsigned stride  = AGNES_SCREEN_WIDTH;
   agnes_color_t c;

   for (unsigned y = 0; y < AGNES_SCREEN_HEIGHT; y++) {
      for (unsigned x = 0; x < AGNES_SCREEN_WIDTH; x++) {
         c = agnes_get_screen_pixel(agnes, x, y);
         buf[y * stride + x] = ((c.r & 0xff) << 24) + ((c.g & 0xff) << 16) + ((c.b & 0xff) << 8) + (c.a & 0xff);
      }
   }

   video_cb(buf, AGNES_SCREEN_WIDTH, AGNES_SCREEN_HEIGHT, stride << 2);
}

static void check_variables(void)
{
}

static void audio_callback(void)
{
   audio_cb(0, 0);
}

void retro_run(void)
{
   update_input();
   bool new;
   new = agnes_next_frame(agnes);
   if (new) {
      printf("%s\n", new ? "true": "false");
   }
   render_checkered();
   audio_callback();

   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
      check_variables();
   }
}

bool retro_load_game(const struct retro_game_info *info)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
      return false;
   }

   bool ok = agnes_load_ines_data(agnes, (void*)info->data, info->size);
   if (!ok) {
      printf("Loading game failed.\n");
      return false;
   }

   check_variables();
   return true;
}

void retro_unload_game(void)
{
   agnes_destroy(agnes);
   agnes = NULL;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   return retro_load_game(info);
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}
