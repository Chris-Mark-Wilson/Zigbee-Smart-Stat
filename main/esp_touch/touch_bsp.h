#ifndef TOUCH_BSP_H
#define TOUCH_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_Touch_ADDR 0x15

void touch_Init(void);
uint8_t getTouch(uint16_t *x,uint16_t *y);
#ifdef __cplusplus
}
#endif

#endif
