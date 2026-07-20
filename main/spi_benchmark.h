// Temporary instrumentation: measures effective pushSprite throughput.
// Call runSpiBenchmark() from app_main() before setup(). Remove when done.

#pragma once

#include <TFT_eSPI.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include "flash_asset.h"

static const char* SPIBENCH_TAG = "spi_bench";

// Push the same sprite repeatedly and report the achieved data rate.
// The effective clock is what the display actually sees, including the
// per-push setWindow() overhead (CASET/PASET/RAMWR), so it lands somewhat
// below the configured SPI_FREQUENCY.
static void benchOneSize(TFT_eSPI& tft, int w, int h, int repeats)
{
    TFT_eSprite spr = TFT_eSprite(&tft);

    if (spr.createSprite(w, h) == nullptr)
    {
        ESP_LOGW(SPIBENCH_TAG, "%3dx%3d : allocation FAILED (%d bytes needed, %d largest free block)",
                 w, h, 2 * w * h + 2,
                 (int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        return;
    }

    spr.fillSprite(TFT_BLUE);   // touch every pixel so nothing is optimised away

    const int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < repeats; i++) spr.pushSprite(0, 0);
    const int64_t elapsed = esp_timer_get_time() - t0;

    spr.deleteSprite();

    const int64_t bytes    = (int64_t)w * h * 2 * repeats;
    const double  seconds  = elapsed / 1e6;
    const double  mbPerSec = bytes / seconds / 1e6;

    ESP_LOGI(SPIBENCH_TAG, "%3dx%3d : %7.3f ms/push, %5.2f MB/s, %5.1f MHz effective",
             w, h, (elapsed / 1000.0) / repeats, mbPerSec, mbPerSec * 8);
}

// Same sizes, but pushed with the DMA engine. The sprite pixels are already in
// the display's byte order, so no swap is needed and the buffer can go straight
// to spi_device_queue_trans(). Pushes are queued back-to-back: the CPU only
// waits for the previous transfer when it needs the engine again, so this
// measures raw DMA throughput without any overlapping render work.
static void benchOneSizeDma(TFT_eSPI& tft, int w, int h, int repeats)
{
    TFT_eSprite spr = TFT_eSprite(&tft);

    uint16_t* ptr = (uint16_t*)spr.createSprite(w, h);
    if (ptr == nullptr)
    {
        ESP_LOGW(SPIBENCH_TAG, "%3dx%3d : DMA allocation FAILED (%d bytes needed, %d largest free block)",
                 w, h, 2 * w * h + 2,
                 (int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        return;
    }

    spr.fillSprite(TFT_GREEN);

    tft.startWrite();
    const int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < repeats; i++) tft.pushImageDMA(0, 0, w, h, ptr);
    tft.dmaWait();                      // include the last transfer in the timing
    const int64_t elapsed = esp_timer_get_time() - t0;
    tft.endWrite();

    spr.deleteSprite();

    const int64_t bytes    = (int64_t)w * h * 2 * repeats;
    const double  seconds  = elapsed / 1e6;
    const double  mbPerSec = bytes / seconds / 1e6;

    ESP_LOGI(SPIBENCH_TAG, "%3dx%3d : %7.3f ms/push, %5.2f MB/s, %5.1f MHz effective (DMA)",
             w, h, (elapsed / 1000.0) / repeats, mbPerSec, mbPerSec * 8);
}

// The scenario DMA is actually for: two sprites, draw into one while the other
// is being sent. Reported as frames/s so it can be compared with the same loop
// running without DMA (single sprite, blocking pushSprite).
static void benchDoubleBuffer(TFT_eSPI& tft, int w, int h, int frames, bool useDma)
{
    TFT_eSprite spr[2] = { TFT_eSprite(&tft), TFT_eSprite(&tft) };
    uint16_t*   ptr[2] = { nullptr, nullptr };

    const int count = useDma ? 2 : 1;
    for (int i = 0; i < count; i++)
    {
        ptr[i] = (uint16_t*)spr[i].createSprite(w, h);
        if (ptr[i] == nullptr)
        {
            ESP_LOGW(SPIBENCH_TAG, "%3dx%3d : double buffer allocation FAILED at sprite %d (%d largest free block)",
                     w, h, i, (int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            for (int j = 0; j < i; j++) spr[j].deleteSprite();
            return;
        }
    }

    int sel = 0;
    tft.startWrite();
    const int64_t t0 = esp_timer_get_time();
    for (int f = 0; f < frames; f++)
    {
        // Representative render work: clear plus a handful of filled shapes.
        spr[sel].fillSprite(TFT_BLACK);
        for (int c = 0; c < 8; c++)
            spr[sel].fillCircle(20 + (c * 25) % w, h / 2, 10, TFT_RED + c);

        if (useDma) { tft.pushImageDMA(0, 0, w, h, ptr[sel]); sel ^= 1; }
        else        { spr[0].pushSprite(0, 0); }
    }
    if (useDma) tft.dmaWait();
    const int64_t elapsed = esp_timer_get_time() - t0;
    tft.endWrite();

    for (int i = 0; i < count; i++) spr[i].deleteSprite();

    const double fps = frames / (elapsed / 1e6);
    ESP_LOGI(SPIBENCH_TAG, "%3dx%3d : %7.3f ms/frame, %5.1f fps (render+push, %s)",
             w, h, (elapsed / 1000.0) / frames, fps, useDma ? "DMA double buffered" : "no DMA");
}

// Redraw the whole 240x320 screen as horizontal strips. A fullscreen 16bpp
// sprite does not fit (153602 bytes vs a 110592 byte largest block), so strips
// are the only option anyway - and they are what makes double buffering cheap:
// two strips instead of two screens.
//
// With DMA the transfer of strip N overlaps the rendering of strip N+1, so the
// frame costs max(render, transfer) instead of render + transfer. "load" scales
// the amount of drawing per strip so the crossover can be seen.
static void benchFullScreen(TFT_eSPI& tft, int stripH, int load, int frames, bool useDma)
{
    const int w     = 240;
    const int total = 320;

    TFT_eSprite spr[2] = { TFT_eSprite(&tft), TFT_eSprite(&tft) };
    uint16_t*   ptr[2] = { nullptr, nullptr };

    const int count = useDma ? 2 : 1;
    for (int i = 0; i < count; i++)
    {
        ptr[i] = (uint16_t*)spr[i].createSprite(w, stripH);
        if (ptr[i] == nullptr)
        {
            ESP_LOGW(SPIBENCH_TAG, "strip %dx%d : allocation FAILED at sprite %d (%d largest free block)",
                     w, stripH, i, (int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            for (int j = 0; j < i; j++) spr[j].deleteSprite();
            return;
        }
    }

    int sel = 0;
    tft.startWrite();
    const int64_t t0 = esp_timer_get_time();
    for (int f = 0; f < frames; f++)
    {
        for (int y = 0; y < total; y += stripH)
        {
            const int h = (y + stripH > total) ? (total - y) : stripH;

            spr[sel].fillSprite(TFT_BLACK);
            for (int c = 0; c < load; c++)
                spr[sel].fillCircle((17 * c) % w, (11 * c) % h, 9, TFT_RED + c);

            if (useDma) { tft.pushImageDMA(0, y, w, h, ptr[sel]); sel ^= 1; }
            else        { spr[0].pushSprite(0, y); }
        }
    }
    if (useDma) tft.dmaWait();
    const int64_t elapsed = esp_timer_get_time() - t0;
    tft.endWrite();

    for (int i = 0; i < count; i++) spr[i].deleteSprite();

    const double msPerFrame = (elapsed / 1000.0) / frames;
    ESP_LOGI(SPIBENCH_TAG, "strip %3d, load %2d : %7.2f ms/frame, %5.1f fps (%s)",
             stripH, load, msPerFrame, 1000.0 / msPerFrame, useDma ? "DMA" : "no DMA");
}

// Render cost alone, with no pushing at all - so the model can be checked
// against the measurements instead of taken on faith.
static void benchRenderOnly(TFT_eSPI& tft, int stripH, int load, int frames)
{
    TFT_eSprite spr = TFT_eSprite(&tft);
    if (spr.createSprite(240, stripH) == nullptr) return;

    const int64_t t0 = esp_timer_get_time();
    for (int f = 0; f < frames; f++)
        for (int y = 0; y < 320; y += stripH)
        {
            spr.fillSprite(TFT_BLACK);
            for (int c = 0; c < load; c++)
                spr.fillCircle((17 * c) % 240, (11 * c) % stripH, 9, TFT_RED + c);
        }
    const int64_t elapsed = esp_timer_get_time() - t0;
    spr.deleteSprite();

    ESP_LOGI(SPIBENCH_TAG, "strip %3d, load %2d : %7.2f ms/frame (render only, no push)",
             stripH, load, (elapsed / 1000.0) / frames);
}

// Burn roughly the requested number of microseconds of CPU, standing in for
// per-entity game logic. Volatile so the optimiser leaves it alone.
static void busyWork(int us)
{
    if (us <= 0) return;
    const int64_t until = esp_timer_get_time() + us;
    volatile int sink = 0;
    while (esp_timer_get_time() < until) sink += 1;
    (void)sink;
}

// The game model: sprites are built once and only ever pushed, never drawn
// into. Nothing needs protecting from the DMA engine, so no second buffer is
// needed - the cost is zero extra bytes.
//
// The question this answers is whether DMA still pays off at typical sprite
// sizes, where the fixed per-push overhead (setAddrWindow + queue_trans) can
// outweigh the transfer itself. workUs simulates game logic done after each
// push, which is what DMA gets to overlap with.
static void benchGameSprites(TFT_eSPI& tft, int size, int count, int workUs, bool useDma)
{
    TFT_eSprite spr = TFT_eSprite(&tft);
    uint16_t* ptr = (uint16_t*)spr.createSprite(size, size);
    if (ptr == nullptr)
    {
        ESP_LOGW(SPIBENCH_TAG, "%2dx%2d : allocation FAILED", size, size);
        return;
    }
    spr.fillSprite(TFT_YELLOW);
    spr.fillCircle(size / 2, size / 2, size / 3, TFT_BLUE);

    const int frames = 30;
    tft.startWrite();
    const int64_t t0 = esp_timer_get_time();
    for (int f = 0; f < frames; f++)
    {
        for (int i = 0; i < count; i++)
        {
            const int x = (i * 37) % (240 - size);
            const int y = (i * 53) % (320 - size);

            if (useDma) tft.pushImageDMA(x, y, size, size, ptr);
            else        spr.pushSprite(x, y);

            busyWork(workUs);   // per-entity game logic
        }
    }
    if (useDma) tft.dmaWait();
    const int64_t elapsed = esp_timer_get_time() - t0;
    tft.endWrite();
    spr.deleteSprite();

    const double msPerFrame = (elapsed / 1000.0) / frames;
    ESP_LOGI(SPIBENCH_TAG, "%2dx%2d x%3d, work %3dus : %7.2f ms/frame, %5.1f fps (%s)",
             size, size, count, workUs, msPerFrame, 1000.0 / msPerFrame,
             useDma ? "DMA" : "no DMA");
}

// ---------------------------------------------------------------------------
// The pattern a game actually uses: image assets sit in flash, get blitted into
// a sprite, and the sprite goes to the TFT. Measured in three separate stages so
// it is visible which one costs what, instead of only seeing the total.
// ---------------------------------------------------------------------------

// Stage 1: flash -> sprite. pushImage() reads straight from .rodata, so every
// read goes through the flash cache. The RAM-sourced variant is the control:
// same copy, same code path, source already in DRAM. The difference between the
// two is what the flash cache costs.
static void benchBlit(TFT_eSPI& tft, int size, int count, bool fromFlash, uint16_t* ramCopy)
{
    TFT_eSprite spr = TFT_eSprite(&tft);
    if (spr.createSprite(size, size) == nullptr)
    {
        ESP_LOGW(SPIBENCH_TAG, "%2dx%2d : blit allocation FAILED", size, size);
        return;
    }

    const uint16_t* src = fromFlash ? flashAsset : ramCopy;

    const int reps = 200;
    const int64_t t0 = esp_timer_get_time();
    for (int r = 0; r < reps; r++)
        for (int i = 0; i < count; i++) spr.pushImage(0, 0, size, size, src);
    const int64_t elapsed = esp_timer_get_time() - t0;
    spr.deleteSprite();

    const double usPerBlit = (double)elapsed / (reps * count);
    const double mbPerSec  = (double)size * size * 2 / usPerBlit;
    ESP_LOGI(SPIBENCH_TAG, "%2dx%2d : %7.1f us/blit, %5.2f MB/s (%s -> sprite)",
             size, size, usPerBlit, mbPerSec, fromFlash ? "flash" : "  RAM");
}

// Stage 2: how much the sprite API costs per pixel. drawPixel() does bounds
// checks, viewport datum, a bpp branch chain and a byte swap around a single
// store; the raw variant writes the same swapped value straight through
// getPointer(). Same result on screen, so the difference is pure API overhead.
static void benchPerPixel(TFT_eSPI& tft, int size, bool raw)
{
    TFT_eSprite spr = TFT_eSprite(&tft);
    uint16_t* ptr = (uint16_t*)spr.createSprite(size, size);
    if (ptr == nullptr) return;

    const int reps = 200;
    const int64_t t0 = esp_timer_get_time();
    for (int r = 0; r < reps; r++)
    {
        if (raw)
        {
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++)
                {
                    const uint16_t c = (uint16_t)(x ^ y);
                    ptr[x + y * size] = (uint16_t)((c >> 8) | (c << 8));
                }
        }
        else
        {
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++)
                    spr.drawPixel(x, y, (uint16_t)(x ^ y));
        }
    }
    const int64_t elapsed = esp_timer_get_time() - t0;
    spr.deleteSprite();

    const double nsPerPixel = (double)elapsed * 1000.0 / (reps * size * size);
    ESP_LOGI(SPIBENCH_TAG, "%2dx%2d : %6.1f ns/pixel (%s)",
             size, size, nsPerPixel, raw ? "raw store via getPointer" : "spr.drawPixel()");
}

// Stage 3: the whole pipeline per frame - blit N assets from flash into the
// sprite, then ship the sprite. This is the number that matters for a game.
//
// Note this pattern *writes* into the sprite every frame, so the DMA path needs
// two of them: blitting the next asset into the buffer the DMA engine is still
// reading would corrupt the frame in flight. That is the difference with a
// static sprite that is only ever pushed - that one needs no second buffer.
static void benchAssetPipeline(TFT_eSPI& tft, int size, int count, bool useDma)
{
    TFT_eSprite spr[2] = { TFT_eSprite(&tft), TFT_eSprite(&tft) };
    uint16_t*   ptr[2] = { nullptr, nullptr };

    const int buffers = useDma ? 2 : 1;
    for (int i = 0; i < buffers; i++)
    {
        ptr[i] = (uint16_t*)spr[i].createSprite(size, size);
        if (ptr[i] == nullptr)
        {
            for (int j = 0; j < i; j++) spr[j].deleteSprite();
            return;
        }
    }

    int sel = 0;
    const int frames = 60;
    tft.startWrite();
    const int64_t t0 = esp_timer_get_time();
    for (int f = 0; f < frames; f++)
        for (int i = 0; i < count; i++)
        {
            const int x = (i * 37) % (240 - size);
            const int y = (i * 53) % (320 - size);

            spr[sel].pushImage(0, 0, size, size, flashAsset);   // flash -> sprite
            if (useDma) { tft.pushImageDMA(x, y, size, size, ptr[sel]); sel ^= 1; }
            else        { spr[0].pushSprite(x, y); }
        }
    if (useDma) tft.dmaWait();
    const int64_t elapsed = esp_timer_get_time() - t0;
    tft.endWrite();
    for (int i = 0; i < buffers; i++) spr[i].deleteSprite();

    const double msPerFrame = (elapsed / 1000.0) / frames;
    ESP_LOGI(SPIBENCH_TAG, "%2dx%2d x%2d : %7.2f ms/frame, %5.1f fps (blit+push, %s)",
             size, size, count, msPerFrame, 1000.0 / msPerFrame, useDma ? "DMA" : "no DMA");
}

static void runSpiBenchmark(void)
{
    TFT_eSPI tft = TFT_eSPI();
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

#ifdef USE_HSPI_PORT
    const char* port = "HSPI";
#else
    const char* port = "VSPI";
#endif

    ESP_LOGI(SPIBENCH_TAG, "=== SPI benchmark: %s port, SPI_FREQUENCY %d Hz ===", port, SPI_FREQUENCY);
    ESP_LOGI(SPIBENCH_TAG, "free heap %d, largest block %d",
             (int)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    ESP_LOGI(SPIBENCH_TAG, "--- blocking pushSprite ---");
    benchOneSize(tft, 240,  30, 100);   // banner strip, as in Sprite_scroll_16bit
    benchOneSize(tft, 240, 160,  30);   // half screen
    benchOneSize(tft, 240, 320,  20);   // full screen - also tests whether it fits at all

    // Two 240x80 16bpp sprites is 76.8 KB, which fits next to the rest of the
    // app; 240x160 would need 153 KB and cannot be double buffered here.
    ESP_LOGI(SPIBENCH_TAG, "--- render+push without DMA ---");
    benchDoubleBuffer(tft, 240, 80, 60, false);

    if (!tft.initDMA())
    {
        ESP_LOGW(SPIBENCH_TAG, "initDMA() failed - skipping DMA measurements");
        ESP_LOGI(SPIBENCH_TAG, "=== benchmark done ===");
        return;
    }
    tft.setSwapBytes(false);            // sprite data is already display byte order

    ESP_LOGI(SPIBENCH_TAG, "--- pushImageDMA ---");
    benchOneSizeDma(tft, 240,  30, 100);
    benchOneSizeDma(tft, 240, 160,  30);
    benchOneSizeDma(tft, 240, 320,  20);

    ESP_LOGI(SPIBENCH_TAG, "--- render+push with DMA ---");
    benchDoubleBuffer(tft, 240, 80, 60, true);

    // Full 240x320 redraw in 80px strips, at increasing render load.
    static const int loads[] = { 4, 50 };

    ESP_LOGI(SPIBENCH_TAG, "--- fullscreen redraw: render cost alone ---");
    for (int i = 0; i < 2; i++) benchRenderOnly(tft, 80, loads[i], 20);

    ESP_LOGI(SPIBENCH_TAG, "--- fullscreen redraw without DMA ---");
    for (int i = 0; i < 2; i++) benchFullScreen(tft, 80, loads[i], 20, false);

    ESP_LOGI(SPIBENCH_TAG, "--- fullscreen redraw with DMA double buffering ---");
    for (int i = 0; i < 2; i++) benchFullScreen(tft, 80, loads[i], 20, true);

    // Game model: static sprites, pushed only. No second buffer anywhere.
    static const int sizes[] = { 16, 32, 64 };

    ESP_LOGI(SPIBENCH_TAG, "--- game sprites, no game logic (pure push overhead) ---");
    for (int i = 0; i < 3; i++) benchGameSprites(tft, sizes[i], 40, 0, false);
    for (int i = 0; i < 3; i++) benchGameSprites(tft, sizes[i], 40, 0, true);

    ESP_LOGI(SPIBENCH_TAG, "--- game sprites, 100us logic per entity ---");
    for (int i = 0; i < 3; i++) benchGameSprites(tft, sizes[i], 40, 100, false);
    for (int i = 0; i < 3; i++) benchGameSprites(tft, sizes[i], 40, 100, true);

    // ---- flash asset -> sprite -> TFT ----
    // A RAM copy of the same asset, so the flash cache cost can be isolated.
    uint16_t* ramCopy = (uint16_t*)malloc(sizeof(flashAsset));
    if (ramCopy) memcpy(ramCopy, flashAsset, sizeof(flashAsset));

    ESP_LOGI(SPIBENCH_TAG, "--- stage 1: blit asset into sprite ---");
    for (int i = 0; i < 3; i++) benchBlit(tft, sizes[i], 1, true,  ramCopy);
    if (ramCopy) for (int i = 0; i < 3; i++) benchBlit(tft, sizes[i], 1, false, ramCopy);

    ESP_LOGI(SPIBENCH_TAG, "--- stage 2: sprite API cost per pixel ---");
    for (int i = 0; i < 3; i++) benchPerPixel(tft, sizes[i], false);
    for (int i = 0; i < 3; i++) benchPerPixel(tft, sizes[i], true);

    ESP_LOGI(SPIBENCH_TAG, "--- stage 3: full pipeline, 40 assets per frame ---");
    for (int i = 0; i < 3; i++) benchAssetPipeline(tft, sizes[i], 40, false);
    for (int i = 0; i < 3; i++) benchAssetPipeline(tft, sizes[i], 40, true);

    free(ramCopy);

    tft.deInitDMA();

    ESP_LOGI(SPIBENCH_TAG, "=== benchmark done ===");
}
