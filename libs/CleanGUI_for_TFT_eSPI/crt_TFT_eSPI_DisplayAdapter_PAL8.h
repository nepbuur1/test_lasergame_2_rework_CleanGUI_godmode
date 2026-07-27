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

        uint16_t* _arpStripBuffer[2];
        int _indexStripBuffer;
        int32_t _widthPix, _heightPix;		// van de buffer; ook de stride
        bool _bDmaEnabled;
        bool _bDmaTried;
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
              _arpStripBuffer{}, _indexStripBuffer(0),
              _widthPix(0), _heightPix(0),
              _bDmaEnabled(false),
              _bDmaTried(false),
              _bRgb565WarningGiven(false)
        {
        }

        ~TFT_eSPI_DisplayAdapter_PAL8()
        {
            if (_bBackBufferAllocated)
            {
                _backBuffer.deleteSprite();
            }
            freeStripBuffers();
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
        // Het beeld is af: de indices gaan door het palet en de bus op.
        //
        // Met DMA pingpongt dit tussen twee stroken: terwijl strook N wordt
        // uitgezonden pakt de CPU strook N+1 uit. De dmaWait() zit binnenin
        // pushImageDMA en wacht op de vorige transfer, niet op de zijne - dat is
        // precies de pijplijn die we willen. Aan het eind bewust niet wachten: de
        // laatste strook mag onderweg zijn terwijl de tekentaak alweer aan het
        // volgende beeld begint.
        /*override*/ void present()
        {
            if (!_bBackBufferAllocated)
            {
                return;
            }

            const uint8_t* pSrc = (const uint8_t*)_backBuffer.getPointer();
            if (pSrc == nullptr)
            {
                return;
            }

            bool bSwapBefore = this->_tft.getSwapBytes();
            this->_tft.setSwapBytes(false);		// het palet staat al bytegewisseld klaar

            for (int32_t y = 0; y < _heightPix; y += NofRowsPerStrip)
            {
                int32_t nofRows = _heightPix - y;
                if (nofRows > NofRowsPerStrip)
                {
                    nofRows = NofRowsPerStrip;
                }

                uint16_t* pStrip = _arpStripBuffer[_indexStripBuffer];

                uint16_t* pDst = pStrip;
                for (int32_t n = _widthPix * nofRows; n > 0; n--)
                {
                    *pDst++ = _arPaletteSwapped[*pSrc++];
                }

                if (_bDmaEnabled)
                {
                    // De const-variant met opzet: die kopieert niet en wisselt geen
                    // bytes om, en wacht alleen op de vorige transfer.
                    this->_tft.pushImageDMA(0, y, _widthPix, nofRows, (const uint16_t*)pStrip);
                    _indexStripBuffer = 1 - _indexStripBuffer;
                }
                else
                {
                    this->_tft.pushImage(0, y, _widthPix, nofRows, pStrip);
                }
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
        // dit is kopieren. Precies waar de hele paletted opzet het om te doen was.
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

            // Klippen: een keer per plaatje in plaats van per stuk opnieuw.
            //
            // Dat is de hele reden dat hier niet gewoon TFT_eSprite::pushImage
            // staat. Die klipt met PI_CLIP bij elke aanroep opnieuw, test de
            // bpp-combinaties af en doet dan een memcpy van gemiddeld elf bytes.
            // Gemeten: 3,5 us per aanroep, en de barbaar heeft er 173 - dus 599 us
            // per blit, waarvan 99% opzet en 1% het verplaatsen van de 1.945
            // zichtbare bytes.
            //
            // Er gelden drie begrenzingen tegelijk: de rechthoek van het panel, de
            // clip-rect die repaintRegion heeft gezet, en de buffer zelf.
            int32_t clipX0 = topLeftPix.x;
            int32_t clipY0 = topLeftPix.y;
            int32_t clipX1 = clipX0 + sizePix.x;
            int32_t clipY1 = clipY0 + sizePix.y;

            if (Base::_bClipActive)
            {
                if (Base::_clipTopLeftPix.x > clipX0) { clipX0 = Base::_clipTopLeftPix.x; }
                if (Base::_clipTopLeftPix.y > clipY0) { clipY0 = Base::_clipTopLeftPix.y; }
                if (Base::_clipTopLeftPix.x + Base::_clipSizePix.x < clipX1) { clipX1 = Base::_clipTopLeftPix.x + Base::_clipSizePix.x; }
                if (Base::_clipTopLeftPix.y + Base::_clipSizePix.y < clipY1) { clipY1 = Base::_clipTopLeftPix.y + Base::_clipSizePix.y; }
            }

            if (clipX0 < 0) { clipX0 = 0; }
            if (clipY0 < 0) { clipY0 = 0; }
            if (clipX1 > _widthPix)  { clipX1 = _widthPix; }
            if (clipY1 > _heightPix) { clipY1 = _heightPix; }

            // En dan: welk deel van het plaatje valt daar nog binnen?
            int32_t xFrom = clipX0 - imagePos.x;
            int32_t xTo   = clipX1 - imagePos.x;
            int32_t yFrom = clipY0 - imagePos.y;
            int32_t yTo   = clipY1 - imagePos.y;

            if (xFrom < 0) { xFrom = 0; }
            if (yFrom < 0) { yFrom = 0; }
            if (xTo > drawWidth)  { xTo = drawWidth; }
            if (yTo > drawHeight) { yTo = drawHeight; }

            if ((xFrom >= xTo) || (yFrom >= yTo))
            {
                return;		// niets van dit plaatje is zichtbaar
            }

            const uint8_t* pIndices = static_cast<const uint8_t*>(image.pData);
            if (bIsFrame)
            {
                pIndices += (int32_t)(frameRow * drawHeight) * image.width + (int32_t)(frameCol * drawWidth);
            }

            const uint8_t indexTransparent = toPaletteIndex(image.colTransparent);
            uint8_t* pBuffer = (uint8_t*)_backBuffer.getPointer();

            for (int32_t y = yFrom; y < yTo; y++)
            {
                const uint8_t* pSrc = pIndices + y * image.width;
                uint8_t* pDst = pBuffer + (imagePos.y + y) * _widthPix + imagePos.x;

                if (!image.bHasTransparency)
                {
                    // De bytes zijn al schermindices, dus dit is letterlijk
                    // kopieren.
                    memcpy(pDst + xFrom, pSrc + xFrom, (size_t)(xTo - xFrom));
                    continue;
                }

                // Transparant: per aaneengesloten stuk zichtbare pixels een kopie.
                int32_t xRunStart = -1;
                for (int32_t x = xFrom; x <= xTo; x++)
                {
                    bool bVisible = (x < xTo) && (pSrc[x] != indexTransparent);

                    if (bVisible)
                    {
                        if (xRunStart < 0)
                        {
                            xRunStart = x;
                        }
                    }
                    else if (xRunStart >= 0)
                    {
                        memcpy(pDst + xRunStart, pSrc + xRunStart, (size_t)(x - xRunStart));
                        xRunStart = -1;
                    }
                }
            }
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

            // Twee stroken, en uit DMA-geschikt geheugen: terwijl de ene wordt
            // uitgezonden pakt de CPU de andere uit. Dat is de hele winst van DMA
            // hier - het uitzenden duurt 31,6 ms en het uitpakken 3,4, en die
            // hoeven niet op elkaar te wachten.
            for (int i = 0; i < 2; i++)
            {
                _arpStripBuffer[i] = (uint16_t*)heap_caps_malloc(sizePix.x * NofRowsPerStrip * sizeof(uint16_t),
                                                                MALLOC_CAP_DMA);
                if (_arpStripBuffer[i] == nullptr)
                {
                    ESP_LOGE("DisplayAdapter_PAL8", "geen strookbuffer; terug naar het paneel.");
                    _backBuffer.deleteSprite();
                    freeStripBuffers();
                    return;
                }
            }

            if (!checkCarrierColours())
            {
                _backBuffer.deleteSprite();
                freeStripBuffers();
                return;
            }

            // De eigen blit schrijft rechtstreeks in de buffer en rekent met de
            // breedte als stride. Dat klopt alleen zolang de sprite niet gedraaid
            // is - dan wisselt TFT_eSPI _iwidth en _dwidth om.
            if ((_backBuffer.getRotation() != 0) || (_backBuffer.getPointer() == nullptr))
            {
                ESP_LOGE("DisplayAdapter_PAL8", "onverwachte sprite-toestand (rotatie %d); "
                    "terug naar het paneel.", (int)_backBuffer.getRotation());
                _backBuffer.deleteSprite();
                freeStripBuffers();
                return;
            }

            _widthPix = sizePix.x;
            _heightPix = sizePix.y;


            _backBuffer.fillSprite(0);		// index 0 - wordt straks overtekend
            _bBackBufferAllocated = true;

            ESP_LOGI("DisplayAdapter_PAL8", "backbuffer %d x %d, 8 bpp paletted, %d bytes; "
                "dragerkleuren gecontroleerd", (int)sizePix.x, (int)sizePix.y, (int)(sizePix.x * sizePix.y));
        }

        // Vanaf hier gaat al het tekenwerk naar de buffer.
        void redirectToBackBuffer()
        {
            if (!_bBackBufferAllocated)
            {
                return;
            }

            Base::_pTarget = &_backBuffer;

            // Pas hier, want initDMA praat met de SPI-bus en het paneel moet dus
            // draaien. Lukt het niet, dan pusht present() gewoon blokkerend: alles
            // werkt dan nog, alleen zonder de overlap.
            if (!_bDmaTried)
            {
                _bDmaTried = true;
                // ctrl_cs = true: de DMA-engine beheert zelf de chip select.
                //
                // De eerste poging hield in plaats daarvan een startWrite() vast,
                // zoals het DMA-voorbeeld van TFT_eSPI doet, en toen bleef het
                // scherm zwart. Dat voorbeeld draait op de standaard VSPI-poort;
                // wij zitten op HSPI, die door de Arduino-SPI-laag wordt bediend.
                // Zolang die zijn transactie vasthoudt, vechten de Arduino-laag en
                // de IDF-driver om dezelfde peripheral. Met ctrl_cs regelt de
                // IDF-kant zowel de data als CS, en raken ze elkaar alleen nog
                // tussen de stroken door - waar dmaWait() ze uit elkaar houdt.
                _bDmaEnabled = this->_tft.initDMA(true);

                ESP_LOGI("DisplayAdapter_PAL8", "DMA %s",
                    _bDmaEnabled ? "aan: uitpakken en uitzenden overlappen" : "niet beschikbaar");
            }
        }

        void freeStripBuffers()
        {
            for (int i = 0; i < 2; i++)
            {
                if (_arpStripBuffer[i] != nullptr)
                {
                    heap_caps_free(_arpStripBuffer[i]);
                    _arpStripBuffer[i] = nullptr;
                }
            }
        }
    };
};
