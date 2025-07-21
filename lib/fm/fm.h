#ifndef FM_H
#define FM_H
#include <Arduino.h>
#include <soc/gpio_reg.h>

#include "SI5351.hpp"

// GPIO Assignment
#define D0 10
#define D1 11
#define D2 12
#define D3 13
#define D4 14
#define D5 21
#define D6 47
#define D7 48

#define A0 18
#define A1 8
#define WR 40
#define CS0 38
#define CS1 39
#define CS2 43
#define CS3 17
#define IC 9

// GPIO 0～31用のマクロ
#define A0_HIGH (REG_WRITE(GPIO_OUT_W1TS_REG, (1 << A0)))
#define A0_LOW (REG_WRITE(GPIO_OUT_W1TC_REG, (1 << A0)))
#define CS3_HIGH (REG_WRITE(GPIO_OUT_W1TS_REG, (1 << CS3)))
#define CS3_LOW (REG_WRITE(GPIO_OUT_W1TC_REG, (1 << CS3)))
#define A1_HIGH (REG_WRITE(GPIO_OUT_W1TS_REG, (1 << A1)))
#define A1_LOW (REG_WRITE(GPIO_OUT_W1TC_REG, (1 << A1)))
#define IC_HIGH (REG_WRITE(GPIO_OUT_W1TS_REG, (1 << IC)))
#define IC_LOW (REG_WRITE(GPIO_OUT_W1TC_REG, (1 << IC)))

// GPIO 32～48用のマクロ（ビット位置は32を引いた値を使用）
#define CS2_HIGH (REG_WRITE(GPIO_OUT1_W1TS_REG, (1 << (CS2 - 32))))
#define CS2_LOW (REG_WRITE(GPIO_OUT1_W1TC_REG, (1 << (CS2 - 32))))
#define WR_HIGH (REG_WRITE(GPIO_OUT1_W1TS_REG, (1 << (WR - 32))))
#define WR_LOW (REG_WRITE(GPIO_OUT1_W1TC_REG, (1 << (WR - 32))))
#define CS0_HIGH (REG_WRITE(GPIO_OUT1_W1TS_REG, (1 << (CS0 - 32))))
#define CS0_LOW (REG_WRITE(GPIO_OUT1_W1TC_REG, (1 << (CS0 - 32))))
#define CS1_HIGH (REG_WRITE(GPIO_OUT1_W1TS_REG, (1 << (CS1 - 32))))
#define CS1_LOW (REG_WRITE(GPIO_OUT1_W1TC_REG, (1 << (CS1 - 32))))

class FMChip {
 public:
  void begin();
  void reset();
  void setRegister(byte addr, byte value, int chipno);
  void setRegisterOPM(byte addr, byte value, uint8_t chipno);
  void setRegisterOPL3(byte port, byte addr, byte data, int chipno);
  void setYM2612(byte port, byte addr, byte data, uint8_t chipno);
  void setYM2612DAC(byte data, uint8_t chipno);
  void write(byte data, byte chipno, si5351Freq_t freq);
  void writeRaw(byte data, byte chipno, si5351Freq_t freq);

 private:
  u8_t _psgFrqLowByte = 0;
};

extern FMChip FM;
#endif