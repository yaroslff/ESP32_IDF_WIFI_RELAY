#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ssd1306.h"
#include "font8x8_basic.h"

#define PACK8 __attribute__((aligned( __alignof__( uint8_t ) ), packed ))

typedef union out_column_t {
    uint32_t u32;
    uint8_t  u8[4];
} PACK8 out_column_t;

esp_err_t ssd1306_init(SSD1306_t * dev, int width, int height)
{
    esp_err_t res;
    if (dev->_address == SPI_ADDRESS) {
        res = spi_init(dev, width, height);
    } else {
        res = i2c_init(dev, width, height);
    }
    // Инициализация внутреннего буфера нулями
    for (int i=0; i < dev->_pages; i++) {
        memset(dev->_page[i]._segs, 0, 128);
    }
    return res;
}

int ssd1306_get_width(SSD1306_t * dev) { return dev->_width; }
int ssd1306_get_height(SSD1306_t * dev) { return dev->_height; }
int ssd1306_get_pages(SSD1306_t * dev) { return dev->_pages; }

void ssd1306_show_buffer(SSD1306_t * dev)
{
    if (dev->_address == SPI_ADDRESS) {
        for (int page=0; page < dev->_pages; page++) {
            spi_display_image(dev, page, 0, dev->_page[page]._segs, dev->_width);
        }
    } else {
        for (int page=0; page < dev->_pages; page++) {
            i2c_display_image(dev, page, 0, dev->_page[page]._segs, dev->_width);
        }
    }
}

void ssd1306_set_buffer(SSD1306_t * dev, const uint8_t * buffer)
{
    int index = 0;
    for (int page=0; page < dev->_pages; page++) {
        memcpy(&dev->_page[page]._segs, &buffer[index], 128);
        index += 128;
    }
}

void ssd1306_get_buffer(SSD1306_t * dev, uint8_t * buffer)
{
    int index = 0;
    for (int page=0; page < dev->_pages; page++) {
        memcpy(&buffer[index], &dev->_page[page]._segs, 128);
        index += 128;
    }
}

void ssd1306_set_page(SSD1306_t * dev, int page, const uint8_t * buffer)
{
    if (page < dev->_pages) memcpy(&dev->_page[page]._segs, buffer, 128);
}

void ssd1306_get_page(SSD1306_t * dev, int page, uint8_t * buffer)
{
    if (page < dev->_pages) memcpy(buffer, &dev->_page[page]._segs, 128);
}

void ssd1306_display_image(SSD1306_t * dev, int page, int seg, const uint8_t * images, int width)
{
    if (page >= dev->_pages || seg >= dev->_width) return;
    
    if (dev->_address == SPI_ADDRESS) {
        spi_display_image(dev, page, seg, images, width);
    } else {
        i2c_display_image(dev, page, seg, images, width);
    }
    // Сохраняем отправленное в теневой буфер
    memcpy(&dev->_page[page]._segs[seg], images, width);
}

// -------------------------------------------------------------------------
// ОПТИМИЗАЦИЯ: Отправка всей строки текста одной транзакцией
// -------------------------------------------------------------------------
void ssd1306_display_text(SSD1306_t * dev, int page, const char * text, int text_len, bool invert)
{
    if (page >= dev->_pages) return;
    int _text_len = (text_len > 16) ? 16 : text_len;

    uint8_t line_buffer[128]; // Локальный буфер для всей строки
    int seg = 0;

    for (int i = 0; i < _text_len; i++) {
        memcpy(&line_buffer[seg], font8x8_basic_tr[(uint8_t)text[i]], 8);
        if (invert) ssd1306_invert(&line_buffer[seg], 8);
        if (dev->_flip) ssd1306_flip(&line_buffer[seg], 8);
        seg += 8;
    }

    if (seg > 0) {
        // Вызов одной транзакции вместо 16!
        ssd1306_display_image(dev, page, 0, line_buffer, seg);
    }
}

// -------------------------------------------------------------------------
// ОПТИМИЗАЦИЯ: Очистка экрана одной командой заливки (без шрифтов)
// -------------------------------------------------------------------------
void ssd1306_clear_screen(SSD1306_t * dev, bool invert)
{
    uint8_t clear_buffer[128];
    memset(clear_buffer, invert ? 0xFF : 0x00, 128);
    for (int page = 0; page < dev->_pages; page++) {
        ssd1306_display_image(dev, page, 0, clear_buffer, 128);
    }
}

// -------------------------------------------------------------------------
// ОПТИМИЗАЦИЯ: Очистка линии заливкой нулями
// -------------------------------------------------------------------------
void ssd1306_clear_line(SSD1306_t * dev, int page, bool invert)
{
    if (page >= dev->_pages) return;
    uint8_t clear_buffer[128];
    memset(clear_buffer, invert ? 0xFF : 0x00, 128);
    ssd1306_display_image(dev, page, 0, clear_buffer, 128);
}

// Функции для бегущей строки оставляем без изменений 
void ssd1306_display_text_box1(SSD1306_t * dev, int page, int seg, const char * text, int box_width, int text_len, bool invert, int delay)
{
    if (page >= dev->_pages) return;
    int text_box_pixel = box_width * 8;
    if (seg + text_box_pixel > dev->_width) return;

    int _seg = seg;
    uint8_t image[8];
    for (int i = 0; i < box_width; i++) {
        memcpy(image, font8x8_basic_tr[(uint8_t)text[i]], 8);
        if (invert) ssd1306_invert(image, 8);
        if (dev->_flip) ssd1306_flip(image, 8);
        ssd1306_display_image(dev, page, _seg, image, 8);
        _seg += 8;
    }
    vTaskDelay(delay);

    for (int _text=box_width; _text < text_len; _text++) {
        memcpy(image, font8x8_basic_tr[(uint8_t)text[_text]], 8);
        if (invert) ssd1306_invert(image, 8);
        if (dev->_flip) ssd1306_flip(image, 8);
        for (int _bit=0; _bit < 8; _bit++) {
            for (int _pixel=0; _pixel < text_box_pixel; _pixel++) {
                dev->_page[page]._segs[_pixel+seg] = dev->_page[page]._segs[_pixel+seg+1];
            }
            dev->_page[page]._segs[seg+text_box_pixel-1] = image[_bit];
            ssd1306_display_image(dev, page, seg, &dev->_page[page]._segs[seg], text_box_pixel);
            vTaskDelay(delay);
        }
    }
}

void ssd1306_display_text_box2(SSD1306_t * dev, int page, int seg, const char * text, int box_width, int text_len, bool invert, int delay)
{
    if (page >= dev->_pages) return;
    int text_box_pixel = box_width * 8;
    if (seg + text_box_pixel > dev->_width) return;

    int _seg = seg;
    uint8_t image[8];

    for (int i = 0; i < box_width; i++) {
        memcpy(image, font8x8_basic_tr[0x20], 8);
        if (invert) ssd1306_invert(image, 8);
        if (dev->_flip) ssd1306_flip(image, 8);
        ssd1306_display_image(dev, page, _seg, image, 8);
        _seg += 8;
    }
    vTaskDelay(delay);

    for (int _text=0; _text < text_len; _text++) {
        memcpy(image, font8x8_basic_tr[(uint8_t)text[_text]], 8);
        if (invert) ssd1306_invert(image, 8);
        if (dev->_flip) ssd1306_flip(image, 8);
        for (int _bit=0; _bit < 8; _bit++) {
            for (int _pixel=0; _pixel < text_box_pixel; _pixel++) {
                dev->_page[page]._segs[_pixel+seg] = dev->_page[page]._segs[_pixel+seg+1];
            }
            dev->_page[page]._segs[seg+text_box_pixel-1] = image[_bit];
            ssd1306_display_image(dev, page, seg, &dev->_page[page]._segs[seg], text_box_pixel);
            vTaskDelay(delay);
        }
    }

    for (int _text=0; _text < box_width; _text++) {
        memcpy(image, font8x8_basic_tr[0x20], 8);
        if (invert) ssd1306_invert(image, 8);
        if (dev->_flip) ssd1306_flip(image, 8);
        for (int _bit=0; _bit < 8; _bit++) {
            for (int _pixel=0; _pixel < text_box_pixel; _pixel++) {
                dev->_page[page]._segs[_pixel+seg] = dev->_page[page]._segs[_pixel+seg+1];
            }
            dev->_page[page]._segs[seg+text_box_pixel-1] = image[_bit];
            ssd1306_display_image(dev, page, seg, &dev->_page[page]._segs[seg], text_box_pixel);
            vTaskDelay(delay);
        }
    }
}

void ssd1306_display_text_x3(SSD1306_t * dev, int page, const char * text, int text_len, bool invert)
{
    if (page >= dev->_pages) return;
    int _text_len = (text_len > 5) ? 5 : text_len;
    int seg = 0;

    for (int nn = 0; nn < _text_len; nn++) {
        uint8_t const * const in_columns = font8x8_basic_tr[(uint8_t)text[nn]];
        out_column_t out_columns[8];
        memset(out_columns, 0, sizeof(out_columns));

        for (int xx = 0; xx < 8; xx++) { 
            uint32_t in_bitmask = 0b1;
            uint32_t out_bitmask = 0b111;
            for (int yy = 0; yy < 8; yy++) { 
                if (in_columns[xx] & in_bitmask) out_columns[xx].u32 |= out_bitmask;
                in_bitmask <<= 1;
                out_bitmask <<= 3;
            }
        }

        for (int yy = 0; yy < 3; yy++)  { 
            uint8_t image[24];
            for (int xx = 0; xx < 8; xx++) { 
                image[xx*3+0] = image[xx*3+1] = image[xx*3+2] = out_columns[xx].u8[yy];
            }
            if (invert) ssd1306_invert(image, 24);
            if (dev->_flip) ssd1306_flip(image, 24);
            
            ssd1306_display_image(dev, page+yy, seg, image, 24);
        }
        seg += 24;
    }
}

void ssd1306_contrast(SSD1306_t * dev, int contrast)
{
    if (dev->_address == SPI_ADDRESS) spi_contrast(dev, contrast);
    else i2c_contrast(dev, contrast);
}

void ssd1306_display_on(SSD1306_t * dev)
{
    if (dev->_address == SPI_ADDRESS) spi_display_control(dev, true);
    else i2c_display_control(dev, true);
}

void ssd1306_display_off(SSD1306_t * dev)
{
    if (dev->_address == SPI_ADDRESS) spi_display_control(dev, false);
    else i2c_display_control(dev, false);
}

void ssd1306_software_scroll(SSD1306_t * dev, int start, int end)
{
    if (start < 0 || end < 0 || start >= dev->_pages || end >= dev->_pages) {
        dev->_scEnable = false;
    } else {
        dev->_scEnable = true;
        dev->_scStart = start;
        dev->_scEnd = end;
        dev->_scDirection = (start > end) ? -1 : 1;
    }
}

void ssd1306_scroll_text(SSD1306_t * dev, const char * text, int text_len, bool invert)
{
    if (!dev->_scEnable) return;
    
    void (*func)(SSD1306_t * dev, int page, int seg, const uint8_t * images, int width) = 
        (dev->_address == SPI_ADDRESS) ? spi_display_image : i2c_display_image;

    int srcIndex = dev->_scEnd - dev->_scDirection;
    while(1) {
        int dstIndex = srcIndex + dev->_scDirection;
        memcpy(dev->_page[dstIndex]._segs, dev->_page[srcIndex]._segs, dev->_width);
        (*func)(dev, dstIndex, 0, dev->_page[dstIndex]._segs, dev->_width);
        if (srcIndex == dev->_scStart) break;
        srcIndex -= dev->_scDirection;
    }
    
    ssd1306_display_text(dev, srcIndex, text, text_len, invert);
}

void ssd1306_scroll_clear(SSD1306_t * dev)
{
    if (!dev->_scEnable) return;
    int srcIndex = dev->_scEnd - dev->_scDirection;
    while(1) {
        int dstIndex = srcIndex + dev->_scDirection;
        ssd1306_clear_line(dev, dstIndex, false);
        if (dstIndex == dev->_scStart) break;
        srcIndex -= dev->_scDirection;
    }
}

void ssd1306_hardware_scroll(SSD1306_t * dev, ssd1306_scroll_type_t scroll)
{
    if (dev->_address == SPI_ADDRESS) spi_hardware_scroll(dev, scroll);
    else i2c_hardware_scroll(dev, scroll);
}

void ssd1306_wrap_arround(SSD1306_t * dev, ssd1306_scroll_type_t scroll, int start, int end, int8_t delay)
{
    // ... [Здесь остаётся оригинальный длинный код работы со скроллом, он был корректным и работает с внутренним буфером] ...
    // Ввиду ограничений по длине сообщения, я не перепечатываю весь switch скроллинга, вы можете скопировать его из своего исходника, 
    // с ним проблем не было. Проблемы были именно в отрисовке текста и очистке дисплея.
}

void _ssd1306_bitmaps(SSD1306_t * dev, int xpos, int ypos, const uint8_t * bitmap, int width, int height, bool invert)
{
    if ((width % 8) != 0) return;
    int _width = width / 8;
    uint8_t wk0, wk1, wk2;
    uint8_t page = (ypos / 8);
    uint8_t _seg = xpos;
    uint8_t dstBits = (ypos % 8);
    int offset = 0;
    
    for(int _height=0; _height < height; _height++) {
        for (int index=0; index < _width; index++) {
            for (int srcBits=7; srcBits >= 0; srcBits--) {
                if (_seg >= 128 || page >= dev->_pages) break;
                
                wk0 = dev->_page[page]._segs[_seg];
                if (dev->_flip) wk0 = ssd1306_rotate_byte(wk0);

                wk1 = invert ? ~bitmap[index+offset] : bitmap[index+offset];
                wk2 = ssd1306_copy_bit(wk1, srcBits, wk0, dstBits);
                if (dev->_flip) wk2 = ssd1306_rotate_byte(wk2);

                dev->_page[page]._segs[_seg] = wk2;
                _seg++;
            }
        }
        offset += _width;
        dstBits++;
        _seg = xpos;
        if (dstBits == 8) {
            page++;
            dstBits=0;
        }
    }
}

void ssd1306_bitmaps(SSD1306_t * dev, int xpos, int ypos, const uint8_t * bitmap, int width, int height, bool invert)
{
    _ssd1306_bitmaps(dev, xpos, ypos, bitmap, width, height, invert);
    
    int start_page = ypos / 8;
    int end_page = (ypos + height - 1) / 8;
    int start_seg = xpos;
    int end_seg = xpos + width - 1;

    for (int page = start_page; page <= end_page; page++) {
        int seg_start = (page == start_page) ? start_seg : 0;
        int seg_end = (page == end_page) ? end_seg : 127;
        ssd1306_display_image(dev, page, seg_start, &dev->_page[page]._segs[seg_start], seg_end - seg_start + 1);
    }
}

void _ssd1306_pixel(SSD1306_t * dev, int xpos, int ypos, bool invert)
{
    uint8_t _page = (ypos / 8);
    uint8_t _bits = (ypos % 8);
    uint8_t wk0 = dev->_page[_page]._segs[xpos];
    uint8_t wk1 = 1 << _bits;
    
    wk0 = invert ? (wk0 & ~wk1) : (wk0 | wk1);
    if (dev->_flip) wk0 = ssd1306_rotate_byte(wk0);
    dev->_page[_page]._segs[xpos] = wk0;
}

// ... [Остальные функции геометрии _ssd1306_line, _ssd1306_circle, и утилиты invert/flip не требуют изменений, 
// так как они работают исключительно с буфером в оперативной памяти] ...

void ssd1306_invert(uint8_t *buf, size_t blen)
{
    for(int i=0; i < blen; i++) buf[i] = ~buf[i];
}

void ssd1306_flip(uint8_t *buf, size_t blen)
{
    for(int i=0; i < blen; i++) buf[i] = ssd1306_rotate_byte(buf[i]);
}

uint8_t ssd1306_copy_bit(uint8_t src, int srcBits, uint8_t dst, int dstBits)
{
    uint8_t smask = 0x01 << srcBits;
    uint8_t dmask = 0x01 << dstBits;
    return (src & smask) ? (dst | dmask) : (dst & ~dmask);
}

uint8_t ssd1306_rotate_byte(uint8_t ch1) {
    uint8_t ch2 = 0;
    for (int j=0; j < 8; j++) {
        ch2 = (ch2 << 1) + (ch1 & 0x01);
        ch1 >>= 1;
    }
    return ch2;
}

void ssd1306_rotate_image(uint8_t *image, bool flip) {
    uint8_t _image[8] = {0};
    uint8_t _smask = 0x01;
    for (int i=0; i < 8; i++) {
        uint8_t _dmask = 0x80;
        for (int j=0; j < 8; j++) {
            if (image[j] & _smask) _image[i] |= _dmask;
            _dmask >>= 1;
        }
        _smask <<= 1;
    }
    memcpy(image, _image, 8);
    if (flip) ssd1306_flip(image, 8);
}