// by Marius Versteegen, 2024
#pragma once
// Important: when using CleanRTOS, 
// #include<CleanRTOS> at this location or prior to including THIS
// crt_TFT_eSPI_DisplayAdapter.h file.

//#include <Arduino.h>
#include "crt_typesAndConstants.h"
#include <LITTLEFS.h>
//#include "Free_Fonts.h" // Include the header file attached to this sketch
#include <crt_TFT_eSPI_IFreeFonts.h>
#include <TFT_eSPI.h>     // Hardware-specific library
#include <crt_TFT_eSPI_TouchDetector.h>
#include <crt_IDisplay.h>
#include <crt_Vec2.h>
#include <User_Setup.h>   // In that file, LOAD_GFXFF determines availability of free fonts.

#include <crt_ILogger.h>

//#define Z_THRESHOLD 350 // Touch pressure threshold for validating touches

#ifdef LOAD_GFXFF
// From Free_Fonts.h :
#define GFXFF 1
#endif

namespace crt
{
	extern ILogger& logger;

    //class TFT_eSPI_accessable;

    template <size_t MaxTouchListenerCount> 
    class TFT_eSPI_DisplayAdapter : public IDisplay
    {
    private:
        TFT_eSPI _tft;
        TFT_eSPI_TouchDetector<MaxTouchListenerCount> _touchDetector;
        TFT_eSPI_IFreeFonts& _freeFonts;
        const char* _filenameCalibration;
        bool _bRepeatCalibration;
        uint8_t _rotation;
        
    public:
        TFT_eSPI_DisplayAdapter(TFT_eSPI_IFreeFonts& freeFonts, const char* filenameCalibration, 
                                bool bRepeatCalibration)
            :_touchDetector(_tft),
            _freeFonts(freeFonts), _filenameCalibration(filenameCalibration),
            _bRepeatCalibration(bRepeatCalibration),
            _rotation(0)
        {}

        // Simple initialization without calibration - useful for non-touch scenarios
        void begin(uint8_t rotation)
        {
            _rotation = rotation;
            _tft.begin();
            _tft.setRotation(rotation);
            _tft.fillScreen(TFT_BLACK);
        }

        /*override*/ void pollTouch(void* pCallingTask)
        {
#ifdef TOUCH_DEBUG_DIRECT
            uint16_t x_tmp = 0, y_tmp = 0;
            #if defined(TOUCH_THRESHOLD)
                bool pressed = _tft.getTouch(&x_tmp, &y_tmp, TOUCH_THRESHOLD);
            #else
                bool pressed = _tft.getTouch(&x_tmp, &y_tmp);
            #endif
            if (pressed)
            {
                //_tft.fillCircle(x_tmp, y_tmp, 2, TFT_YELLOW);
                //logger.logText("Press detected");
                //ESP_LOGI("DisplayAdapter", "touch direct: %u,%u", (unsigned)x_tmp, (unsigned)y_tmp);
            }
#endif
            _touchDetector.update(pCallingTask);
        }

        /*override*/ void addTouchListener(ITouchListener* pTouchListener)
        {
            _touchDetector.addTouchListener(pTouchListener);
        }

        /*override*/ Vec2 getScreenSize()
        {
            if(_rotation == 1 || _rotation == 3)
                return Vec2(_tft.height(),_tft.width());
            else
                return Vec2(_tft.width(),_tft.height());
        }

        /*override*/ int16_t getTextWidth(const char* str, uint8_t font)
        {
            uint8_t usedFont = selectFontOrFreeFont(font);
            return _tft.textWidth(str, usedFont);
        }

        /*override*/ int16_t getFontHeight(uint8_t font)
        {
            uint8_t usedFont = selectFontOrFreeFont(font);
            return _tft.fontHeight(usedFont);
        }

        /*override*/ void setFontScale(uint8_t fontScale)
        {
            _tft.setTextSize(fontScale);
        }

        /*override*/ Vec2 getPrintCursor()
        {
            Vec2 cursor(_tft.getCursorX(), _tft.getCursorY());
            return cursor;
        }

        /*override*/ void fillScreen(uint32_t color)
        {
            _tft.fillScreen(colTo16bit(color));
        }

		/*override*/ void drawRect(const Vec2& topLeftPix, const Vec2& sizePix, uint32_t color)
		{
			_tft.drawRect(topLeftPix.x, topLeftPix.y, sizePix.x, sizePix.y, colTo16bit(color));
		}

        /*override*/ void fillRect(const Vec2& topLeftPix, const Vec2& sizePix, uint32_t color)
        {
            _tft.fillRect(topLeftPix.x, topLeftPix.y, sizePix.x, sizePix.y, colTo16bit(color));
        }

        /*override*/ void drawRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t color)
        {
            _tft.drawRoundRect(topLeftPix.x, topLeftPix.y, sizePix.x, sizePix.y, radius, colTo16bit(color));
        }

        /*override*/ void fillRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t color)
        {
            // Note: this function draws something ugly in the current implementation of TFT_eSPI:
            // The rounded corners do not align with the sides of the button.
            // For now, it is better to use the slow and pretty fillSmoothRoundRect function below instead.
            _tft.fillRoundRect(topLeftPix.x, topLeftPix.y, sizePix.x, sizePix.y, radius, colTo16bit(color));
        }

        /*override*/ void fillSmoothRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t colFg, uint32_t colBg)
        {
            _tft.fillSmoothRoundRect(topLeftPix.x, topLeftPix.y, sizePix.x, sizePix.y, radius, colTo16bit(colFg), colTo16bit(colBg));
        }

        /*override*/ void fillBeveledRect(const Vec2& topLeftPix, const Vec2& sizePix, uint32_t colFg, uint32_t colHighlight, uint32_t colShadow, BevelStyle style)
        {
            int32_t x = topLeftPix.x, y = topLeftPix.y, w = sizePix.x, h = sizePix.y;
            if ((w <= 0) || (h <= 0))
            {
                return;
            }

            uint16_t col16Fg = colTo16bit(colFg);
            uint16_t col16Highlight = colTo16bit(colHighlight);
            uint16_t col16Shadow = colTo16bit(colShadow);

            _tft.fillRect(x, y, w, h, col16Fg);

            // The gap around the key.
            _tft.fillRect(x, y, w, 1, col16Shadow);         // top
            _tft.fillRect(x, y, 1, h, col16Shadow);         // left
            _tft.fillRect(x, y + h - 1, w, 1, col16Shadow); // bottom
            _tft.fillRect(x + w - 1, y, 1, h, col16Shadow); // right

            // The gloss, just inside the gap. Raised: on the top and left, as if
            // lit from above. Sunken: on the bottom and right, because there the
            // light falls on the inner wall of the recess.
            if ((w > 2) && (h > 2))
            {
                if (style == BevelStyle::Sunken)
                {
                    _tft.fillRect(x + 1, y + h - 2, w - 2, 1, col16Highlight); // bottom
                    _tft.fillRect(x + w - 2, y + 1, 1, h - 2, col16Highlight); // right
                }
                else
                {
                    _tft.fillRect(x + 1, y + 1, w - 2, 1, col16Highlight);     // top
                    _tft.fillRect(x + 1, y + 1, 1, h - 2, col16Highlight);     // left
                }
            }
        }

        /*override*/ void drawString(const char* str, const Vec2& bottomLeftPix, Alignment alignment, uint8_t font, uint8_t scale, uint32_t color)
        {
            _tft.setTextDatum(textDatumFromAlignment(alignment));
            _tft.setTextColor(colTo16bit(color));    // one argument: transparent background
            _tft.setTextSize(scale);
            uint8_t usedFont = selectFontOrFreeFont(font);
            _tft.drawString(str, bottomLeftPix.x, bottomLeftPix.y, usedFont);
            _tft.setTextSize(1);
        }

        // Flicker-free label replacement. Two steps, in this order:
        //   1. draw the text with an opaque background, so TFT_eSPI paints each
        //      glyph cell with colBg while it writes the glyph (it does that for
        //      free fonts too, see TFT_eSPI.cpp:5020/5039);
        //   2. blank only what the new text does not cover but the previous,
        //      longer one might have: the strips between the text and the
        //      reserved area.
        // Because the text is written before anything is blanked, there is no
        // moment where the area is empty - and no background has to be painted
        // beforehand, which is exactly what makes a normal redraw flicker.
        //
        // Note: TFT_eSPI's own setTextPadding() is deliberately NOT used, even
        // though it would work here. For free fonts it sets padding=101
        // (TFT_eSPI.cpp:4938) and the blanking switch only handles 1/2/3; the
        // correction back to 1/2/3 happens at TFT_eSPI.cpp:5041, but only inside
        // the branch that also requires the first glyph to be within the font's
        // range. An empty string therefore does not blank anything - and that is
        // exactly the case hideText() relies on. Doing the blanking here covers
        // both, and keeps the behaviour identical for every font type.
        //
        // What is NOT avoided: for free fonts TFT_eSPI fills the whole text cell
        // before drawing the glyphs into it (TFT_eSPI.cpp:5039). So a small erase
        // remains, but it now spans the text cell instead of the entire widget,
        // and it is no longer a separate queued command with a gap after it.
        /*override*/ void drawStringOpaque(const char* str, const Vec2& bottomLeftPix, Alignment alignment, uint8_t font, uint8_t scale, uint32_t colFg, uint32_t colBg, const Vec2& paddingSizePix)
        {
            bool bHasPadding = (paddingSizePix.x > 0) && (paddingSizePix.y > 0);

            // The reserved box, positioned around the datum by the same rule the
            // text itself follows.
            Vec2 padTopLeft = calcAlignedTopLeftPix(bottomLeftPix, Vec2(0, 0), paddingSizePix, alignment);

            if (bHasPadding)
            {
                // Clip everything to the reserved box. Without this, the glyph
                // cell that TFT_eSPI paints can be taller than the widget it
                // belongs to (its height is the font's ascender+descender, not
                // the widget height), and it would paint colBg just outside the
                // widget - visible as a band that makes the widget look taller.
                // Same mechanism as in drawImage: vpDatum false keeps the
                // coordinates absolute.
                _tft.setViewport(padTopLeft.x, padTopLeft.y, paddingSizePix.x, paddingSizePix.y, false /*vpDatum*/);
            }

            _tft.setTextDatum(textDatumFromAlignment(alignment));
            _tft.setTextColor(colTo16bit(colFg), colTo16bit(colBg));  // two arguments: opaque
            _tft.setTextSize(scale);
            uint8_t usedFont = selectFontOrFreeFont(font);
            _tft.drawString(str, bottomLeftPix.x, bottomLeftPix.y, usedFont);

            // Measure while textsize is still `scale` - both textWidth and
            // fontHeight include it (TFT_eSPI.cpp:2973 and :3001).
            int32_t textWidthPix = _tft.textWidth(str, usedFont);
            int32_t textHeightPix = _tft.fontHeight(usedFont);
            _tft.setTextSize(1);

            if (bHasPadding && (paddingSizePix.x > textWidthPix))
            {
                // Blank what the new text does not cover but a previous, longer
                // one may have. The strips span the full height of the reserved
                // box, so nothing of the old text can survive above or below the
                // new glyph cells.
                Vec2 textTopLeft = calcAlignedTopLeftPix(bottomLeftPix, Vec2(0, 0), Vec2(textWidthPix, textHeightPix), alignment);
                uint16_t col16Bg = colTo16bit(colBg);

                int32_t widthLeftStrip = textTopLeft.x - padTopLeft.x;
                if (widthLeftStrip > 0)
                {
                    _tft.fillRect(padTopLeft.x, padTopLeft.y, widthLeftStrip, paddingSizePix.y, col16Bg);
                }

                int32_t xRightStrip = textTopLeft.x + textWidthPix;
                int32_t widthRightStrip = (padTopLeft.x + paddingSizePix.x) - xRightStrip;
                if (widthRightStrip > 0)
                {
                    _tft.fillRect(xRightStrip, padTopLeft.y, widthRightStrip, paddingSizePix.y, col16Bg);
                }
            }

            if (bHasPadding)
            {
                _tft.resetViewport();
            }
        }

        // Blits an image straight from wherever its pixels live - typically a
        // const array in flash. No sprite, no line buffer, no extra DRAM: the
        // data goes from .rodata into the SPI FIFO.
        /*override*/ void drawImage(const Vec2& topLeftPix, const Vec2& sizePix, const Image& image, Alignment alignment)
        {
            if ((image.pData == nullptr) || (image.width <= 0) || (image.height <= 0))
            {
                return;
            }
            if ((sizePix.x <= 0) || (sizePix.y <= 0))
            {
                return;
            }
            if (image.format != PixelFormat::RGB565)
            {
                ESP_LOGI("TFT_eSPI_DisplayAdapter", "Unsupported PixelFormat: %d", (int)image.format);
                return;
            }

            Vec2 imagePos = calcAlignedTopLeftPix(topLeftPix, sizePix, Vec2(image.width, image.height), alignment);

            // Clipping to the panel rectangle is done by TFT_eSPI's viewport:
            // the PI_CLIP macro inside every pushImage crops against it.
            // vpDatum == false keeps the coordinate origin at the top left of
            // the screen, so imagePos stays in absolute screen coordinates.
            _tft.setViewport(topLeftPix.x, topLeftPix.y, sizePix.x, sizePix.y, false /*vpDatum*/);

            // The image data is in host (little endian) byte order, the panel
            // wants big endian. The swap is free: pushSwapBytePixels swaps the
            // next chunk while the previous SPI transfer is still running.
            bool bSwapBytesBefore = _tft.getSwapBytes();
            _tft.setSwapBytes(true);

            // const_cast is deliberate and safe here. TFT_eSPI has a separate
            // "const" overload only because on AVR, PROGMEM is a distinct
            // address space needing pgm_read_word. On the ESP32 flash is
            // memory mapped, so the non-const overload reads .rodata just fine -
            // and it is the faster path: it pushes the whole unclipped image in
            // one pushPixels call instead of copying it line by line into a
            // stack buffer first. pushImage only ever reads the data.
            uint16_t* pPixels = const_cast<uint16_t*>(static_cast<const uint16_t*>(image.pData));

            if (image.bHasTransparency)
            {
                // This overload collects runs of non-transparent pixels per
                // scan line and writes each run as one block: exactly the
                // per-pixel/per-block split we want, without a framebuffer.
                _tft.pushImage(imagePos.x, imagePos.y, image.width, image.height,
                               pPixels, colTo16bit(image.colTransparent));
            }
            else
            {
                _tft.pushImage(imagePos.x, imagePos.y, image.width, image.height, pPixels);
            }

            _tft.setSwapBytes(bSwapBytesBefore);
            _tft.resetViewport();
        }

        /*override*/ void setPrintColor(void* pTask, uint32_t color)
        {
            _tft.setTextColor(colTo16bit(color));
        }

        /*override*/ void setPrintFont(void* pTask, uint8_t font)
        {
            selectFontOrFreeFont(font);
        }

        /*override*/ void setPrintScale(void* pTask, uint8_t fontScale)
        {
            _tft.setTextSize(fontScale);
        }

        /*override*/ void setPrintCursor(void* pTask, const Vec2& cursorPos)
        {
            _tft.setCursor(cursorPos.x, cursorPos.y);
        }

        /*override*/ void setPrintWrap(void* pTask, bool bWrapX)
        {
            _tft.setTextWrap(bWrapX);
        }

        /*override*/ void print(void* pTask, const char* str)
        {
            _tft.print(str);
        }

        /*override*/ void println(void* pTask, const char* str)
        {
            _tft.println(str);
        }

        uint16_t colTo16bit(uint32_t color) // ARGB
        {
            uint16_t color16 = ( (((color & 0x00FF0000) >> 19 ) << 11) |   // Take the 5 MSB bits of red.
                                 (((color & 0x0000FF00) >> 10 ) << 5 ) |   // Take the 6 MSB bits of green.
                                  ((color & 0x000000FF) >> 3  )         ); // Take the 5 MSB bits of blue.
            return color16;
        }

        bool queryDoYouWantToRecalibrate()
        {
            _tft.setCursor(20, 0);
            _tft.setTextFont(2);
            _tft.setTextSize(1);
            _tft.setTextColor(TFT_WHITE, TFT_BLACK);

            _tft.println("Touch now to recalibrate");
            
            int64_t endTime = esp_timer_get_time()+2000000;     // time in micro seconds. wait 2s.

            uint16_t x_tmp, y_tmp;

            bool bValidTouch = false;
            while (!bValidTouch && ((esp_timer_get_time()-endTime)<0))
            {
                //bValidTouch = _tft.validTouch(&x_tmp, &y_tmp, Z_THRESHOLD / 2));
                bValidTouch = _tft.getTouch(&x_tmp, &y_tmp); // Perhaps improve on this, inspired by _tft.validTouch.
            }
            
            if (bValidTouch)
            {
                ESP_LOGI("DisplayAdapter", "touchcalibration requested");
            }
            
            return bValidTouch;
        }

        // The function below was for the greater part copied from Bodmer.
        /*override*/ void touchCalibrate(uint8_t rotation, uint8_t font)
        {
            _rotation = rotation; // store to allow width-height swap in getScreenSize

            _tft.begin();
            _tft.setRotation(rotation);
            _tft.fillScreen(TFT_BLACK);
            _tft.setTextFont(font);  //tft.setFreeFont(FF18);

#ifdef TOUCH_SKIP_CALIBRATION
            // Skip interactive calibration (prevents blocking if touch is not yet working)
            uint16_t calData[5] = { 300, 3600, 300, 3600, 0 };
            if (!LITTLEFS.begin()) {
                LITTLEFS.format();
                LITTLEFS.begin();
            }
            if (LITTLEFS.exists(_filenameCalibration)) {
                fs::File f = LITTLEFS.open(_filenameCalibration, "r");
                if (f) {
                    uint8_t storedRotation;
                    if (f.readBytes((char*)&storedRotation, 1) == 1 &&
                        f.readBytes((char*)calData, 14) == 14 &&
                        storedRotation == rotation) {
                        _tft.setTouch(calData);
                        f.close();
                        _tft.fillScreen(TFT_BLACK);
                        return;
                    }
                    f.close();
                }
            }
            _tft.setTouch(calData);
            _tft.fillScreen(TFT_BLACK);
            return;
#else

            uint16_t calData[5];
            uint8_t calDataOK = 0;
            uint8_t storedRotation = 0xFF; // Invalid value to indicate no valid rotation stored

            // check file system exists
            if (!LITTLEFS.begin()) {
                ESP_LOGI("DisplayAdapter","Formating file system");
                LITTLEFS.format();
                LITTLEFS.begin();
            }

            ESP_LOGI("DisplayAdapter", "=== Touch Calibration Debug ===");
            ESP_LOGI("DisplayAdapter", "Constructor parameter bRepeatCalibration: %d", _bRepeatCalibration);
            ESP_LOGI("DisplayAdapter", "Requested rotation: %d", rotation);
            ESP_LOGI("DisplayAdapter", "Calibration file: %s", _filenameCalibration);

            // Save the ORIGINAL constructor parameter BEFORE any modifications
            bool bConstructorRequestedRecalibration = _bRepeatCalibration;
            bool bAskedForRecalibration = false;
            bool bRotationMismatch = false;

            // check if calibration file exists and size is correct
            if (LITTLEFS.exists(_filenameCalibration)) {
                ESP_LOGI("DisplayAdapter", "Calibration file EXISTS");
                fs::File f = LITTLEFS.open(_filenameCalibration, "r");
                if (f) {
                    ESP_LOGI("DisplayAdapter", "File opened successfully, file size: %d", f.size());
                    // Read rotation first, then calibration data byte by byte
                    int rotRead = f.read();
                    if (rotRead >= 0) {
                        storedRotation = (uint8_t)rotRead;
                        // Read calibration data byte by byte (5 uint16_t values = 10 bytes)
                        for (int i = 0; i < 5; i++) {
                            int lsb = f.read();
                            int msb = f.read();
                            if (lsb >= 0 && msb >= 0) {
                                calData[i] = (uint16_t)(lsb | (msb << 8));
                            }
                        }
                    }
                    ESP_LOGI("DisplayAdapter", "Read rotation=%d", storedRotation);

                    if (rotRead >= 0) {
                        ESP_LOGI("DisplayAdapter", "Stored rotation: %d, Current rotation: %d", storedRotation, rotation);
                        ESP_LOGI("DisplayAdapter", "Calibration data: [%d, %d, %d, %d, %d]",
                                 calData[0], calData[1], calData[2], calData[3], calData[4]);

                        if (storedRotation == rotation) {
                            // Rotation matches, calibration data is valid
                            calDataOK = 1;
                            ESP_LOGI("DisplayAdapter", "Rotation MATCHES - calibration data is valid");
                        } else {
                            // Rotation mismatch - force recalibration without asking
                            ESP_LOGI("DisplayAdapter", "Rotation MISMATCH: stored=%d, current=%d - forcing recalibration", storedRotation, rotation);
                            bRotationMismatch = true;
                            _bRepeatCalibration = true;
                        }
                    } else {
                        ESP_LOGE("DisplayAdapter", "Failed to read calibration data properly");
                    }
                    f.close();
                } else {
                    ESP_LOGE("DisplayAdapter", "Failed to open calibration file");
                }

                if (_bRepeatCalibration && !bRotationMismatch)
                {
                    // Delete if user requested re-calibration
                    LITTLEFS.remove(_filenameCalibration);
                }
            }
            else
            {
                // File doesn't exist - force recalibration without asking
                ESP_LOGI("DisplayAdapter", "Calibration file not found - forcing recalibration");
                _bRepeatCalibration = true;
            }

            ESP_LOGI("DisplayAdapter", "Before decision: calDataOK=%d, _bRepeatCalibration=%d, constructor=%d",
                     calDataOK, _bRepeatCalibration, bConstructorRequestedRecalibration);

            if (calDataOK && !_bRepeatCalibration) {
                ESP_LOGI("DisplayAdapter", "Using stored calibration - setting touch");
                // calibration data valid and rotation matches
                // Set touch calibration FIRST so we can detect touches
                _tft.setTouch(calData);

                // Ask user if they want to recalibrate (unless constructor forced it)
                if (!bConstructorRequestedRecalibration) {
                    ESP_LOGI("DisplayAdapter", "Asking user if they want to recalibrate");
                    _bRepeatCalibration = queryDoYouWantToRecalibrate();
                    bAskedForRecalibration = true;
                    ESP_LOGI("DisplayAdapter", "User response to recalibration query: %d", _bRepeatCalibration);

                    // If user requested recalibration, delete the file and recalibrate
                    if (_bRepeatCalibration) {
                        ESP_LOGI("DisplayAdapter", "User requested recalibration - deleting file");
                        LITTLEFS.remove(_filenameCalibration);
                    }
                } else {
                    ESP_LOGI("DisplayAdapter", "Constructor forced recalibration - skipping user prompt");
                }
            }

            if (calDataOK && !_bRepeatCalibration) {
                // User did not request recalibration, we're done
                ESP_LOGI("DisplayAdapter", "Calibration complete - using stored data");
                _tft.fillScreen(TFT_BLACK);
            }
            else
            {
                // data not valid, rotation mismatch, or recalibration requested so recalibrate
                ESP_LOGI("DisplayAdapter", "PERFORMING CALIBRATION (calDataOK=%d, _bRepeatCalibration=%d)",
                         calDataOK, _bRepeatCalibration);
                _tft.fillScreen(TFT_BLACK);
                _tft.setCursor(20, 0);
                _tft.setTextFont(2);
                _tft.setTextSize(1);
                _tft.setTextColor(TFT_WHITE, TFT_BLACK);

                _tft.println("Touch corners as indicated");

                _tft.setTextFont(1);
                _tft.println();

                if (_bRepeatCalibration && bAskedForRecalibration) {
                    _tft.setTextColor(TFT_RED, TFT_BLACK);
                }

                _tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

                _tft.setTextColor(TFT_GREEN, TFT_BLACK);
                _tft.println("Calibration complete!");

                // store data with rotation
                ESP_LOGI("DisplayAdapter", "Saving calibration to file: %s", _filenameCalibration);
                ESP_LOGI("DisplayAdapter", "Saving rotation: %d", rotation);
                ESP_LOGI("DisplayAdapter", "Saving calibration data: [%d, %d, %d, %d, %d]",
                         calData[0], calData[1], calData[2], calData[3], calData[4]);

                fs::File f = LITTLEFS.open(_filenameCalibration, FILE_WRITE);
                if (f) {
                    // Write as individual bytes to ensure binary mode
                    size_t rotWritten = f.write(rotation);  // Write rotation as single byte
                    size_t calWritten = 0;
                    for (int i = 0; i < 5; i++) {
                        calWritten += f.write((uint8_t)(calData[i] & 0xFF));        // LSB
                        calWritten += f.write((uint8_t)((calData[i] >> 8) & 0xFF)); // MSB
                    }
                    f.close();
                    ESP_LOGI("DisplayAdapter", "Written %d rotation bytes, %d calibration bytes",
                             rotWritten, calWritten);
                    ESP_LOGI("DisplayAdapter", "Calibration file saved successfully");

                    // Verify what was written by reading it back
                    f = LITTLEFS.open(_filenameCalibration, FILE_READ);
                    if (f) {
                        uint8_t verifyRot = f.read();
                        uint16_t verifyCal[5];
                        for (int i = 0; i < 5; i++) {
                            int lsb = f.read();
                            int msb = f.read();
                            verifyCal[i] = (uint16_t)(lsb | (msb << 8));
                        }
                        f.close();
                        ESP_LOGI("DisplayAdapter", "VERIFY: Read back rotation=%d, calData=[%d, %d, %d, %d, %d]",
                                 verifyRot, verifyCal[0], verifyCal[1], verifyCal[2], verifyCal[3], verifyCal[4]);
                    }
                } else {
                    ESP_LOGE("DisplayAdapter", "FAILED to open calibration file for writing!");
                }
            }

            _tft.fillScreen(TFT_BLACK);
#endif
        }

    private:
        uint8_t textDatumFromAlignment(Alignment alignment)
        {
            switch (alignment)
            {
            case Alignment::TopLeft:     return TL_DATUM;
            case Alignment::BottomLeft:  return BL_DATUM;
            case Alignment::MidLeft:     return CL_DATUM;
            case Alignment::TopRight:    return TR_DATUM;
            case Alignment::BottomRight: return BR_DATUM;
            case Alignment::MidRight:    return CR_DATUM;
            case Alignment::MidMid:      return MC_DATUM;
            case Alignment::TopMid:      return TC_DATUM;
            case Alignment::BottomMid:   return BC_DATUM;
            default:
                ESP_LOGI("TFT_eSPI_DisplayAdapter", "Unimplemented alignment: %d", (int)alignment);
                return TL_DATUM;
            }
        }

        uint8_t selectFontOrFreeFont(uint8_t font)
        {
#ifdef LOAD_GFXFF

            const GFXfont* freeFont = _freeFonts.getFreeFont(font);
            if (freeFont != nullptr)
            {
                _tft.setFreeFont(freeFont);
                return GFXFF; // Meaning: use the currently set free font.
            }
            else // no free font available
            {
                _tft.setTextFont(font);
                return font;
            }
#else
            _tft.setTextFont(font);
            return font;
#endif
        }
	};
};
