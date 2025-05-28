# Zigbee Smart Stat with esp32-c6-1.9-touch screen

# Smart, individual room, zigbee thermostat for HMO Landlords

# Still under development, slightly buggy but working MVP

### instructions for use..

Fork and clone, open in esp_idf

Set up on a breadboard to test, you will need (apart from the [esp32c6 touch (scroll down for pinout)](https://www.waveshare.com/esp32-c6-lcd-1.9.htm)) :- 

A dht22 temp and humidity sensor, take 3v to +ve, gnd, output to GPIO 3, with a 10k pull-up resistor between output and +ve [The Pi Hut, cheaper on Ali Express](https://thepihut.com/products/dht22-temperature-humidity-sensor?variant=39627167629507&country=GB&currency=GBP&utm_medium=product_sync&utm_source=google&utm_content=sag_organic&utm_campaign=sag_organic&gad_source=1&gad_campaignid=22549809780&gbraid=0AAAAADfQ4GGHHk97fJP2bI-DSh0Z7dJBI&gclid=CjwKCAjw6NrBBhB6EiwAvnT_rrohfnZETwP80opzOg0qV9o0eH8Cg_CkcTgWIiKqjLj-qetcH8UknxoCA4gQAvD_BwE)

A uart capable microwave presence sensor. (human micro motion detector) 3v power, gnd, connect tx pin to GPIO 17 (uart rx) using uart over straight On/OFF gives you *RANGE to target*... [The Pi Hut again](https://thepihut.com/products/human-micro-motion-detection-mmwave-sensor-24ghz?variant=43310263468227&country=GB&currency=GBP&utm_medium=product_sync&utm_source=google&utm_content=sag_organic&utm_campaign=sag_organic&gad_source=1&gad_campaignid=22549809780&gbraid=0AAAAADfQ4GGHHk97fJP2bI-DSh0Z7dJBI&gclid=CjwKCAjw6NrBBhB6EiwAvnT_rlzAqD1tx89wm_IWMKnPxCN7-5-k8wm8hujHqEo8SYzE_hlU1JyG3RoCzi4QAvD_BwE)

Window sensors (if required), I managed to get the [Tuya](https://www.aliexpress.com/item/1005006215113198.html?src=google&snpsid=1&pdp_npi=4%40dis!GBP!7.66!3.82!!!!!%402111896717483220653996495d0d92!12000036317325977!ppc!!!&snps=y&src=google&albch=shopping&acnt=742-864-1166&isdl=y&slnk=&plac=&mtctp=&albbt=Google_7_shopping&aff_platform=google&aff_short_key=UneMJZVf&gclsrc=aw.ds&&albagn=888888&&ds_e_adid=&ds_e_matchtype=&ds_e_device=c&ds_e_network=x&ds_e_product_group_id=&ds_e_product_id=en1005006215113198&ds_e_product_merchant_id=5551326180&ds_e_product_country=GB&ds_e_product_language=en&ds_e_product_channel=online&ds_e_product_store_id=&ds_url_v=2&albcp=22435797343&albag=&isSmbAutoCall=false&needSmbHouyi=false&gad_source=1&gad_campaignid=22432265180&gbraid=0AAAAA99aYpdeT4fnsrJenbfMImI99qsfm&gclid=CjwKCAjw6NrBBhB6EiwAvnT_rrPP5-NmmAz6rgm0TGpZyTE4YoLTlQeNV7L6igVGChl2lqXpjKWKdRoCjD8QAvD_BwE) ones to work..

A Zigbee TRV (or more than one if needed), I use [Sonoff TRVs](https://www.aliexpress.com/item/1005006304701422.html?src=google&pdp_npi=4%40dis!GBP!11.99!11.99!!!!!%40!12000036689723777!ppc!!!&src=google&albch=shopping&acnt=494-037-6276&isdl=y&slnk=&plac=&mtctp=&albbt=Google_7_shopping&aff_platform=google&aff_short_key=UneMJZVf&gclsrc=aw.ds&&albagn=888888&&ds_e_adid=&ds_e_matchtype=&ds_e_device=c&ds_e_network=x&ds_e_product_group_id=&ds_e_product_id=en1005006304701422&ds_e_product_merchant_id=105390795&ds_e_product_country=GB&ds_e_product_language=en&ds_e_product_channel=online&ds_e_product_store_id=&ds_url_v=2&albcp=17859500389&albag=&isSmbAutoCall=false&needSmbHouyi=false&gad_source=1&gad_campaignid=17190468917&gbraid=0AAAAADznYb8oa-5LLUyyL9AgPnnJ-qdUk&gclid=CjwKCAjw6NrBBhB6EiwAvnT_ruGg9toJBfGORUEunaz_FILTFZ8ALfvogWUtMXHp_WIOrqpm4GeAYRoC6B8QAvD_BwE)

A *REGULATED* step down 240v-5v transformer [amazon](https://www.amazon.co.uk/JZK-HLK-PM01-supply-module-isolated-220V-5V/dp/B073QH1XT8/ref=sr_1_16?dib=eyJ2IjoiMSJ9.BzZYYZhEGlm3qPYAzjpljeHJwXb1Hl5bcsM_4cxq8d0LCISV0JE_rm3w3W_ZkzyaZ0rCnphgLibITFq2GiqLFCaV_YRNuF8Ep3VjvkPlJSJiqxrKXTSXDvbo2JTL_8Rg3fi5OiIef9k9C_2XZhMKHojRC55JRYfgo2mHrpXUc2n0Jd1UJ7Ayb4RT-ZuX003gLNpv8G-9LanCURR4KRQYHkvzVClANg7vwGXhTWYLw_o.oZ6U9KH2tcU_V2ANbvEeO98iov4HISG2nzp7rTHY6LU&dib_tag=se&keywords=5v+transformer&qid=1748439831&sr=8-16) if you plan to run it on 240v mains but *BE CAREFULL*, also some screw terminals to wire the mains into, connect this to the **5 Volt** GPIO pin on the chip

A Case (enclosure) To assemble into, [these ones](https://thepihut.com/products/room-sensor-enclosure-size-1) will fit over a standard (76mm) uk single socket backbox or pattress

set target to esp32c6 and flash

**Current version, touch branch works on esp32c6 1.9" touch, development ceased for other versions**


# What is it for? #
### A solution to 'The Landlord HMO Problem' i.e. windows open, heating on. It massively reduces heating bills, not only by monitoring open window status, but also room occupancy, privately. ###
##

# What does it do? #
### If the window (or any windows in the room, paired to the thermostat via a zigbee window/door sensor) is/are open - TRV is turned OFF. No ifs, no buts. ###




### If the room becomes unoccupied for more than 10 minutes, set the trv to a lower target temp. ###
#### Rather than turn it off completely until the room is occupied again, it monitors the room temp and will keep the room at the LOWER limit while unoccupied. This is to reduce heating bills, but also because a 4-5 degree increase in temp when the room becomes occupied is easily reached quickly, wheras a 10 degree increase would take an uncomfortably long time.
####
### As soon as the thermostat registers occupation for more than 10 seconds (again, no false positives) it sets the target temp to the higher limit. ###

### So, if the room is below upper target temp AND the room is occupied AND the windows are closed, it will open the trv until target temp is reached and will monitor and keep it there. ###
### If the room is unoccupied AND the windows are closed it will keep the room at the lower target temp ###
### If you would like to allow 'night vent' style ventilation (window very slightly open) then position the window sensor in such a way as not to trigger the open condition until the window passes a certain point ### 
##

## How does it do that then? ##
### The thermostat uses the Zigbee IEEE 802.15.4 protocol to communicate with Zigbee TRV's (thermostatic radiator valves) and Zigbee window/door sensors (just zigbee enabled reed switches that broadcast their on/off state to the thermostat).

### It utilises an hmmd microwave presence detector (not a PIR) hardwired to the chip along with a dht22 temperature and humidity sensor also hardwired to the chip.

## What about privacy? ##
### Tennants don't like having their presence status broadcast to all and sundry over the 'interwebs' - might get hacked, or robbed, anyway I'm not having that in my room etc etc etc.. ###
###
### It doesnt connect to the internet in any way, it uses its own locally broadcast network, totally separate from anything network based, such as the house router. ###
### The only thing that knows the presence status of the room (apart from the occupant) is the chip itself, because the presence detector is hardwired to the thermostat itself, nothing presence related is broadcast anywhere, so it cant be hacked, or eavesdropped ###



# How do I use it? #

### Settings screen ###
~~~
On first boot (or long press boot button), the settings screen is shown. There are 4 sliders.

High temp: the upper target temp for when room is occupied.

Low temp: The target temp when the room is UNoccupied.

Presence range: Basically, the distance to the opposite wall. As the microwave presence sensor can see through walls, this is to stop false positives.

Room Number: To be used when collating data.

Save button / Cancel button: On save, the settings are saved into non volatile storage (NVS) so that a power cut will not remove the settings. On cancel the default settings are applied but nothing is saved (maybe a todo) so will boot back into settings if power is cut and no settings are saved.
~~~

### Boot screen ###
~~~
If settings are found (or saved from settings screen), the thermostat will start the zigbee stack.

Once the network is formed, the thermostat will check NVS for stored (paired) devices and load these from memory (again, so we dont have to re-pair devices after power cut)

The device check includes a test for TRV devices. If none are found (even if there are window sensors), the thermostat will open the network for pairing for 3 minutes and display a message telling the user to pair devices.

If a TRV device is found in memory, it is assumed all devices are paired and the flow continues to main screen.

If at this point you have forgotten to pair any required window sensors, long press the boot button to unpair any existing devices and clear NVS before rebooting in factory mode.
~~~

### Main screen ###
~~~
The main screen is just the display for the occupant. There are 3 sections.

Current temp in degrees celcius (to a tenth of a degree)
   
Relative humidity (rh%)

Window status/Presence detected icons. The window will be OPEN if any windows are open and the presence icon will be SOLID when presence detected, hollow when not. These are just indicators. The actual range to a detected person/moving object is shown underneath the presence icon in metres to 1 decimal place (accuracy 10cm)
~~~


### TODO ###
Currently still under development.

Control logic...

Add better indication when devices pair e.g. show device list with name and type as they join the network

Add a Status Screen to be tapped from main that shows device list with battery states

Add a Log Screen to be tapped in sequence, showing total time ON and OFF

Include more globals and NVS storage for these logs

Add a temp offset for when its mounted in a case, just an ENUM will do, to suit the enclosure, perceived temp will rise when enclosed but previous iterations have been stable once mounted.



