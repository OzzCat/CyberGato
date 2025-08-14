Used modules:
1. Computing core and SD card - Wemos ESP32-S3 Pro
2. RFID reading - LIHTNE 134B
3. Coordinates - GY-GPSV3-NEO-M8N
4. Time counting - GSMIN DS3231
5. Power - ClimateGuard board based on ETA9640 for chargind and distribution and LiPo battery (nowadays up to 10000mah)
6. Power control - simple low-voltage voltmeter


Possible replacements:
1. Any ESP32 board, may need to buy and wire sd-card module separately
2. Tried out other chinese modules, any 3.3/5 should do the work (need to additionally check the output format), but those gave the best results. 9v modules require some tinkering with battery wiring as well
3. Any NEO dev board should be fine, 8 and 10 are better than older ones.
4. Again, any other RTC module, those were just cheap and reliable
5. A lot of options. Firstly, boards based on ETA9640/IP5306/BQ25895. We look for 3.3v for ESP/RTC/GPS and 3.3/5/9v for RFID module. Another possible option - using power source with LiIon 18650 batteries, for example diymore battery shields.
