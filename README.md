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

```menuconfig -> partition table -> set custom```

```menuconfig -> serial flasher config -> flash size (8MB, for esp32c6)```

This allows for partitions.csv to become the partition table over default

```menuconfig -> component config -> zigbee -> enable```

dont forget this bit to enable zigbee out of the box

``idf.py --version`` should be 5.3.2

`idf.py set-target esp32c6` to ensure correct chip

`idf.py fullclean build flash monitor` and hopefully it should run...

\s pause monitor
\q resume monitor

sometimes target conflict, use unset IDF_TARGET before retargetting




