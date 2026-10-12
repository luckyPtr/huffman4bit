#include "ssd1306.h"
#include "main.h"
#include "spi.h"

#define OLED_SPI_TIMEOUT  100U  /* ms; blocking transfers only */

#define OLED_CS_LOW()   HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_RESET)
#define OLED_CS_HIGH()  HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_SET)
#define OLED_DC_LOW()   HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_RESET)
#define OLED_DC_HIGH()  HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_SET)
#define OLED_RST_LOW()  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_RESET)
#define OLED_RST_HIGH() HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_SET)

/* Every byte of a command sequence is sent with D/C low - the SSD1306
 * treats the whole sequence as its command stream. */
static void oled_write_cmd(const uint8_t *seq, uint16_t len)
{
  OLED_DC_LOW();
  OLED_CS_LOW();
  HAL_SPI_Transmit(&hspi1, (uint8_t *)seq, len, OLED_SPI_TIMEOUT);
  OLED_CS_HIGH();
}

/* flat init table; pairs are "command, argument" where one follows */
static const uint8_t oled_init_seq[] = {
  0xAE,             /* display off */
  0xD5, 0x80,       /* clock divide = default ratio */
  0xA8, 0x3F,       /* multiplex ratio = 64 lines */
  0xD3, 0x00,       /* display offset = 0 */
  0x40,             /* display start line = 0 */
  0x8D, 0x14,       /* charge pump enable */
  0x20, 0x00,       /* horizontal addressing: frame = one contiguous burst */
  0xA1,             /* segment remap: col 127 -> SEG0 */
  0xC8,             /* COM scan direction: remapped */
  0xDA, 0x12,       /* COM pins: alternative config (128x64) */
  0x81, 0xCF,       /* contrast */
  0xD9, 0xF1,       /* pre-charge period */
  0xDB, 0x40,       /* VCOMH deselect level ~0.77 x Vcc */
  0xA4,             /* display follows RAM content */
  0xA6              /* normal mapping: 1 = lit pixel */
};

static void oled_hw_reset(void)
{
  OLED_RST_HIGH();
  HAL_Delay(10);
  OLED_RST_LOW();
  HAL_Delay(10);    /* datasheet wants >= 3 us low */
  OLED_RST_HIGH();
  HAL_Delay(100);   /* charge pump needs time before init commands */
}

void ssd1306_init(void)
{
  oled_hw_reset();
  oled_write_cmd(oled_init_seq, (uint16_t)sizeof(oled_init_seq));
  ssd1306_clear();            /* RAM content is random after power-up */
  ssd1306_display_on(1);
}

void ssd1306_display_on(uint8_t on)
{
  uint8_t cmd = (on != 0U) ? 0xAFU : 0xAEU;
  oled_write_cmd(&cmd, 1);
}

void ssd1306_set_contrast(uint8_t value)
{
  uint8_t cmd[2] = {0x81, value};
  oled_write_cmd(cmd, 2);
}

void ssd1306_invert(uint8_t on)
{
  uint8_t cmd = (on != 0U) ? 0xA7U : 0xA6U;
  oled_write_cmd(&cmd, 1);
}

void ssd1306_frame_begin(void)
{
  static const uint8_t window[] = {
    0x21, 0x00, 0x7F,         /* column range 0..127 */
    0x22, 0x00, 0x07          /* page range 0..7 */
  };

  oled_write_cmd(window, (uint16_t)sizeof(window));
  OLED_DC_HIGH();             /* everything until frame_end() is data */
  OLED_CS_LOW();
}

void ssd1306_frame_end(void)
{
  OLED_CS_HIGH();
}

void ssd1306_write_byte(uint8_t byte)
{
  HAL_SPI_Transmit(&hspi1, &byte, 1, OLED_SPI_TIMEOUT);
}

void ssd1306_write_data(const uint8_t *buf, uint16_t len)
{
  HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, len, OLED_SPI_TIMEOUT);
}

void ssd1306_blit(const uint8_t *frame)
{
  ssd1306_frame_begin();
  ssd1306_write_data(frame, SSD1306_FRAME_LEN);
  ssd1306_frame_end();
}

void ssd1306_fill(uint8_t pattern)
{
  uint16_t i;

  ssd1306_frame_begin();
  for (i = 0U; i < SSD1306_FRAME_LEN; i++)
  {
    ssd1306_write_byte(pattern);
  }
  ssd1306_frame_end();
}

void ssd1306_clear(void)
{
  ssd1306_fill(0x00U);
}

void ssd1306_stream_byte(void *ctx, uint8_t byte)
{
  (void)ctx;
  ssd1306_write_byte(byte);
}
