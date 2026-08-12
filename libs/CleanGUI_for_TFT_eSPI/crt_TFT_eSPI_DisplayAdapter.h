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
#include <esp_heap_caps.h>
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

    // Tekent rechtstreeks op het paneel: elk commando staat er meteen op, en
    // present() heeft niets te doen. De pixels gaan als RGB565 over de bus - 5
    // bits rood, 6 groen, 5 blauw, want groen is waar het oog het gevoeligst
    // voor is.
    // De tegenhanger is TFT_eSPI_DisplayAdapter_RGB332, die in een backbuffer
    // tekent. Die erft van deze klasse: het gedeelde deel is de vertaling van
    // CleanGUI naar TFT_eSPI en heeft zelf geen pixelformaat.
    template <size_t MaxTouchListenerCount> 
    class TFT_eSPI_DisplayAdapter_RGB565 : public IDisplay
    {
    protected:
        // Het paneel zelf: aanzetten, roteren, touch, kalibreren, en straks het
        // uiteindelijke uitsturen van een backbuffer. Altijd de hardware.
        TFT_eSPI _tft;

        // Waar het tekenwerk heen gaat. Hier is dat het paneel zelf, zodat elk
        // commando meteen zichtbaar is. Een afgeleide adapter kan dit naar een
        // TFT_eSprite laten wijzen en er in present() een keer per beeld mee naar
        // het paneel gaan; dat kan omdat TFT_eSprite van TFT_eSPI erft en dus
        // precies dezelfde tekenmethoden heeft.
        // Wat hier gedeeld wordt is de vertaling van CleanGUI naar TFT_eSPI - de
        // clipping, de uitlijning, de lettertypen - en die is onafhankelijk van
        // het aantal bits per pixel waarin het doel zijn pixels bewaart.
        TFT_eSPI* _pTarget;

    protected:
        TFT_eSPI_TouchDetector<MaxTouchListenerCount> _touchDetector;
        TFT_eSPI_IFreeFonts& _freeFonts;
        const char* _filenameCalibration;
        bool _bRepeatCalibration;
        uint8_t _rotation;

        // The clip set through setClipRect. Everything that draws within a
        // viewport of its own has to intersect with this instead of overwriting
        // it - see applyViewport/restoreViewport.
        bool _bBegun;
        bool _bClipActive;
        Vec2 _clipTopLeftPix, _clipSizePix;

    protected:
        // Het palet van het scherm: 256 kleuren in RGB565, meestal het palet dat
        // de converter naast de plaatjes heeft gezet. Nodig om paletteIndex(i) te
        // kunnen omzetten, en om een PAL8-plaatje te kunnen uitpakken.
        //
        // Bewust een kopie in DRAM en niet de flash-array zelf. Het uitpakken
        // leest de indices sequentieel uit flash en springt per pixel willekeurig
        // in deze tabel; staat die tabel ook in flash, dan verdringen die twee
        // elkaar in de flash-cache. Gemeten op een 64x64 plaatje: 5,45 ms met het
        // palet in flash tegen 1,88 ms voor hetzelfde plaatje in RGB565 - en die
        // 512 bytes DRAM halen dat verschil weg.
        uint16_t _arPalette[256];
        bool _bPaletteSet;
        bool _bPaletteWarningGiven;
        bool _bFormatWarningGiven;

        // Een beeldregel werk, voor het uitpakken van PAL8 naar RGB565. Pas
        // aangemaakt als er echt een PAL8-plaatje langskomt.
        uint16_t* _pLineBuffer;
        int32_t _nofLineBufferPixels;

    private:
        
    public:
        TFT_eSPI_DisplayAdapter_RGB565(TFT_eSPI_IFreeFonts& freeFonts, const char* filenameCalibration, 
                                bool bRepeatCalibration)
            :_pTarget(&_tft), _touchDetector(_tft),
            _freeFonts(freeFonts), _filenameCalibration(filenameCalibration),
            _bRepeatCalibration(bRepeatCalibration),
            _rotation(0),
            _bBegun(false),
            _bClipActive(false),
            _arPalette{},
            _bPaletteSet(false),
            _bPaletteWarningGiven(false),
            _bFormatWarningGiven(false),
            _pLineBuffer(nullptr),
            _nofLineBufferPixels(0)
        {}

        // Het palet dat bij paletteIndex(...) en bij PAL8-plaatjes hoort. Wijst
        // normaal naar de arPalette uit de gegenereerde _palette.h, die in flash
        // staat; er wordt niets gekopieerd.
        virtual void setPalette(const uint16_t* pPalette)
        {
            if (pPalette == nullptr)
            {
                _bPaletteSet = false;
                return;
            }

            for (int i = 0; i < 256; i++)
            {
                _arPalette[i] = pPalette[i];
            }
            _bPaletteSet = true;
        }

        const uint16_t* getPalette()
        {
            return _bPaletteSet ? _arPalette : nullptr;
        }

        /*override*/ void setClipRect(const Vec2& topLeftPix, const Vec2& sizePix)
        {
            _bClipActive = true;
            _clipTopLeftPix = topLeftPix;
            _clipSizePix = sizePix;
            restoreViewport();
        }

        /*override*/ void clearClipRect()
        {
            _bClipActive = false;
            restoreViewport();
        }

        // Er is niets te doen: alles wat getekend is, staat al op het paneel.
        // Zie IDisplayDraw::present() voor waarom een voorbeeld hem toch aanroept.
        /*override*/ void present()
        {
        }

        /*override*/ bool isBuffered()
        {
            return false;		// alles staat meteen op het paneel
        }

        // Simple initialization without calibration - useful for non-touch scenarios
        virtual void begin(uint8_t rotation)
        {
            _rotation = rotation;
            _tft.begin();
            _bBegun = true;
            _tft.setRotation(rotation);
            _pTarget->fillScreen(TFT_BLACK);
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

        // Onthoudt de rotatie, en past hem toe zodra de TFT draait. Zo klopt
        // getScreenSize() ook als hij wordt aangeroepen voordat begin() of
        // touchCalibrate() is uitgevoerd.
        /*override*/ void setRotation(uint8_t rotation)
        {
            _rotation = rotation;
            if (_bBegun)
            {
                _tft.setRotation(rotation);
            }
        }

        /*override*/ Vec2 getScreenSize()
        {
            // Afgeleid van de natuurlijke paneelmaat plus de rotatie, en NIET van
            // _pTarget->width()/height(). Die geven de gedraaide maat pas nadat
            // setRotation is uitgevoerd; ervoor melden ze portret. Hier stond
            // daarom een omwisseling voor rotatie 1 en 3 - maar TFT_eSPI doet die
            // omwisseling na setRotation zelf al, dus dat was er een te veel.
            // Gemeten gevolg: getScreenSize() gaf (240,320) waar de layout op
            // (320,240) was gebaseerd, en er werd een kwart van de breedte
            // weggeknipt. Dat het daarvoor goed leek te gaan, berustte op een
            // race tussen PageRoot en touchCalibrate.
            if ((_rotation == 1) || (_rotation == 3))
            {
                return Vec2(TFT_HEIGHT, TFT_WIDTH);		// TFT_WIDTH/HEIGHT zijn in portret gedefinieerd
            }
            return Vec2(TFT_WIDTH, TFT_HEIGHT);
        }

        /*override*/ int16_t getTextWidth(const char* str, uint8_t font)
        {
            uint8_t usedFont = selectFontOrFreeFont(font);
            return _pTarget->textWidth(str, usedFont);
        }

        /*override*/ int16_t getFontHeight(uint8_t font)
        {
            uint8_t usedFont = selectFontOrFreeFont(font);
            return _pTarget->fontHeight(usedFont);
        }

        /*override*/ void setFontScale(uint8_t fontScale)
        {
            _pTarget->setTextSize(fontScale);
        }

        /*override*/ Vec2 getPrintCursor()
        {
            Vec2 cursor(_pTarget->getCursorX(), _pTarget->getCursorY());
            return cursor;
        }

        /*override*/ void fillScreen(uint32_t color)
        {
            _pTarget->fillScreen(colTo16bit(color));
        }

		/*override*/ void drawRect(const Vec2& topLeftPix, const Vec2& sizePix, uint32_t color)
		{
			_pTarget->drawRect(topLeftPix.x, topLeftPix.y, sizePix.x, sizePix.y, colTo16bit(color));
		}

        /*override*/ void fillRect(const Vec2& topLeftPix, const Vec2& sizePix, uint32_t color)
        {
            _pTarget->fillRect(topLeftPix.x, topLeftPix.y, sizePix.x, sizePix.y, colTo16bit(color));
        }

        /*override*/ void drawRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t color)
        {
            _pTarget->drawRoundRect(topLeftPix.x, topLeftPix.y, sizePix.x, sizePix.y, radius, colTo16bit(color));
        }

        /*override*/ void fillRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t color)
        {
            // Note: this function draws something ugly in the current implementation of TFT_eSPI:
            // The rounded corners do not align with the sides of the button.
            // For now, it is better to use the slow and pretty fillSmoothRoundRect function below instead.
            _pTarget->fillRoundRect(topLeftPix.x, topLeftPix.y, sizePix.x, sizePix.y, radius, colTo16bit(color));
        }

        /*override*/ void fillSmoothRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t colFg, uint32_t colBg)
        {
            _pTarget->fillSmoothRoundRect(topLeftPix.x, topLeftPix.y, sizePix.x, sizePix.y, radius, colTo16bit(colFg), colTo16bit(colBg));
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

            _pTarget->fillRect(x, y, w, h, col16Fg);

            // The gap around the key.
            _pTarget->fillRect(x, y, w, 1, col16Shadow);         // top
            _pTarget->fillRect(x, y, 1, h, col16Shadow);         // left
            _pTarget->fillRect(x, y + h - 1, w, 1, col16Shadow); // bottom
            _pTarget->fillRect(x + w - 1, y, 1, h, col16Shadow); // right

            // The gloss, just inside the gap. Raised: on the top and left, as if
            // lit from above. Sunken: on the bottom and right, because there the
            // light falls on the inner wall of the recess.
            if ((w > 2) && (h > 2))
            {
                if (style == BevelStyle::Sunken)
                {
                    _pTarget->fillRect(x + 1, y + h - 2, w - 2, 1, col16Highlight); // bottom
                    _pTarget->fillRect(x + w - 2, y + 1, 1, h - 2, col16Highlight); // right
                }
                else
                {
                    _pTarget->fillRect(x + 1, y + 1, w - 2, 1, col16Highlight);     // top
                    _pTarget->fillRect(x + 1, y + 1, 1, h - 2, col16Highlight);     // left
                }
            }
        }

        /*override*/ void drawString(const char* str, const Vec2& bottomLeftPix, Alignment alignment, uint8_t font, uint8_t scale, uint32_t color)
        {
            _pTarget->setTextDatum(textDatumFromAlignment(alignment));
            _pTarget->setTextColor(colTo16bit(color));    // one argument: transparent background
            _pTarget->setTextSize(scale);
            uint8_t usedFont = selectFontOrFreeFont(font);
            _pTarget->drawString(str, bottomLeftPix.x, bottomLeftPix.y, usedFont);
            _pTarget->setTextSize(1);
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
                applyViewport(padTopLeft, paddingSizePix);
            }

            _pTarget->setTextDatum(textDatumFromAlignment(alignment));
            _pTarget->setTextColor(colTo16bit(colFg), colTo16bit(colBg));  // two arguments: opaque
            _pTarget->setTextSize(scale);
            uint8_t usedFont = selectFontOrFreeFont(font);
            _pTarget->drawString(str, bottomLeftPix.x, bottomLeftPix.y, usedFont);

            // Measure while textsize is still `scale` - both textWidth and
            // fontHeight include it (TFT_eSPI.cpp:2973 and :3001).
            int32_t textWidthPix = _pTarget->textWidth(str, usedFont);
            int32_t textHeightPix = _pTarget->fontHeight(usedFont);
            _pTarget->setTextSize(1);

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
                    _pTarget->fillRect(padTopLeft.x, padTopLeft.y, widthLeftStrip, paddingSizePix.y, col16Bg);
                }

                int32_t xRightStrip = textTopLeft.x + textWidthPix;
                int32_t widthRightStrip = (padTopLeft.x + paddingSizePix.x) - xRightStrip;
                if (widthRightStrip > 0)
                {
                    _pTarget->fillRect(xRightStrip, padTopLeft.y, widthRightStrip, paddingSizePix.y, col16Bg);
                }
            }

            if (bHasPadding)
            {
                restoreViewport();
            }
        }

        // Blits an image straight from wherever its pixels live - typically a
        // const array in flash. No sprite, no line buffer, no extra DRAM: the
        // data goes from .rodata into the SPI FIFO.
        /*override*/ void drawImage(const Vec2& topLeftPix, const Vec2& sizePix, const Image& image, Alignment alignment,
                                    int32_t frameRow, int32_t frameCol)
        {
            if ((image.pData == nullptr) || (image.width <= 0) || (image.height <= 0))
            {
                return;
            }
            if ((sizePix.x <= 0) || (sizePix.y <= 0))
            {
                return;
            }
            if ((image.format != PixelFormat::RGB565) && (image.format != PixelFormat::PAL8))
            {
                // Een keer klagen en niet per beeld: op 115200 baud kost een
                // logregel ~4 ms, en dan meet je de klacht in plaats van het
                // tekenwerk.
                if (!_bFormatWarningGiven)
                {
                    _bFormatWarningGiven = true;
                    ESP_LOGE("TFT_eSPI_DisplayAdapter", "Unsupported PixelFormat: %d", (int)image.format);
                }
                return;
            }

            // A frame of a spritesheet is a rectangle inside the pixel array, so
            // its rows are not adjacent: every next row starts image.width pixels
            // further on. That is the only difference with a whole image.
            bool bIsFrame = (frameRow >= 0) && (frameCol >= 0) &&
                            (image.nofRows > 0) && (image.nofCols > 0) &&
                            ((image.nofRows > 1) || (image.nofCols > 1));

            int32_t drawWidth  = bIsFrame ? (image.width / image.nofCols)  : image.width;
            int32_t drawHeight = bIsFrame ? (image.height / image.nofRows) : image.height;

            if ((drawWidth <= 0) || (drawHeight <= 0))
            {
                return;
            }

            Vec2 imagePos = calcAlignedTopLeftPix(topLeftPix, sizePix, Vec2(drawWidth, drawHeight), alignment);

            // Clipping to the panel rectangle is done by TFT_eSPI's viewport:
            // the PI_CLIP macro inside every pushImage crops against it.
            // vpDatum == false keeps the coordinate origin at the top left of
            // the screen, so imagePos stays in absolute screen coordinates.
            applyViewport(topLeftPix, sizePix);

            // The image data is in host (little endian) byte order, the panel
            // wants big endian. The swap is free: pushSwapBytePixels swaps the
            // next chunk while the previous SPI transfer is still running.
            bool bSwapBytesBefore = _pTarget->getSwapBytes();
            _pTarget->setSwapBytes(true);

            // const_cast is deliberate and safe here. TFT_eSPI has a separate
            // "const" overload only because on AVR, PROGMEM is a distinct
            // address space needing pgm_read_word. On the ESP32 flash is
            // memory mapped, so the non-const overload reads .rodata just fine -
            // and it is the faster path: it pushes the whole unclipped image in
            // one pushPixels call instead of copying it line by line into a
            // stack buffer first. pushImage only ever reads the data.
            if (image.format == PixelFormat::PAL8)
            {
                drawImagePal8(image, imagePos, drawWidth, drawHeight, frameRow, frameCol, bIsFrame);
                _pTarget->setSwapBytes(bSwapBytesBefore);
                restoreViewport();
                return;
            }

            uint16_t* pPixels = const_cast<uint16_t*>(static_cast<const uint16_t*>(image.pData));

            if (bIsFrame)
            {
                // One pushImage per scan line, because pushImage has no notion
                // of a source stride. That costs a setWindow (~34 us) per line
                // instead of one for the whole blit: a 32x32 frame lands at
                // ~1,4 ms where the bus alone needs 0,4 ms. Acceptable for a
                // sprite, and it keeps the viewport clipping of pushImage - which
                // a hand-rolled setWindow + pushPixels would bypass.
                uint16_t* pFrame = pPixels + (int32_t)(frameRow * drawHeight) * image.width
                                           + (int32_t)(frameCol * drawWidth);

                for (int32_t y = 0; y < drawHeight; y++)
                {
                    uint16_t* pLine = pFrame + y * image.width;

                    if (image.bHasTransparency)
                    {
                        _pTarget->pushImage(imagePos.x, imagePos.y + y, drawWidth, 1,
                                       pLine, colTo16bit(image.colTransparent));
                    }
                    else
                    {
                        _pTarget->pushImage(imagePos.x, imagePos.y + y, drawWidth, 1, pLine);
                    }
                }
            }
            else if (image.bHasTransparency)
            {
                // This overload collects runs of non-transparent pixels per
                // scan line and writes each run as one block: exactly the
                // per-pixel/per-block split we want, without a framebuffer.
                _pTarget->pushImage(imagePos.x, imagePos.y, image.width, image.height,
                               pPixels, colTo16bit(image.colTransparent));
            }
            else
            {
                _pTarget->pushImage(imagePos.x, imagePos.y, image.width, image.height, pPixels);
            }

            _pTarget->setSwapBytes(bSwapBytesBefore);
            restoreViewport();
        }

        /*override*/ void setPrintColor(void* pTask, uint32_t color)
        {
            _pTarget->setTextColor(colTo16bit(color));
        }

        /*override*/ void setPrintFont(void* pTask, uint8_t font)
        {
            selectFontOrFreeFont(font);
        }

        /*override*/ void setPrintScale(void* pTask, uint8_t fontScale)
        {
            _pTarget->setTextSize(fontScale);
        }

        /*override*/ void setPrintCursor(void* pTask, const Vec2& cursorPos)
        {
            _pTarget->setCursor(cursorPos.x, cursorPos.y);
        }

        /*override*/ void setPrintWrap(void* pTask, bool bWrapX)
        {
            _pTarget->setTextWrap(bWrapX);
        }

        /*override*/ void print(void* pTask, const char* str)
        {
            _pTarget->print(str);
        }

        /*override*/ void println(void* pTask, const char* str)
        {
            _pTarget->println(str);
        }

        // Een PAL8-plaatje uitpakken naar RGB565, regel voor regel, en pushen.
        // Deze adapter tekent rechtstreeks op het paneel, dus het palet moet er
        // hier alsnog doorheen; een PAL8-adapter kopieert de indices juist zoals
        // ze zijn. Zelfde plaatje in flash, twee manieren om het te tonen.
        void drawImagePal8(const Image& image, const Vec2& imagePos,
                           int32_t drawWidth, int32_t drawHeight,
                           int32_t frameRow, int32_t frameCol, bool bIsFrame)
        {
            if (!_bPaletteSet)
            {
                if (!_bPaletteWarningGiven)
                {
                    _bPaletteWarningGiven = true;
                    ESP_LOGE("DisplayAdapter", "PAL8-plaatje zonder palet: roep setPalette() aan "
                        "met de arPalette uit de gegenereerde _palette.h.");
                }
                return;
            }

            // Uitpakken in stroken en niet per beeldregel: elke pushImage kost
            // ~23 us aan vaste opzet, en bij een sprite van 64 breed is dat meer
            // dan de pixels zelf. Met 16 regels per keer blijven er vier aanroepen
            // over in plaats van vierenzestig. Gemeten verschil: 5,5 -> ~2 ms.
            const int32_t nofRowsPerStrip = 16;
            if (!ensureLineBuffer(drawWidth * nofRowsPerStrip))
            {
                return;
            }

            const uint8_t* pIndices = static_cast<const uint8_t*>(image.pData);
            if (bIsFrame)
            {
                pIndices += (int32_t)(frameRow * drawHeight) * image.width + (int32_t)(frameCol * drawWidth);
            }

            const uint8_t indexTransparent = toPaletteIndex(image.colTransparent);

            if (!image.bHasTransparency)
            {
                for (int32_t yStrip = 0; yStrip < drawHeight; yStrip += nofRowsPerStrip)
                {
                    int32_t nofRows = drawHeight - yStrip;
                    if (nofRows > nofRowsPerStrip)
                    {
                        nofRows = nofRowsPerStrip;
                    }

                    uint16_t* pDst = _pLineBuffer;
                    for (int32_t y = 0; y < nofRows; y++)
                    {
                        const uint8_t* pLine = pIndices + (yStrip + y) * image.width;
                        for (int32_t x = 0; x < drawWidth; x++)
                        {
                            *pDst++ = _arPalette[pLine[x]];
                        }
                    }

                    _pTarget->pushImage(imagePos.x, imagePos.y + yStrip, drawWidth, nofRows, _pLineBuffer);
                }
                return;
            }

            for (int32_t y = 0; y < drawHeight; y++)
            {
                const uint8_t* pLine = pIndices + y * image.width;

                // Transparant: per aaneengesloten stuk zichtbare pixels een push.
                // Het aantal stukken telt, niet het aantal pixels - een setWindow
                // kost ~12 us, een pixel 0,4.
                int32_t xRunStart = -1;
                for (int32_t x = 0; x <= drawWidth; x++)
                {
                    bool bVisible = (x < drawWidth) && (pLine[x] != indexTransparent);

                    if (bVisible)
                    {
                        if (xRunStart < 0)
                        {
                            xRunStart = x;
                        }
                        _pLineBuffer[x] = _arPalette[pLine[x]];
                    }
                    else if (xRunStart >= 0)
                    {
                        _pTarget->pushImage(imagePos.x + xRunStart, imagePos.y + y,
                                            x - xRunStart, 1, _pLineBuffer + xRunStart);
                        xRunStart = -1;
                    }
                }
            }
        }

        bool ensureLineBuffer(int32_t nofPixels)
        {
            if (_nofLineBufferPixels >= nofPixels)
            {
                return true;
            }

            if (_pLineBuffer != nullptr)
            {
                heap_caps_free(_pLineBuffer);
            }

            _pLineBuffer = (uint16_t*)heap_caps_malloc(nofPixels * sizeof(uint16_t), MALLOC_CAP_8BIT);
            _nofLineBufferPixels = (_pLineBuffer != nullptr) ? nofPixels : 0;

            if (_pLineBuffer == nullptr)
            {
                ESP_LOGE("DisplayAdapter", "geen %d bytes voor de regelbuffer van een PAL8-plaatje.",
                    (int)(nofPixels * sizeof(uint16_t)));
                return false;
            }
            return true;
        }

        // De enige plek waar een CleanGUI-kleur hardware-kleur wordt - en daarom
        // ook de plek waar een palet-index wordt herkend. Zie paletteIndex() in
        // crt_typesAndConstants.h: dezelfde fillRect() kan daardoor zowel "maak dit
        // lichtblauw" als "maak dit kleur 7" betekenen.
        virtual uint16_t colTo16bit(uint32_t color) // ARGB, of paletteIndex(...)
        {
            if (isPaletteIndex(color))
            {
                if (!_bPaletteSet)
                {
                    // Niet stilzwijgend iets willekeurigs tekenen: magenta valt op,
                    // en de melding zegt wat er ontbreekt.
                    if (!_bPaletteWarningGiven)
                    {
                        _bPaletteWarningGiven = true;
                        ESP_LOGE("DisplayAdapter", "er wordt een palet-index gebruikt zonder dat "
                            "setPalette() is aangeroepen; alles wordt magenta.");
                    }
                    return 0xF81F;
                }
                return _arPalette[toPaletteIndex(color)];
            }

            uint16_t color16 = ( (((color & 0x00FF0000) >> 19 ) << 11) |   // Take the 5 MSB bits of red.
                                 (((color & 0x0000FF00) >> 10 ) << 5 ) |   // Take the 6 MSB bits of green.
                                  ((color & 0x000000FF) >> 3  )         ); // Take the 5 MSB bits of blue.
            return color16;
        }

        bool queryDoYouWantToRecalibrate()
        {
            _pTarget->setCursor(20, 0);
            _pTarget->setTextFont(2);
            _pTarget->setTextSize(1);
            _pTarget->setTextColor(TFT_WHITE, TFT_BLACK);

            _pTarget->println("Touch now to recalibrate");
            
            int64_t endTime = esp_timer_get_time()+2000000;     // time in micro seconds. wait 2s.

            uint16_t x_tmp, y_tmp;

            bool bValidTouch = false;
            while (!bValidTouch && ((esp_timer_get_time()-endTime)<0))
            {
                //bValidTouch = _tft.validTouch(&x_tmp, &y_tmp, Z_THRESHOLD / 2));
                // Met de drempel uit User_Setup.h, niet met de standaardwaarde van
                // TFT_eSPI. Die staat namelijk op 600 terwijl dit bord 350 opgeeft,
                // en het verschil voel je: de drempel is de druk (Z) waarboven een
                // aanraking meetelt, dus hoger betekent harder duwen.
                // Nog gevoeliger kan door TOUCH_THRESHOLD te verlagen, maar dan
                // gaat ruis meetellen en krijg je aanrakingen die er niet waren.
#ifdef TOUCH_THRESHOLD
                bValidTouch = _tft.getTouch(&x_tmp, &y_tmp, TOUCH_THRESHOLD);
#else
                bValidTouch = _tft.getTouch(&x_tmp, &y_tmp);
#endif
            }
            
            if (bValidTouch)
            {
                ESP_LOGI("DisplayAdapter", "touchcalibration requested");
            }
            
            return bValidTouch;
        }

        // --- de kalibratietabel -------------------------------------------
        //
        // Versie 1 bewaarde precies een kalibratie, met de rotatie waarvoor hij
        // gold als eerste byte ervoor. Startte je een app in een andere rotatie,
        // dan kwam die byte niet overeen, werd er opnieuw gekalibreerd en werd
        // het bestand overschreven - de kalibratie van de vorige rotatie was dan
        // weg. Heen en weer springen tussen een portret-app en een landschap-app
        // kostte dus elke boot vier hoeken aantikken, en dat maakte het testen
        // van de voorbeelden achter elkaar onwerkbaar.
        //
        // Versie 2 bewaart alle vier de rotaties naast elkaar, elk in een eigen
        // slot, met een geldigheidsbit per slot. Kalibreren raakt alleen het slot
        // van de rotatie die je kalibreert; de andere drie blijven staan. Zijn ze
        // alle vier een keer gedaan - zie het voorbeeld TouchCalibrateAll - dan
        // is wisselen tussen apps met verschillende rotaties gratis.
        //
        // Bestandsindeling, little endian net als in versie 1:
        //   [0..3]   magic 'C','G','T','C'
        //   [4]      versie (2)
        //   [5]      geldigheidsmasker, bit r hoort bij rotatie r
        //   [6..45]  4 slots van 5 uint16: de calData van TFT_eSPI::setTouch
        //
        // De twee versies zijn niet te verwarren: versie 1 is 11 bytes en begint
        // met een rotatie (0..3), versie 2 is 46 bytes en begint met 'C' (67).
    protected:
        enum : int
        {
            NofRotations = 4,
            NofCalValues = 5,
            CalFileVersion = 2,
            CalFileSizeV1 = 1 + NofCalValues * 2,                               // 11
            CalFileHeaderSize = 6,
            CalFileSizeV2 = CalFileHeaderSize + NofRotations * NofCalValues * 2 // 46
        };

        struct CalTable
        {
            uint8_t validMask;
            uint16_t arCalData[NofRotations][NofCalValues];
        };

        static bool isCalSlotValid(const CalTable& table, uint8_t rotation)
        {
            return (rotation < NofRotations) && ((table.validMask & (1 << rotation)) != 0);
        }

        bool mountFileSystem()
        {
            bool bMounted = LITTLEFS.begin();
            if (!bMounted)
            {
                ESP_LOGI("DisplayAdapter", "bestandssysteem formatteren");
                LITTLEFS.format();
                bMounted = LITTLEFS.begin();
            }

            if (bMounted)
            {
                // Een keer per boot, zodat de ruimte op het bord na te rekenen is en
                // niet alleen op papier. De spiffs-partitie is 0x19000 = 100 KB en
                // littlefs werkt in blokken van 4 KB; de kalibratietabel (46 bytes)
                // en de hall of fame (126 bytes) zijn de enige twee bestanden die
                // deze codebase erin zet. Ruimte is dus geen zorg - maar als iemand
                // er later meer in kwijt wil, staat het hier in de log.
                ESP_LOGI("DisplayAdapter", "littlefs: %u van %u bytes in gebruik",
                         (unsigned)LITTLEFS.usedBytes(), (unsigned)LITTLEFS.totalBytes());
            }
            return bMounted;
        }

        // Leest de tabel, of laat hem leeg als er niets bruikbaars staat. Een
        // bestand van versie 1 wordt overgenomen: die ene kalibratie hoort bij de
        // rotatie die erin staat en gaat dus in dat slot. Een bord dat al
        // gekalibreerd was hoeft die rotatie daardoor niet opnieuw te doen.
        bool loadCalTable(CalTable& table)
        {
            table.validMask = 0;
            for (int r = 0; r < NofRotations; r++)
            {
                for (int i = 0; i < NofCalValues; i++)
                {
                    table.arCalData[r][i] = 0;
                }
            }

            if (!LITTLEFS.exists(_filenameCalibration))
            {
                ESP_LOGI("DisplayAdapter", "geen kalibratiebestand %s", _filenameCalibration);
                return false;
            }

            fs::File f = LITTLEFS.open(_filenameCalibration, FILE_READ);
            if (!f)
            {
                ESP_LOGE("DisplayAdapter", "kalibratiebestand %s niet te openen", _filenameCalibration);
                return false;
            }

            uint8_t buf[CalFileSizeV2] = {};
            size_t nofRead = f.read(buf, sizeof(buf));
            f.close();

            if ((nofRead >= (size_t)CalFileSizeV2) &&
                (buf[0] == 'C') && (buf[1] == 'G') && (buf[2] == 'T') && (buf[3] == 'C') &&
                (buf[4] == (uint8_t)CalFileVersion))
            {
                table.validMask = buf[5];
                const uint8_t* p = buf + CalFileHeaderSize;
                for (int r = 0; r < NofRotations; r++)
                {
                    for (int i = 0; i < NofCalValues; i++)
                    {
                        table.arCalData[r][i] = (uint16_t)(p[0] | (p[1] << 8));
                        p += 2;
                    }
                }
                ESP_LOGI("DisplayAdapter", "kalibratietabel gelezen, gekalibreerde rotaties: %s%s%s%s",
                         isCalSlotValid(table, 0) ? "0 " : "", isCalSlotValid(table, 1) ? "1 " : "",
                         isCalSlotValid(table, 2) ? "2 " : "", isCalSlotValid(table, 3) ? "3 " : "");
                return true;
            }

            // Exact 11 bytes, niet "minstens": het TFT_eWidget-voorbeeld
            // Button_demo.ino schrijft naar datzelfde pad een bestand van 14 bytes
            // in zijn eigen indeling. Dat wordt zo niet aangezien voor versie 1 -
            // wat er soms wel uit zou komen, want de eerste byte daarvan is de LSB
            // van calData[0] en die kan toevallig onder de 4 liggen.
            if ((nofRead == (size_t)CalFileSizeV1) && (buf[0] < (uint8_t)NofRotations))
            {
                uint8_t rotation = buf[0];
                const uint8_t* p = buf + 1;
                for (int i = 0; i < NofCalValues; i++)
                {
                    table.arCalData[rotation][i] = (uint16_t)(p[0] | (p[1] << 8));
                    p += 2;
                }
                table.validMask = (uint8_t)(1 << rotation);
                ESP_LOGI("DisplayAdapter", "kalibratiebestand van versie 1 overgenomen voor rotatie %d",
                         (int)rotation);
                return true;
            }

            ESP_LOGE("DisplayAdapter", "kalibratiebestand niet herkend (%d bytes gelezen)", (int)nofRead);
            return false;
        }

        bool saveCalTable(const CalTable& table)
        {
            uint8_t buf[CalFileSizeV2];
            buf[0] = 'C'; buf[1] = 'G'; buf[2] = 'T'; buf[3] = 'C';
            buf[4] = (uint8_t)CalFileVersion;
            buf[5] = table.validMask;

            uint8_t* p = buf + CalFileHeaderSize;
            for (int r = 0; r < NofRotations; r++)
            {
                for (int i = 0; i < NofCalValues; i++)
                {
                    *p++ = (uint8_t)(table.arCalData[r][i] & 0xFF);
                    *p++ = (uint8_t)((table.arCalData[r][i] >> 8) & 0xFF);
                }
            }

            // FILE_WRITE kapt het bestand af, dus de hele tabel gaat er in een keer
            // opnieuw in. Bewust niet LITTLEFS.remove() eerst: dat was in versie 1
            // precies de stap die de andere rotaties weggooide.
            fs::File f = LITTLEFS.open(_filenameCalibration, FILE_WRITE);
            if (!f)
            {
                ESP_LOGE("DisplayAdapter", "kalibratiebestand %s niet te schrijven", _filenameCalibration);
                return false;
            }
            size_t nofWritten = f.write(buf, sizeof(buf));
            f.close();

            if (nofWritten != sizeof(buf))
            {
                ESP_LOGE("DisplayAdapter", "kalibratiebestand maar deels geschreven: %d van %d bytes",
                         (int)nofWritten, (int)sizeof(buf));
                return false;
            }

            ESP_LOGI("DisplayAdapter", "kalibratietabel opgeslagen in %s, geldigheidsmasker 0x%02X",
                     _filenameCalibration, (unsigned)table.validMask);
            return true;
        }

        // Wacht tot er geen vinger meer op het glas ligt. Tussen twee rotaties
        // door nodig: calibrateTouch() begint meteen te meten in de hoek
        // linksboven, en een vinger die daar nog van de vorige ronde ligt wordt
        // dan als die eerste hoek gemeten.
        //
        // Bewust op de ruwe druk en niet op getTouch(): die rekent de meting eerst
        // om met de kalibratie die er op dat moment in staat, en juist die is hier
        // nog onbetrouwbaar - een aanraking buiten het scherm telt dan niet mee.
        void waitUntilTouchReleased(uint32_t timeoutMs)
        {
#ifdef TOUCH_THRESHOLD
            const uint16_t threshold = TOUCH_THRESHOLD;
#else
            const uint16_t threshold = 600;
#endif
            const int NofQuietSamplesNeeded = 4;    // een enkele lage meting is ruis

            int64_t endTime = esp_timer_get_time() + (int64_t)timeoutMs * 1000;
            int nofQuiet = 0;

            while ((esp_timer_get_time() - endTime) < 0)
            {
                nofQuiet = (_tft.getTouchRawZ() <= threshold) ? (nofQuiet + 1) : 0;
                if (nofQuiet >= NofQuietSamplesNeeded)
                {
                    return;
                }
                delay(20);
            }
            ESP_LOGI("DisplayAdapter", "er blijft druk op het scherm staan; toch maar doorgaan");
        }

        // Kalibreert een rotatie en zet het resultaat in de tabel. De rotatie moet
        // al op de TFT staan. Het bewaren doet de aanroeper, zodat die kan kiezen
        // wanneer dat gebeurt.
        void calibrateRotationIntoTable(CalTable& table, uint8_t rotation, bool bWasAsked,
                                        const char* pExtraLine)
        {
            ESP_LOGI("DisplayAdapter", "kalibreren van rotatie %d", (int)rotation);

            _pTarget->fillScreen(TFT_BLACK);
            _pTarget->setCursor(20, 0);
            _pTarget->setTextFont(2);
            _pTarget->setTextSize(1);

            // Rood als de gebruiker er zelf om vroeg: dan is dit geen verrassing
            // maar een bevestiging dat de tik is aangekomen.
            _pTarget->setTextColor(bWasAsked ? TFT_RED : TFT_WHITE, TFT_BLACK);

            if (pExtraLine != nullptr)
            {
                _pTarget->println(pExtraLine);
            }
            _pTarget->println("Touch corners as indicated");

            _pTarget->setTextFont(1);
            _pTarget->println();

            _tft.calibrateTouch(table.arCalData[rotation], TFT_MAGENTA, TFT_BLACK, 15);
            table.validMask |= (uint8_t)(1 << rotation);

            _pTarget->setTextColor(TFT_GREEN, TFT_BLACK);
            _pTarget->println("Calibration complete!");

            ESP_LOGI("DisplayAdapter", "rotatie %d gekalibreerd: [%d, %d, %d, %d, %d]", (int)rotation,
                     table.arCalData[rotation][0], table.arCalData[rotation][1],
                     table.arCalData[rotation][2], table.arCalData[rotation][3],
                     table.arCalData[rotation][4]);
        }

    public:
        // The function below was for the greater part copied from Bodmer.
        /*override*/ void touchCalibrate(uint8_t rotation, uint8_t font)
        {
            _rotation = rotation; // store to allow width-height swap in getScreenSize

            _tft.begin();
            _bBegun = true;
            _tft.setRotation(rotation);
            _pTarget->fillScreen(TFT_BLACK);
            _pTarget->setTextFont(font);  //tft.setFreeFont(FF18);

            mountFileSystem();

            CalTable table;
            loadCalTable(table);

#ifdef TOUCH_SKIP_CALIBRATION
            // Niet interactief kalibreren. Handig als de touch nog niet werkt, want
            // anders blijft het bord in calibrateTouch() staan wachten op hoeken die
            // nooit komen.
            if (isCalSlotValid(table, rotation))
            {
                _tft.setTouch(table.arCalData[rotation]);
            }
            else
            {
                uint16_t arFallback[NofCalValues] = { 300, 3600, 300, 3600, 0 };
                _tft.setTouch(arFallback);
            }
            _pTarget->fillScreen(TFT_BLACK);
            return;
#else
            bool bConstructorRequestedRecalibration = _bRepeatCalibration;
            bool bHaveStored = isCalSlotValid(table, rotation);
            bool bAskedForRecalibration = false;

            ESP_LOGI("DisplayAdapter", "=== Touch Calibration ===");
            ESP_LOGI("DisplayAdapter", "rotatie %d, opgeslagen kalibratie: %s, bRepeatCalibration: %d",
                     (int)rotation, bHaveStored ? "ja" : "nee", (int)bConstructorRequestedRecalibration);

            if (bHaveStored && !bConstructorRequestedRecalibration)
            {
                // Eerst instellen, anders kan de vraag hieronder geen aanraking zien.
                _tft.setTouch(table.arCalData[rotation]);
                _bRepeatCalibration = queryDoYouWantToRecalibrate();
                bAskedForRecalibration = true;
            }

            if (bHaveStored && !_bRepeatCalibration)
            {
                ESP_LOGI("DisplayAdapter", "opgeslagen kalibratie voor rotatie %d in gebruik", (int)rotation);
            }
            else
            {
                calibrateRotationIntoTable(table, rotation, bAskedForRecalibration, nullptr);
                saveCalTable(table);
                _tft.setTouch(table.arCalData[rotation]);
            }

            _pTarget->fillScreen(TFT_BLACK);
#endif
        }

        // Kalibreert alle vier de rotaties achter elkaar en bewaart ze naast
        // elkaar. Daarna hoeft geen enkele app nog te kalibreren, ook niet als hij
        // in een andere rotatie draait dan de app ervoor. Zie het voorbeeld
        // TouchCalibrateAll, dat verder niets anders doet.
        //
        // Draait bewust buiten AsyncDisplay om: dit tekent rechtstreeks op het
        // paneel, net als touchCalibrate zelf, en er is nog geen widgetboom.
        // Gebruik hem daarom op de RGB565-adapter; de PAL8-adapter zou per rotatie
        // een backbuffer van een andere maat nodig hebben.
        void touchCalibrateAllRotations(uint8_t font)
        {
            _tft.begin();
            _bBegun = true;

            mountFileSystem();

            CalTable table;
            loadCalTable(table);

            for (uint8_t rotation = 0; rotation < (uint8_t)NofRotations; rotation++)
            {
                _rotation = rotation;
                _tft.setRotation(rotation);
                _pTarget->setTextFont(font);

                // Los van calibrateTouch(), dat pas na de eerste hoek een pauze
                // inlast: hier ligt de vinger nog op de hoek rechtsonder van de
                // vorige rotatie.
                waitUntilTouchReleased(3000);

                char arLine[32];
                snprintf(arLine, sizeof(arLine), "Rotation %d of %d", (int)rotation + 1, (int)NofRotations);
                calibrateRotationIntoTable(table, rotation, false, arLine);

                // Per rotatie bewaren en niet pas aan het eind: gaat het bord
                // halverwege uit, dan is alleen de laatste rotatie kwijt.
                saveCalTable(table);
            }

            _rotation = 0;
            _tft.setRotation(_rotation);
            _pTarget->fillScreen(TFT_BLACK);
            _pTarget->setCursor(20, 20);
            _pTarget->setTextFont(2);
            _pTarget->setTextSize(1);
            _pTarget->setTextColor(TFT_GREEN, TFT_BLACK);
            _pTarget->println("All 4 rotations calibrated.");
            _pTarget->println("Flash your app; it will not");
            _pTarget->println("ask for calibration again.");

            ESP_LOGI("DisplayAdapter", "alle vier de rotaties gekalibreerd, masker 0x%02X",
                     (unsigned)table.validMask);
        }

    private:
    protected:
        // Sets the viewport to the given rectangle, intersected with the clip
        // that setClipRect established. Without the intersection a widget that
        // clips internally - drawImage, drawStringOpaque - would silently draw
        // outside the region a partial repaint is limited to.
        // An empty intersection yields a width or height below 1, on which
        // setViewport sets its own out-of-bounds flag and inhibits all drawing.
        void applyViewport(const Vec2& topLeftPix, const Vec2& sizePix)
        {
            int32_t x0 = topLeftPix.x;
            int32_t y0 = topLeftPix.y;
            int32_t x1 = x0 + sizePix.x;
            int32_t y1 = y0 + sizePix.y;

            if (_bClipActive)
            {
                if (_clipTopLeftPix.x > x0) { x0 = _clipTopLeftPix.x; }
                if (_clipTopLeftPix.y > y0) { y0 = _clipTopLeftPix.y; }
                if (_clipTopLeftPix.x + _clipSizePix.x < x1) { x1 = _clipTopLeftPix.x + _clipSizePix.x; }
                if (_clipTopLeftPix.y + _clipSizePix.y < y1) { y1 = _clipTopLeftPix.y + _clipSizePix.y; }
            }

            _pTarget->setViewport(x0, y0, x1 - x0, y1 - y0, false /*vpDatum*/);
        }

        // Back to the active clip, or to the whole screen if there is none.
        // Never call resetViewport directly - that would drop the clip.
        void restoreViewport()
        {
            if (_bClipActive)
            {
                _pTarget->setViewport(_clipTopLeftPix.x, _clipTopLeftPix.y, _clipSizePix.x, _clipSizePix.y, false /*vpDatum*/);
            }
            else
            {
                _pTarget->resetViewport();
            }
        }

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
                _pTarget->setFreeFont(freeFont);
                return GFXFF; // Meaning: use the currently set free font.
            }
            else // no free font available
            {
                _pTarget->setTextFont(font);
                return font;
            }
#else
            _pTarget->setTextFont(font);
            return font;
#endif
        }
	};

    // De naam van voor de splitsing. Hij houdt bestaande code (apps/pong_cleangui,
    // CleanCore/examples/StdQueue) draaiend zonder aanpassing.
    template <size_t MaxTouchListenerCount>
    using TFT_eSPI_DisplayAdapter = TFT_eSPI_DisplayAdapter_RGB565<MaxTouchListenerCount>;
};
