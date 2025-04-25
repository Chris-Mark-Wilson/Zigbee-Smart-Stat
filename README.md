# esp32-c6-wroom-N8

# ACE number 1 smart stat for landlords

### instructions for use..

clone repo 

install esp-idf  
https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32c6/get-started/index.html#installation

once done, run install script, cd into repo
open terminal and run 

``. ~/esp/esp-idf/export.sh``

to get the python commands in your current shell

then run

```idf.py menuconfig```

and change the following

```menuconfig -> partition table -> set custom``` this should also be done in CMakeLists.txt in root but doesnt seem to be working

```menuconfig -> serial flasher config -> flash size (8MB, for esp32c6)```

This allows for partitions.csv to become the partition table over default, again trying to do this programmatically via sdkconfig.defaults but will see...

```menuconfig -> component config -> zigbee -> enable```

dont forget this bit to enable zigbee out of the box

``idf.py --version`` should be 5.3.2

`idf.py set-target esp32c6` to ensure correct chip

`idf.py fullclean build flash monitor` and hopefully it should run...

\s pause monitor
\q resume monitor

 if partition table still not found it could be residual shizzle on the chip from previous flashes.. so 
 ``idf.py erase_flash``

 ``idf.py flash monitor``

fuck knows, it just doesnt work depending on the environment, once youve got it going have a brew before carrrying on because if youve got to this point and are seeing 

I (23) boot: ESP-IDF v5.3.2 2nd stage bootloader

I (23) boot: compile time Apr 24 2025 15:07:27

I (24) boot: chip revision: v0.0

I (26) boot: efuse block revision: v0.2

I (30) boot.esp32c6: SPI Speed      : 80MHz

I (35) boot.esp32c6: SPI Mode       : DIO

I (40) boot.esp32c6: SPI Flash Size : 8MB

I (44) boot: Enabling RNG early entropy source...

I (50) boot: Partition Table:

I (53) boot: ## Label            Usage          Type ST Offset   Length

I (61) boot:  0 nvs              WiFi data        01 02 00009000 00006000

I (68) boot:  1 phy_init         RF data          01 01 0000f000 00001000

I (76) boot:  2 factory          factory app      00 00 00010000 0015e000

I (83) boot:  3 zb_storage       WiFi data        01 02 0016e000 00004000

I (91) boot: End of partition table

I (95) esp_image: segment 0: paddr=00010020 vaddr=420b0020 size=3c284h (246404) map

I (149) esp_image: segment 1: paddr=0004c2ac vaddr=40800000 size=03d6ch ( 15724) load

I (154) esp_image: segment 2: paddr=00050020 vaddr=42000020 size=a5168h (676200) map

I (282) esp_image: segment 3: paddr=000f5190 vaddr=40803d6c size=09ec0h ( 40640) load

I (292) esp_image: segment 4: paddr=000ff058 vaddr=4080dc30 size=021f4h (  8692) load

I (297) boot: Loaded app from partition at offset 0x10000

I (298) boot: Disabling RNG early entropy source...

I (312) cpu_start: Unicore app


trust me, you've done well because it took me 4 days of solid wtf's before i got to this point

sometimes there is a target conflict, use ``unset IDF_TARGET`` before retargetting

ok basic zigbee config done..

## Add a dht22 sensor...
add the following to idf_component.yml in main..


dht:

    git: "https://github.com/UncleRus/esp-idf-lib.git"
    path: "components/dht"
  ## Required by the dht library ->
  esp_idf_lib_helpers:
    git: "https://github.com/UncleRus/esp-idf-lib.git"
    path: "components/esp_idf_lib_helpers"
   

then..
``idf.py reconfigure``

then in smartStatMain.c
``#include "dht.h"``

more helpfull stuff...

if it wont flash, or erase - check to see if its already  running opn a port.

lsof /dev/ttyACM*

and kill -9 <pid> if it is...

ensure the correct version is being used esp-idf v5.3.2


   




