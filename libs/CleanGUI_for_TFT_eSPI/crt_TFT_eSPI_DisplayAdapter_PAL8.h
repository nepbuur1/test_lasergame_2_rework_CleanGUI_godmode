// by Marius Versteegen, 2026
#pragma once
// Important: when using CleanRTOS,
// #include<CleanRTOS> at this location or prior to including THIS file.

#include <crt_TFT_eSPI_DisplayAdapter.h>
#include <esp_heap_caps.h>

namespace crt
{
    // Tekent niet op het paneel maar in een schermvullende backbuffer van een byte
    // per pixel, waarin elke byte een index in het palet is. Bij present() gaat
    // die buffer in een keer naar het scherm.
    //
    // Wat dat oplevert:
    // - Een PAL8-plaatje blitten is geheugen kopieren. Plaatje en scherm delen
    //   hetzelfde palet, dus er valt per pixel niets om te rekenen. Dat is wat
    //   paletted werken snel maakt - niet de kleurdiepte.
    // - Geen flikkering en geen wisselende volgorde bij overlappende widgets: er
    //   komt pas iets op het scherm als het beeld af is.
    // - De beeldtijd wordt onafhankelijk van wat erop staat.
    //
    // Wat het kost:
    // - 76.802 bytes DRAM voor de buffer, plus een strook van 10 KB om in uit te
    //   pakken.
    // - 256 kleuren voor het hele scherm tegelijk.
    // - present() stuurt altijd het hele scherm: ~38 ms. Voor een scherm dat maar
    //   zelden verandert is dat duurder dan rechtstreeks tekenen; daarvoor is de
    //   RGB565-adapter er nog.
    //
    // Hoe het tekenen in die indexbuffer werkt - en waarom dat niet vanzelf
    // spreekt: TFT_eSPI zet bij een 8bpp sprite elke kleur om met
    //
    //     (color & 0xE000)>>8 | (color & 0x0700)>>6 | (color & 0x0018)>>3
    //
    // en bewaart die byte. Die reductie is exact omkeerbaar, dus voor elke index
    // bestaat er precies een "drager"-kleur die naar die index reduceert. Deze
    // adapter zoekt per teken-commando de index bij de gevraagde kleur en geeft de
    // dragerkleur door; TFT_eSPI schrijft dan onze index. Daardoor werkt alles wat
    // de sprite zelf implementeert - vlakken, lijnen, en vooral tekst met free
    // fonts, wat we anders zelf hadden moeten bouwen.
    //
    // Dat leunt op een implementatiedetail van TFT_eSPI, dus het wordt bij het
    // opstarten gecontroleerd in plaats van aangenomen: alle 256 indices erin
    // schrijven en teruglezen. Zie checkCarrierColours().
    template <size_t MaxTouchListenerCount>
    class TFT_eSPI_DisplayAdapter_PAL8 : public TFT_eSPI_DisplayAdapter_RGB565<MaxTouchListenerCount>
    {
    private:
        using Base = TFT_eSPI_DisplayAdapter_RGB565<MaxTouchListenerCount>;

        static const int32_t NofRowsPerStrip = 16;

        TFT_eSprite _backBuffer;
        bool _bBackBufferAllocated;

        // Het palet nog twee keer, allebei om een reden:
        // - in bytegewisselde volgorde, zodat present() geen swap per pixel hoeft;
        // - als losse r/g/b, om de dichtstbijzijnde index bij een kleur te zoeken
        //   zonder per vergelijking bits uit te pakken.
        uint16_t _arPaletteSwapped[256];
        uint8_t _arPaletteR[256], _arPaletteG[256], _arPaletteB[256];

        // De laatste paar kleuren die zijn opgezocht. Widgets hertekenen keer op
        // keer met dezelfde handvol kleuren, dus dit scheelt vrijwel altijd de
        // zoektocht over 256 ingangen.
        static const int NofCachedColours = 8;
        uint32_t _arCacheColour[NofCachedColours];
        uint8_t _arCacheIndex[NofCachedColours];
        int _nofCached;
        int _indexCacheNext;

        uint16_t* _pStripBuffer;
        bool _bRgb565WarningGiven;

    public:
        // Dezelfde parameters als de RGB565-variant, zodat een voorbeeld alleen
        // zijn typenaam hoeft om te zetten en verder identiek blijft.
        TFT_eSPI_DisplayAdapter_PAL8(TFT_eSPI_IFreeFonts& freeFonts, const char* filenameCalibration,
                                     bool bRepeatCalibration)
            : Base(freeFonts, filenameCalibration, bRepeatCalibration),
              _backBuffer(&this->_tft),
              _bBackBufferAllocated(false),
              _arPaletteSwapped{}, _arPaletteR{}, _arPaletteG{}, _arPaletteB{},
              _arCacheColour{}, _arCacheIndex{},
              _nofCached(0), _indexCacheNext(0),
              _pStripBuffer(nullptr),
              _bRgb565WarningGiven(false)
        {
        }

        ~TFT_eSPI_DisplayAdapter_PAL8()
        {
            if (_bBackBufferAllocated)
            {
                _backBuffer.deleteSprite();
            }
            if (_pStripBuffer != nullptr)
            {
                heap_caps_free(_pStripBuffer);
            }
        }

        /*override*/ void setPalette(const uint16_t* pPalette)
        {
            Base::setPalette(pPalette);

            if (pPalette == nullptr)
            {
                return;
            }

            for (int i = 0; i < 256; i++)
            {
                uint16_t colour = pPalette[i];
                _arPaletteSwapped[i] = (uint16_t)((colour >> 8) | (colour << 8));

                // Terug naar 8 bits per kanaal, zodat het zoeken naar de
                // dichtstbijzijnde kleur in gewone RGB-afstand gebeurt.
                _arPaletteR[i] = (uint8_t)(((colour >> 11) & 0x1F) * 255 / 31);
                _arPaletteG[i] = (uint8_t)(((colour >> 5) & 0x3F) * 255 / 63);
                _arPaletteB[i] = (uint8_t)((colour & 0x1F) * 255 / 31);
            }

            _nofCached = 0;
            _indexCacheNext = 0;
        }

        /*override*/ void begin(uint8_t rotation)
        {
            Base::begin(rotation);
            allocateBackBufferIfNeeded();
            redirectToBackBuffer();
        }

        // De kalibratie tekent bewust op het paneel: ze draait voordat er een
        // buffer is, ze vraagt om aanrakingen en moet dus meteen zichtbaar zijn.
        /*override*/ void touchCalibrate(uint8_t rotation, uint8_t font)
        {
            Base::touchCalibrate(rotation, font);
            allocateBackBufferIfNeeded();
            redirectToBackBuffer();
        }

        // Het beeld is af: de indices gaan door het palet en de bus op. Uitpakken
        // gebeurt per strook van 16 beeldregels, want een push per losse regel
        // kost ~23 us aan vaste opzet en dat tikt over 240 regels flink aan.
        /*override*/ void present()
        {
            if (!_bBackBufferAllocated || (_pStripBuffer == nullptr))
            {
                return;
            }

            const uint8_t* pSrc = (const uint8_t*)_backBuffer.getPointer();
            if (pSrc == nullptr)
            {
                return;
            }

            Vec2 sizePix = Base::getScreenSize();

            bool bSwapBefore = this->_tft.getSwapBytes();
            this->_tft.setSwapBytes(false);		// het palet staat al bytegewisseld klaar

            for (int32_t y = 0; y < sizePix.y; y += NofRowsPerStrip)
            {
                int32_t nofRows = sizePix.y - y;
                if (nofRows > NofRowsPerStrip)
                {
                    nofRows = NofRowsPerStrip;
                }

                uint16_t* pDst = _pStripBuffer;
                for (int32_t n = sizePix.x * nofRows; n > 0; n--)
                {
                    *pDst++ = _arPaletteSwapped[*pSrc++];
                }

                this->_tft.pushImage(0, y, sizePix.x, nofRows, _pStripBuffer);
            }

            this->_tft.setSwapBytes(bSwapBefore);
        }

        /*override*/ bool isBuffered()
        {
            // Meteen aanmaken als dat nog niet is gebeurd, zodat het antwoord niet
            // afhangt van wie er toevallig het eerst wakker is. Het tekenwerk gaat
            // er pas naartoe na de kalibratie - zie touchCalibrate().
            allocateBackBufferIfNeeded();
            return _bBackBufferAllocated;
        }

        // Een PAL8-plaatje in een PAL8-buffer: de bytes zijn al schermindices, dus
        // dit is kopieren. Precies waar de hele opzet het om te doen was.
        /*override*/ void drawImage(const Vec2& topLeftPix, const Vec2& sizePix, const Image& image, Alignment alignment,
                                    int32_t frameRow, int32_t frameCol)
        {
            if (!_bBackBufferAllocated)
            {
                Base::drawImage(topLeftPix, sizePix, image, alignment, frameRow, frameCol);
                return;
            }

            if (image.format != PixelFormat::PAL8)
            {
                // Een RGB565-plaatje zou per pixel de dichtstbijzijnde palet-index
                // moeten krijgen, en dat is een zoektocht per pixel. Niet doen:
                // zet het plaatje om met de converter (--pal8) en het is gratis.
                if (!_bRgb565WarningGiven)
                {
                    _bRgb565WarningGiven = true;
                    ESP_LOGE("DisplayAdapter_PAL8", "een niet-PAL8 plaatje wordt overgeslagen. "
                        "Zet de assets om met tools/img2rgb565.py --pal8.");
                }
                return;
            }

            if ((image.pData == nullptr) || (image.width <= 0) || (image.height <= 0)
                || (sizePix.x <= 0) || (sizePix.y <= 0))
            {
                return;
            }

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

            // Ook hier klipt de viewport van het doel, net als bij het paneel.
            Base::applyViewport(topLeftPix, sizePix);

            const uint8_t* pIndices = static_cast<const uint8_t*>(image.pData);
            if (bIsFrame)
            {
                pIndices += (int32_t)(frameRow * drawHeight) * image.width + (int32_t)(frameCol * drawWidth);
            }

            const uint8_t indexTransparent = toPaletteIndex(image.colTransparent);

            for (int32_t y = 0; y < drawHeight; y++)
            {
                const uint8_t* pLine = pIndices + y * image.width;

                if (!image.bHasTransparency)
                {
                    // sbpp=8 laat TFT_eSPI dit als 8-naar-8 herkennen: een memcpy
                    // per beeldrij, met de klipping van de sprite eromheen.
                    _backBuffer.pushImage(imagePos.x, imagePos.y + y, drawWidth, 1,
                                          (uint16_t*)const_cast<uint8_t*>(pLine), 8 /*sbpp*/);
                    continue;
                }

                // Transparant: per aaneengesloten stuk zichtbare pixels een kopie.
                // In geheugen kost zo'n aanroep vrijwel niets - er komt geen bus
                // aan te pas - dus het aantal stukken is hier geen bezwaar.
                int32_t xRunStart = -1;
                for (int32_t x = 0; x <= drawWidth; x++)
                {
                    bool bVisible = (x < drawWidth) && (pLine[x] != indexTransparent);

                    if (bVisible && (xRunStart < 0))
                    {
                        xRunStart = x;
                    }
                    else if (!bVisible && (xRunStart >= 0))
                    {
                        _backBuffer.pushImage(imagePos.x + xRunStart, imagePos.y + y, x - xRunStart, 1,
                                              (uint16_t*)const_cast<uint8_t*>(pLine + xRunStart), 8 /*sbpp*/);
                        xRunStart = -1;
                    }
                }
            }

            Base::restoreViewport();
        }

        // --- wat een TFT_eSprite zelf niet heeft -----------------------------
        // TFT_eSPI's fillScreen, drawRect en de rounded rects zijn niet virtueel
        // en zijn in de sprite ook niet overschreven, dus ze zouden op het paneel
        // terechtkomen in plaats van in de buffer. Hier opgebouwd uit fillRect,
        // die de sprite wel zelf heeft.

        /*override*/ void fillScreen(uint32_t color)
        {
            Vec2 sizePix = Base::getScreenSize();
            Base::fillRect(Vec2(0, 0), sizePix, color);
        }

        /*override*/ void drawRect(const Vec2& topLeftPix, const Vec2& sizePix, uint32_t color)
        {
            Base::fillRect(topLeftPix, Vec2(sizePix.x, 1), color);
            Base::fillRect(Vec2(topLeftPix.x, topLeftPix.y + sizePix.y - 1), Vec2(sizePix.x, 1), color);
            Base::fillRect(topLeftPix, Vec2(1, sizePix.y), color);
            Base::fillRect(Vec2(topLeftPix.x + sizePix.x - 1, topLeftPix.y), Vec2(1, sizePix.y), color);
        }

        // Afgeronde hoeken zijn er in deze eerste versie niet: een sprite heeft er
        // geen primitieve voor en het zelf natekenen is werk dat kan wachten tot
        // het opvalt. Ze worden dus vierkant.
        /*override*/ void drawRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t color)
        {
            drawRect(topLeftPix, sizePix, color);
        }

        /*override*/ void fillRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t color)
        {
            Base::fillRect(topLeftPix, sizePix, color);
        }

        /*override*/ void fillSmoothRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius,
                                              uint32_t colFg, uint32_t colBg)
        {
            Base::fillRect(topLeftPix, sizePix, colFg);
        }

    protected:
        // Elke kleur die de basisklasse doorgeeft komt hier langs. In plaats van
        // een echte 5-6-5 kleur geven we de dragerkleur van de palet-index terug,
        // zodat TFT_eSPI die index in de buffer schrijft.
        /*override*/ uint16_t colTo16bit(uint32_t color)
        {
            if (!_bBackBufferAllocated)
            {
                return Base::colTo16bit(color);		// nog op het paneel
            }
            return carrierForIndex(indexForColour(color));
        }

    private:
        // De omgekeerde van TFT_eSPI's 8bpp-reductie.
        static uint16_t carrierForIndex(uint8_t index)
        {
            return (uint16_t)(((index & 0xE0) << 8) | ((index & 0x1C) << 6) | ((index & 0x03) << 3));
        }

        static uint8_t indexFromCarrier(uint16_t colour)
        {
            return (uint8_t)(((colour & 0xE000) >> 8) | ((colour & 0x0700) >> 6) | ((colour & 0x0018) >> 3));
        }

        uint8_t indexForColour(uint32_t colour)
        {
            if (isPaletteIndex(colour))
            {
                return toPaletteIndex(colour);
            }

            for (int i = 0; i < _nofCached; i++)
            {
                if (_arCacheColour[i] == colour)
                {
                    return _arCacheIndex[i];
                }
            }

            uint8_t r = (uint8_t)((colour >> 16) & 0xFF);
            uint8_t g = (uint8_t)((colour >> 8) & 0xFF);
            uint8_t b = (uint8_t)(colour & 0xFF);

            // Index 0 is de transparante en hoort nooit als gewone kleur uit een
            // zoektocht te komen; vandaar vanaf 1.
            int32_t bestDistance = 0x7FFFFFFF;
            uint8_t bestIndex = 1;
            for (int i = 1; i < 256; i++)
            {
                int32_t dr = (int32_t)r - _arPaletteR[i];
                int32_t dg = (int32_t)g - _arPaletteG[i];
                int32_t db = (int32_t)b - _arPaletteB[i];
                int32_t distance = dr * dr + dg * dg + db * db;
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestIndex = (uint8_t)i;
                    if (distance == 0)
                    {
                        break;
                    }
                }
            }

            _arCacheColour[_indexCacheNext] = colour;
            _arCacheIndex[_indexCacheNext] = bestIndex;
            _indexCacheNext = (_indexCacheNext + 1) % NofCachedColours;
            if (_nofCached < NofCachedColours)
            {
                _nofCached++;
            }

            return bestIndex;
        }

        // De aanname onder deze hele adapter, gemeten in plaats van geloofd.
        bool checkCarrierColours()
        {
            for (int i = 0; i < 256; i++)
            {
                if (indexFromCarrier(carrierForIndex((uint8_t)i)) != i)
                {
                    ESP_LOGE("DisplayAdapter_PAL8", "dragerkleur klopt niet voor index %d.", i);
                    return false;
                }

                _backBuffer.drawPixel(0, 0, carrierForIndex((uint8_t)i));
                if (_backBuffer.readPixelValue(0, 0) != i)
                {
                    ESP_LOGE("DisplayAdapter_PAL8", "TFT_eSPI bewaart index %d niet zoals verwacht "
                        "(kreeg %d terug). Deze adapter leunt op de 8bpp-reductie van TFT_eSPI; "
                        "die blijkt anders te werken.", i, (int)_backBuffer.readPixelValue(0, 0));
                    return false;
                }
            }
            return true;
        }

        // Het geheugen regelen. Bewust los van het omleiden van het tekenwerk: de
        // kalibratie vraagt om aanrakingen en moet dus op het paneel te zien zijn,
        // niet in een buffer die pas bij present() wordt getoond.
        void allocateBackBufferIfNeeded()
        {
            if (_bBackBufferAllocated)
            {
                return;
            }

            Vec2 sizePix = Base::getScreenSize();

            _backBuffer.setColorDepth(8);
            if (_backBuffer.createSprite(sizePix.x, sizePix.y) == nullptr)
            {
                ESP_LOGE("DisplayAdapter_PAL8",
                    "backbuffer van %d x %d (%d bytes) past niet in de heap (%u vrij); "
                    "er wordt rechtstreeks op het paneel getekend.",
                    (int)sizePix.x, (int)sizePix.y, (int)(sizePix.x * sizePix.y),
                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
                return;
            }

            _pStripBuffer = (uint16_t*)heap_caps_malloc(sizePix.x * NofRowsPerStrip * sizeof(uint16_t),
                                                        MALLOC_CAP_8BIT);
            if (_pStripBuffer == nullptr)
            {
                ESP_LOGE("DisplayAdapter_PAL8", "geen strookbuffer; terug naar het paneel.");
                _backBuffer.deleteSprite();
                return;
            }

            if (!checkCarrierColours())
            {
                _backBuffer.deleteSprite();
                heap_caps_free(_pStripBuffer);
                _pStripBuffer = nullptr;
                return;
            }

            _backBuffer.fillSprite(0);		// index 0 - wordt straks overtekend
            _bBackBufferAllocated = true;

            ESP_LOGI("DisplayAdapter_PAL8", "backbuffer %d x %d, 8 bpp paletted, %d bytes; "
                "dragerkleuren gecontroleerd", (int)sizePix.x, (int)sizePix.y, (int)(sizePix.x * sizePix.y));
        }

        // Vanaf hier gaat al het tekenwerk naar de buffer.
        void redirectToBackBuffer()
        {
            if (_bBackBufferAllocated)
            {
                Base::_pTarget = &_backBuffer;
            }
        }
    };
};
