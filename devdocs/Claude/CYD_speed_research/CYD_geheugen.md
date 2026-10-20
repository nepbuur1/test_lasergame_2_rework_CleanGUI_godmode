Je legt hier precies de vinger op de zere plek van het klassieke **CYD-bordje**.

Het originele CYD-bord (ESP32-2432S028R) gebruikt namelijk **geen ESP32-S3**, maar de oude **ESP32-WROOM-32**. En die module heeft een cruciale beperking: **hij heeft 0 bytes PSRAM aan boord**.

Bovendien spelen hier twee technische mechanismen waarom het misgaat:

### 1. De rekensom achter de schermbuffer (en waarom 24bpp faalt)

Het scherm op de CYD heeft een resolutie van **320 × 240 pixels**:

- **Bij 24bpp (3 bytes per pixel - RGB888):**
  
  $$320 \times 240 \times 3\text{ bytes} = \mathbf{230.400\text{ bytes}} \approx \mathbf{225\text{ kB}}$$

- **Het probleem:** De basis-ESP32 heeft in theorie 520 kB SRAM, maar na het laden van WiFi, Bluetooth, de FreeRTOS heap en overhead blijft er vaak maar **160 tot 200 kB aaneengesloten heap** over.

- Een vol schermbuffer in 24bpp past fysiek simpelweg niet in het vrije geheugen.

### 2. "Stack" vs. "Heap"

Als je probeert een schermbuffer aan te maken als lokale variabele (op de **stack**):

C++

```
// FOUT: Dit blaast de stack direct op (Guru Meditation / Stack Overflow Crash)
void drawScreen() {
    uint8_t buffer[320 * 240 * 3]; 
}
```

In FreeRTOS op de ESP32 is de default stack van een task vaak maar **4 kB tot 8 kB**. Een array van 225 kB op de stack zetten resulteert direct in een crash.

Zelfs als je dynamisch op de **heap** alloceert (`malloc`), faalt het op een gewone CYD door geheugentekort:

C++

```
uint8_t* buffer = (uint8_t*) malloc(320 * 240 * 3); // Geeft NULL terug op een standaard CYD!
```

### Hoe lost men dit op een CYD op?

Als je vastzit aan het standaard CYD-bordje zonder PSRAM, zijn er twee standaardtrucs die libraries zoals **TFT_eSPI** en **LVGL** gebruiken:

#### A. Schakel terug naar 16bpp (RGB565)

Vrijwel geen enkele ILI9341-driver gebruikt 24bpp; de hardware-standaard voor deze TFT-controllers is **RGB565 (16-bit / 2 bytes per pixel)**:

$$320 \times 240 \times 2\text{ bytes} = \mathbf{153.600\text{ bytes}} \approx \mathbf{150\text{ kB}}$$

Dit past nét op de heap als je geen zware WiFi-stacks hebt draaien.

#### B. Gebruik "Sub-buffering" / "Line buffers" (de LVGL-manier)

Je hoeft nooit het hele scherm tegelijk in het geheugen te houden. Je reserveert bijvoorbeeld een buffer voor **1/10e van het scherm** (bijv. 320 × 24 pixels = 15 kB):

1. Render de bovenste 24 regels in de 15 kB buffer.

2. Schiet deze 15 kB via SPI DMA naar het scherm.

3. Terwijl de hardware verstuurt, berekent de CPU alvast de volgende 24 regels.

### Waarom de ESP32-S3 dit verschil maakt

Er bestaan inmiddels nieuwere varianten van het "Cheap Yellow Display" (zoals de **CYD 2-USB**, vaak aangeduid met **ESP32-S3-2432S028** of de grotere varianten met capacitive touch).

- Die nieuwere modellen bevatten een **ESP32-S3 module met 2 MB of 8 MB PSRAM ingebouwd**.

- Pas dán werkt `ps_malloc(320 * 240 * 3)` wél probleemloos, omdat je 8.000 kB extra werkgeheugen hebt in plaats van 160 kB.



op de module van mijn cyd staat: XH-32S

*Sul modulo del mio CYD c'è scritto: XH-32S.*

Dat verklaart direct waarom je tegen geheugenlimieten aanloopt.

De **XH-32S** is een directe Chinese kloon/variant van de klassieke **ESP32-WROOM-32** (gebaseerd op de originele dual-core Xtensa LX6 ESP32-D0WD-chip).

Hier zijn de exacte eigenschappen van de XH-32S module op jouw bord:

- **Geen PSRAM:** De XH-32S heeft **0 bytes PSRAM** aan boord. Hij heeft enkel 4 MB (of soms 8 MB) externe SPI Flash voor de programmacode.

- **Intern SRAM:** Slechts de standaard 520 kB, waarvan na aftrek van het FreeRTOS-systeem, hardware-buffers en WiFi/Bluetooth meestal maar circa **150 tot 200 kB aaneengesloten RAM (heap)** overblijft.

- **Geen ESP32-S3:** Het is de oudere generatie ESP32, dus de architectuur ondersteunt ook niet de geavanceerde octal-PSRAM van de nieuwere S3-borden.

op een andere staat esp-32S ISM2.4G 802.11/b/g/n

*Su un altro c'è scritto: esp-32S ISM2.4G 802.11/b/g/n.*

Ook deze module is helaas **exact dezelfde categorie als de XH-32S**: het is de bekende **ESP-32S module van fabrikant Ai-Thinker** (eveneens een 1-op-1 clone/equivalent van de standaard Espressif ESP-WROOM-32).

### Eigenschappen van de ESP-32S (Ai-Thinker)

- **PSRAM:** **Niet aanwezig (0 bytes)**.

- **Flash:** 4 MB SPI Flash (puur voor firmware en eventueel LittleFS/SPIFFS bestandssysteem).

- **RAM:** Slechts het interne SRAM van ~520 kB (waarvan netto slechts ~160–200 kB vrije heap overblijft).

- **SoC:** De originele dual-core ESP32 (Xtensa LX6), geen ESP32-S3.

Zowel de **XH-32S** als de **ESP-32S** zijn dus de "kale" basisvarianten zonder extra RAM.





kun je links geven naar CYD met zoveel mogelijk extra PSRAM?

*Puoi fornire link a CYD con la maggior quantità possibile di PSRAM extra?*

Directe webwinkel-links (zoals op AliExpress of Amazon) veranderen voortdurend van verkoper en product-ID, maar hier zijn de **officiële open-source projectpagina's, community-databases en de exacte zoektermen/modelnummers** waarmee je direct de juiste modellen met maximaal **8 MB PSRAM** vindt:

### 1. Officiële community & hardware-documentatie (Links)

- **[ESP32 Cheap Yellow Display GitHub repository (door witnessmenow / Brian Lough)](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)**
  
  De centrale community-hub voor CYD. Onder de sectie *"Alternative / S3 models"* worden alle varianten bijgehouden inclusief pinouts en specificaties.

- **[Makerfabs ESP32-S3 Parallel TFT met Touch](https://www.google.com/search?q=https://www.makerfabs.com/esp32-s3-parallel-tft-with-touch-3-5-inch.html)**
  
  De bekendste kwalitatieve fabrieksvariant van het concept: uitgerust met een **ESP32-S3-WROOM-1-N16R8** (16 MB Flash + **8 MB Octal PSRAM**).

- **[Sunton Display Boards documentatie & repo's](https://www.google.com/search?q=https://github.com/search%3Fq%3DESP32-S3-2432S028)**
  
  "Sunton" is de oorspronkelijke Chinese fabrikant achter de meeste CYD-borden. Zoeken op hun modelnummers leidt direct naar de fabrieksstores.

### 2. Modellen met maximaal PSRAM (8 MB) om naar te zoeken

Let bij aankoop op AliExpress, Banggood of Amazon specifiek op de **"S3"** aanduiding en de vermelding **"8M PSRAM" / "N16R8"**:

| Modelcode                               | Schermdiagonaal | Resolutie                  | Controller & Geheugen                                                         |
| --------------------------------------- | --------------- | -------------------------- | ----------------------------------------------------------------------------- |
| **ESP32-S3-2432S028** *(CYD S3 versie)* | 2,8 inch        | 320 × 240 (ST7789/ILI9341) | **ESP32-S3 met 8 MB Octal PSRAM** + 16 MB Flash (vaak met twee USB-C poorten) |
| **ESP32-8048S050 / S070** *(Sunton S3)* | 5,0 of 7,0 inch | 800 × 480 (RGB parallel)   | **ESP32-S3 met 8 MB PSRAM** (geschikt voor complete 16bpp/24bpp buffers)      |
| **ESP32-4827S043**                      | 4,3 inch        | 480 × 272 (RGB parallel)   | **ESP32-S3 met 8 MB PSRAM**                                                   |

### 3. Hoe controleer je vóór aankoop of er echt PSRAM op zit?

- Kijk op de productfoto's naar het **metalen kapje van de ESP32**:
  
  Er moet expliciet **ESP32-S3-WROOM-1-N8R8** of **N16R8** op staan.
  
  *(De code **R8** staat voor 8 MB PSRAM; **R2** staat voor 2 MB; staat er alleen **N4** of **N8** zonder 'R', dan zit er **0 bytes** PSRAM in).*

- De modellen met twee USB-poorten (meestal 2× USB-C) zijn vrijwel altijd de nieuwere S3-versies met PSRAM, terwijl de klassieke CYD met Micro-USB + Type-C de oude WROOM-module zonder PSRAM heeft.


Zelf gevonden:

- [Jingcai ESP32-8048S050C-I - 5 inch IPS Display 800*480 pixels with Capacitive Touchscreen - ESP32-S3 - ESP32-8048S050C-I](https://www.tinytronics.nl/en/development-boards/microcontroller-boards/with-wi-fi/jingcai-esp32-8048s050c-i-5-inch-ips-display-800*480-pixels-with-capacitive-touchscreen-esp32-s3)
  5 inch, 35 euro

- [Jingcai ESP32-4827S043C - 4.3 inch TFT-LCD Display 480*272 pixels with Capacitive Touchscreen - ESP32-S3 - ESP32-4827S043C](https://www.tinytronics.nl/en/development-boards/microcontroller-boards/with-wi-fi/jingcai-esp32-4827s043c-4.3-inch-tft-lcd-display-480*272-pixels-with-capacitive-touchscreen-esp32-s3)
  4.3 inch, 25 euro inc btw.


