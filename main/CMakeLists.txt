# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )

set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS 
"main.cpp" 
"../libs/hx711/src/HX711.cpp"
"../libs/CleanCore/src/crt_FromArduinoIDE.cpp"
"../libs/CleanRTOS/src/crt_MainInits.cpp" 

"../libs/esp_http_client/main/esp_http_client_example.c"

## TFT_.. some incompatible stuff, it seems, for esp32-c6
## perhaps easily fixed (some constant names?)
"../libs/TFT_eSPI/TFT_eSPI.cpp"
"../libs/TFT_eWidget/src/TFT_eWidget.cpp"
"../libs/TFT_eWidget/src/widgets/button/ButtonWidget.cpp"

# "../libs/esp_littlefs/src/esp_littlefs.c"
# "../libs/esp_littlefs/src/lfs_config.c"
# "../libs/esp_littlefs/src/littlefs_api.c"

## esp_littlefs lijkt niet helemaal compatible met esp32-c6 en/of zigbee..
## aan de andere kant lijkt er een joltwallet/littlefs standaard bij te zitten.
"../libs/LITTLEFS/src/esp_littlefs.c"
"../libs/LITTLEFS/src/lfs_util.c"
"../libs/LITTLEFS/src/littlefs_api.c"
"../libs/LITTLEFS/src/lfs.c"
"../libs/LITTLEFS/src/LITTLEFS.cpp"

# StateMachineTemplatePro (kies hem in main.cpp met StateMachineTemplatePro.ino).
# Deze drie worden in elke build meegecompileerd, maar ze bevatten geen globale
# objecten; zolang de .ino niet gekozen is, gooit de linker ze er weer uit.
"../apps/StateMachineTemplatePro/app/Counter.cpp"
"../apps/StateMachineTemplatePro/app/MachineControl.cpp"
"../apps/StateMachineTemplatePro/app/testHelpers/InputSimulator.cpp"

#"../libs/hwlib/hwlib.cpp"
#"../libs/hwlib/peripherals/hwlib-glcd-oled.cpp"
#"../libs/hwlib/char-io/hwlib-ostream.cpp"

# Adafruit libs for driving SSD1306 OLED:
# "../libs/Adafruit_SSD1306/Adafruit_SSD1306.cpp"
# "../libs/Adafruit-GFX-Library/Adafruit_GFX.cpp"
# "../libs/Adafruit_BusIO/Adafruit_BusIO_Register.cpp"
# "../libs/Adafruit_BusIO/Adafruit_I2CDevice.cpp"
)

set(COMPONENT_ADD_INCLUDEDIRS 
"."
"../libs/CleanRTOS/src" 
"../libs/CleanRTOS/src/internals" 
"../libs/CleanRTOS/examples/AllWaitables"
"../libs/CleanRTOS/examples/Flag"
"../libs/CleanRTOS/examples/Queue"
"../libs/CleanRTOS/examples/Timer"
"../libs/CleanRTOS/examples/HelloWorld"
"../libs/CleanRTOS/examples/TwoTasks"
"../libs/CleanRTOS/examples/MutexSection"
"../libs/CleanRTOS/examples/Pool"
"../libs/CleanRTOS/examples/Handler"
"../libs/CleanRTOS/examples/Logger"
"../libs/CleanRTOS/examples/TenTasks"
"../libs/CleanRTOS_extraTests/examples/HasFired"
"../libs/CleanRTOS_extraTests/examples/Mutex"
"../libs/CleanRTOS_extraTests/examples/Queue2"
"../libs/CleanCore/src"
"../libs/CleanCore/examples/StdQueue"
"../libs/CleanCore/examples/Button"
"../libs/CYD_tools_crt/src/ADC_buttons"
"../libs/CYD_tools_crt/examples/ADC_buttons"
"../libs/CYD_tools_crt/tests/TestWorstCaseVoltageToBorderDistance"
"../libs/CYD_tools_frt/src/ADC_buttons"
"../libs/CYD_tools_frt/examples/ADC_buttons"
"../libs/LaserGameUtils/src"
"../libs/LaserGameUtils/examples/ClockPin"
"../libs/hx711/src"
"../libs/hx711/tests"

# TFT_.. some incompatible stuff, it seems, for esp32-c6
"../libs/TFT_eSPI"
"../libs/TFT_eSPI/TFT_Drivers"
"../libs/TFT_eSPI/Fonts"
"../libs/TFT_eSPI/Extensions"
"../libs/TFT_eSPI/examples/480x320/Free_Font_Demo"
"../libs/TFT_eWidget/src"
"../libs/TFT_eWidget/src/widgets/button"
"../libs/TFT_eWidget/examples/Buttons/Button_demo"
"../libs/TFT_eSPI/examples/480x320/Touch_Controller_Demo"

#"../libs/esp_littlefs/src"

## esp_littlefs lijkt niet helemaal compatible met esp32-c6 en/of zigbee..
"../libs/LITTLEFS/src"
"../libs/LITTLEFS/examples/LITTLEFS_test"

"../libs/CleanGUI/src"
"../libs/CleanGUI/src/internals"
"../libs/CleanGUI_async/src"
"../libs/CleanGUI_async/src/internals"
"../libs/CleanGUI_extraTests/examples/Vec2"

"../libs/CleanGUI_for_TFT_eSPI/examples/Panel"
"../libs/CleanGUI_for_TFT_eSPI/examples/AsyncDisplay"
"../libs/CleanGUI_for_TFT_eSPI/examples/TouchCalibrateAll"
"../libs/CleanGUI_for_TFT_eSPI/examples/TouchscreenButton"
"../libs/CleanGUI_for_TFT_eSPI/examples/TouchscreenButtonGroup"
"../libs/CleanGUI_for_TFT_eSPI/examples/TouchscreenKeyboard"
"../libs/CleanGUI_for_TFT_eSPI/examples/Inputfield"
"../libs/CleanGUI_for_TFT_eSPI/examples/ImageViewer"
"../libs/CleanGUI_for_TFT_eSPI/examples/MultiPanel"
"../libs/CleanGUI_for_TFT_eSPI"

# Geen voorbeeld om van te leren maar een zelftest: hij legt vast wat show,
# enable en tekenen elk wel en niet doen, en faalt als dat verschuift.
"../libs/CleanGUI_for_TFT_eSPI/internal/tests/ShowEnableSemantics"

"../libs"
#"../libs/hwlib"
#"../libs/hwlib/core"
#"../libs/hwlib/graphics"
#"../libs/hwlib/pins"
#"../libs/hwlib/targets"
#"../libs/hwlib/peripherals"
#"../libs/hwlib/char-io"

# Belangrijk voor Arduino IDE code: de output van Serial Monitor
# wordt naar een van de USB-ports van je devkit gestuurd. 
# Als je toevallig programmeert via de andere, dan zie je niets.
#"../components/arduino-esp32/libraries/ESP32/examples/Analogread"
#"../components/arduino-esp32/libraries/WiFi/examples/WifiAccessPoint"
#"../components/arduino-esp32/libraries/WiFi/examples/WifiScan"
#"../components/arduino-esp32/libraries/LittleFS/examples/LITTLEFS_test"

"../apps/kopie_van_wifi_scan_example_van_esp_idf"
"../libs/esp_http_client/main"

# Alle vier de pong-varianten mogen samen in het includepad staan: main.cpp
# kiest met zijn #include van de .ino welke ervan er gebouwd wordt, en een
# includepad op zichzelf compileert niets.
#
# Hier stond dat apps/pong_ascii_quest_with_3_bugs (toen nog apps/pong/pong) zou botsen omdat die nog de kale crt_-namen
# gebruikt. Dat klopt niet (meer): die vijf headers - crt_BallControl.h,
# crt_CharMatrix.h, crt_GameControl.h, crt_PaddleControl.h en
# crt_SceneDisplayControl.h - komen nergens anders in de boom voor, en hun
# klassen ook niet. De drie CleanGUI-varianten dragen elk hun eigen prefix
# (psim_, pdg_, psup_) en hun eigen namespace, dus onderling botsen ze evenmin.
# Nagegaan op 2026-08-12 door beide richtingen te bouwen: Pong_super
# met dit pad aan, en Pong2 met de drie andere paden aan. Allebei groen.
"../apps/pong_ascii_quest_with_3_bugs"
"../apps/pong_simple"
"../apps/pong_super"

# apps_frt: dezelfde apps, maar de eigen klassen op vanilla FreeRTOS in plaats van
# CleanRTOS. Eigen bestandsnamen (psupf_, Pong_super_frt) en een eigen namespace,
# zodat ze naast de CleanRTOS-versie op het includepad kunnen staan.
"../apps_frt/pong_super_frt"
"../apps_frt/pong_simple_frt"

# Gereedschap bij die laatste: losse programma's die aan de opslag op het
# bord komen, elk in een eigen map. Zie de README daar. Ook hier geldt dat
# een includepad op zichzelf niets compileert - main.cpp kiest.
"../apps/pong_super/tools/ClearHallOfFame"
"../apps/pong_super/tools/ClearCalibrations"

# StateMachineTemplatePro: de .ino staat in de map zelf, en de bronnen includen
# elkaar als "app/....hpp", dus de map zelf is het includepad.
"../apps/StateMachineTemplatePro"

# DisplayId: leest het ID van de displaycontroller uit (ILI9341 of ST7789?) en
# tekent per rotatie een testpatroon.
"../apps/DisplayId"

#"../apps/TestHwLibGlcOled"
# Adafruit libs for driving SSD1306 OLED:
# "../libs/Adafruit_SSD1306"
# "../libs/Adafruit-GFX-Library"
# "../libs/Adafruit_BusIO"
# "../libs/Adafruit_SSD1306/examples/van_jan"
# "../libs/Adafruit_SSD1306/examples/ssd1306_128x32_i2c"
# "../libs/Adafruit_SSD1306/examples/ssd1306_128x64_i2c"
)

register_component()
