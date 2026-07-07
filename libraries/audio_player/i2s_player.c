#include "i2s_configuration.h" // basic sysetm includes and pin setup
#include "driver/i2s_std.h"    // i2s setup
#include "lvgl.h"
#include <string.h>
#include "esp_heap_caps.h"

#define AUDIO_BUFFER 2048

static const char *TAG = "I2S Audio Player";

// Set true by playbackSound() (main.c) to abort the sound currently streaming so a newer
// press's sound can start immediately. Cleared by the audio task before each new playback.
extern volatile bool soundInterrupt;

i2s_chan_handle_t tx_handle;
i2s_std_slot_config_t slotConfig = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
bool i2s_playbackError = false;
// The I2S channel (DMA descriptors + clock config) is allocated once and reused across plays.
// Re-creating it per sound was a big chunk of the per-press audio latency.
static bool i2s_initialized = false;

// Preloaded (PSRAM-resident) WAVs so hot sounds (the arrows) play with no per-sound SD open — the
// file open + first read was the startup latency that made rapid presses feel like they queued.
// Keyed by the same path string passed to play_wav(); playback then streams straight from RAM.
#define MAX_PRELOAD 8
static struct
{
    const char *path;
    uint8_t *data;
    size_t len;
} preloads[MAX_PRELOAD];
static int preloadCount = 0;

// Software master volume: 0..256 (256 = unity/full). Set from the config slider (main.c).
extern volatile uint16_t audioVolume;
// Scratch for volume-scaled output so const (preloaded) source data isn't modified in place.
static int16_t volScratch[AUDIO_BUFFER / 2];

// Write PCM to I2S, applying the master volume. At unity the source is passed straight through.
static void i2s_write_scaled(const void *src, size_t bytes)
{
    size_t written = 0;
    uint16_t vol = audioVolume;

    if (vol >= 256)
    {
        i2s_channel_write(tx_handle, src, bytes, &written, portMAX_DELAY);
        return;
    }

    const int16_t *in = (const int16_t *)src;
    size_t samples = bytes / sizeof(int16_t);
    for (size_t i = 0; i < samples; i++)
    {
        volScratch[i] = (int16_t)(((int32_t)in[i] * vol) >> 8);
    }
    i2s_channel_write(tx_handle, volScratch, samples * sizeof(int16_t), &written, portMAX_DELAY);
}

esp_err_t i2s_setup(void)
{
    // setup a standard config and the channel
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    // setup the i2s config
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),                                                    // the wav file sample rate
        .slot_cfg = slotConfig, // the wav faile bit and channel config
        .gpio_cfg = {
            // refer to configuration.h for pin setup
            .mclk = AUDIO_I2S_MCK_IO,
            .bclk = AUDIO_I2S_BCK_IO,
            .ws = AUDIO_I2S_LRCK_IO,
            .dout = AUDIO_I2S_DO_IO,
            .din = 0,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    return i2s_channel_init_std_mode(tx_handle, &std_cfg);
}

// Stream an in-RAM WAV (44-byte header included) straight to I2S, skipping the header. Interruptible
// on the same soundInterrupt flag as file playback, so a newer press still cuts it.
static esp_err_t play_mem(uint8_t *data, size_t len)
{
    if (len <= 44)
    {
        return ESP_OK;
    }

    if (!i2s_initialized)
    {
        if (i2s_setup() != ESP_OK)
        {
            i2s_playbackError = true;
            return ESP_FAIL;
        }
        i2s_initialized = true;
    }
    i2s_channel_enable(tx_handle);

    size_t offset = 44;
    while (offset < len)
    {
        if (soundInterrupt)
        {
            break;
        }
        size_t chunk = len - offset;
        if (chunk > AUDIO_BUFFER)
        {
            chunk = AUDIO_BUFFER;
        }
        i2s_write_scaled(data + offset, chunk);
        offset += chunk;
    }

    i2s_channel_disable(tx_handle);
    return ESP_OK;
}

// Load an entire WAV into PSRAM once (at boot) so later play_wav(fp) calls with the same path play
// it from RAM with an instant start instead of opening the file on the SD card each time.
void preload_wav(char *fp)
{
    if (preloadCount >= MAX_PRELOAD)
    {
        return;
    }

    lv_fs_file_t f;
    if (lv_fs_open(&f, fp, LV_FS_MODE_RD) != LV_FS_RES_OK)
    {
        ESP_LOGE(TAG, "(%s) preload open failed.", fp);
        return;
    }

    lv_fs_seek(&f, 0, LV_FS_SEEK_END);
    uint32_t size = 0;
    lv_fs_tell(&f, &size);
    lv_fs_seek(&f, 0, LV_FS_SEEK_SET);

    if (size == 0)
    {
        lv_fs_close(&f);
        return;
    }

    uint8_t *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (buf == NULL)
    {
        ESP_LOGE(TAG, "(%s) preload alloc failed (%lu bytes).", fp, (unsigned long)size);
        lv_fs_close(&f);
        return;
    }

    uint32_t rd = 0;
    lv_fs_read(&f, buf, size, &rd);
    lv_fs_close(&f);

    preloads[preloadCount].path = fp;
    preloads[preloadCount].data = buf;
    preloads[preloadCount].len = rd;
    preloadCount++;
    ESP_LOGI(TAG, "(%s) preloaded %lu bytes to PSRAM.", fp, (unsigned long)rd);
}

esp_err_t play_wav(char *fp)
{
    if(i2s_playbackError){
        return ESP_ERR_INVALID_STATE;
    }

    // If this sound was preloaded, play it straight from PSRAM — no SD open, instant start.
    for (int i = 0; i < preloadCount; i++)
    {
        if (strcmp(preloads[i].path, fp) == 0)
        {
            return play_mem(preloads[i].data, preloads[i].len);
        }
    }

    lv_fs_file_t f;
    lv_fs_res_t res;

    res = lv_fs_open(&f, fp, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK)
    {
        // A missing/unreadable file just skips THIS sound — do NOT set i2s_playbackError (which is
        // permanent and would silence all future audio). Only real I2S hardware faults are fatal.
        ESP_LOGE(TAG, "(%s) FS open failed — skipping this sound.", fp);
        lv_fs_close(&f);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "(%s) FS opened.", fp);

    // skip the header...
    res = lv_fs_seek(&f, 44, LV_FS_SEEK_SET);
    if (res != LV_FS_RES_OK)
    {
        ESP_LOGE(TAG, "(%s) Seek failed — skipping this sound.", fp);
        lv_fs_close(&f);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "(%s) FS seeked.", fp);

    // create a writer buffer
    size_t chunkSize = sizeof(int8_t);
    uint16_t *buf = calloc(AUDIO_BUFFER, chunkSize);
    uint32_t bytes_read = 0;

    res = lv_fs_read(&f, buf, AUDIO_BUFFER, &bytes_read);
    if (res != LV_FS_RES_OK)
    {
        ESP_LOGE(TAG, "(%s) FS read failed — skipping this sound.", fp);
        free(buf);
        lv_fs_close(&f);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "FS read bytes: %lx", bytes_read);

    // Allocate + configure the channel only on the first play, then reuse it every time.
    if (!i2s_initialized)
    {
        if (i2s_setup() != ESP_OK)
        {
            ESP_LOGE(TAG, "I2S setup failed.");
            free(buf);
            lv_fs_close(&f);
            i2s_playbackError = true;
            return ESP_FAIL;
        }
        i2s_initialized = true;
    }
    i2s_channel_enable(tx_handle);

    while (bytes_read > 0)
    {
        // A newer sound was requested — stop early so it can start with minimal delay
        if (soundInterrupt)
        {
            break;
        }

        // write the buffer to the i2s
        i2s_write_scaled(buf, bytes_read * (uint32_t)chunkSize);
        lv_fs_read(&f, buf, AUDIO_BUFFER, &bytes_read);
    }

    i2s_channel_disable(tx_handle);
    // Keep the channel allocated for the next play (no i2s_del_channel here).
    free(buf);

    lv_fs_close(&f);

    return ESP_OK;
}