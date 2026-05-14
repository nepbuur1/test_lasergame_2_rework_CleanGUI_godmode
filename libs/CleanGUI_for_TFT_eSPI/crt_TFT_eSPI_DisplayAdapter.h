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

        /*override*/ void drawString(const char* str, const Vec2& bottomLeftPix, Alignment alignment, uint8_t font, uint8_t scale, uint32_t color)
        {
            uint8_t textDatum = 0;

            switch (alignment)
            {
            case Alignment::TopLeft:
                textDatum = TL_DATUM;
                break;
            case Alignment::BottomLeft:
                textDatum = BL_DATUM;
                break;
            case Alignment::MidLeft:
                textDatum = CL_DATUM;
                break;
            case Alignment::TopRight:
                textDatum = TR_DATUM;
                break;
            case Alignment::BottomRight:
                textDatum = BR_DATUM;
                break;
            case Alignment::MidRight:
                textDatum = CR_DATUM;
                break;
            case Alignment::MidMid:
                textDatum = MC_DATUM;
                break;
            case Alignment::TopMid:
                textDatum = TC_DATUM;
                break;
            case Alignment::BottomMid:
                textDatum = BC_DATUM;
                break;

            default:
                ESP_LOGI("TFT_eSPI_DisplayAdapter", "Unimplemented alignment: %d", (int)alignment);
                break;
            }
            _tft.setTextDatum(textDatum);
            _tft.setTextColor(colTo16bit(color));
            _tft.setTextSize(scale);
            uint8_t usedFont = selectFontOrFreeFont(font);
            _tft.drawString(str, bottomLeftPix.x, bottomLeftPix.y, usedFont);
            _tft.setTextSize(1);
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
