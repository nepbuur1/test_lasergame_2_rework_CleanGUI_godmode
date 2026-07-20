# CYD speed research
Potentiele verbeteringen:
- HSPI port naar gpio verbinden
- snellere SPI clock
- DMA activeren (voorkomt dat esp wacht tot spi transactie klaar is voordat hij verder gaat).

## HSPI port.
                                                                                                            ┌───────────────┬──────┬──────┐
  │      tak      │ MOSI │ SCLK │
  ├───────────────┼──────┼──────┤
  │ USE_HSPI_PORT │ 13   │ 14   │
  ├───────────────┼──────┼──────┤
  │ VSPI (huidig) │ 23   │ 18   │
  └───────────────┴──────┴──────┘

  De HSPI-defaults zijn precies jouw CYD-pinnen. Dat is geen toeval: 12/13/14/15 zijn de IOMUX-pinnen van HSPI op de
  ESP32. De CYD-ontwerper heeft de display bewust op HSPI's native pinnen gelegd.                                                                                                                                                                 Elke SPI-peripheral heeft een set pinnen waarmee hij rechtstreeks verbonden kan worden, buiten de GPIO-matrix om.
  Gebruik je die native pinnen met de bijbehorende controller, dan gaat het signaal direct — minder                       propagatievertraging, betere signaalintegriteit, en daardoor een hogere haalbare klok. Gebruik je ze met de andere      controller, dan moet alles alsnog door de matrix.                                                                                                                                                                                               Dat is wat er nu gebeurt: je draait de display op VSPI, terwijl de bedrading voor HSPI is ontworpen. Het wérkt prima —  de matrix routeert het netjes — maar je laat de directe route liggen.                                                                                                                                                                           Dus                                                                                                                                                                                                                                             #define USE_HSPI_PORT
                                                                                                                          verandert niets aan je bedrading en niets aan je pinconfiguratie. Het zegt alleen: gebruik de controller waar dit
  bordje voor bedoeld is. Vrijwel alle CYD-voorbeeldsetups die je online vindt hebben deze define er dan ook in staan.                                                                                                                            Eén ding om op te letten: de touch-XPT2046 op 32/33/25/39 hangt aan de andere poort. Als die na de omzetting stopt met  werken, zitten ze elkaar in de weg — maar met deze pinverdeling is dat onwaarschijnlijk, want ze delen geen enkele      lijn.                                                  