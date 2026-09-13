#include <stddef.h>
#include <stdint.h>

#include "hexa_oled.h"
#include "hexa_types.h"
#include "hexa_board.h"


/* Буфер кадра: нулевой байт — control byte SSD1306 (0x40, "дальше идут
 * данные"), дальше сама видеопамять. Размер выведен из hexa_board.h, а не
 * записан числом: поменяешь там разрешение — пересчитается здесь.
 *
 * Control byte выставлен прямо в инициализаторе, а не в hexa_oled_init():
 * иначе clear()+flush() до init() отправили бы килобайт в командный поток
 * дисплея. */
#define FB_DATA_SIZE    (HEXA_OLED_WIDTH * HEXA_OLED_HEIGHT / 8)
#define FB_SIZE         (FB_OFFSET + FB_DATA_SIZE)

_Static_assert(HEXA_OLED_HEIGHT % 8 == 0,
        "SSD1306 адресует видеопамять страницами по 8 строк");

static uint8_t fb[FB_SIZE] = { 0x40 };


/* Применить маску к одному байту видеопамяти. Здесь и только здесь
 * толкуется color, поэтому смысл "погасить / зажечь / инвертировать" один
 * на всё, что рисует драйвер. */
static void fb_apply(uint16_t idx, uint8_t mask, uint8_t color)
{
	switch (color) {
		case 0: fb[idx] &= (uint8_t)~mask; break;
		case 1: fb[idx] |=  mask;          break;
		default: fb[idx] ^= mask;          break;
	}
}

/* Смещение байта, в котором лежит пиксель (x,y). Видеопамять разбита на
 * страницы по 8 строк, поэтому строку задаёт y/8, а не y. Координаты
 * обязаны быть уже проверены. */
static uint16_t fb_index(int16_t x, int16_t y)
{
	return (uint16_t)(FB_OFFSET + x + (y / 8) * HEXA_OLED_WIDTH);
}

/* Пиксель со знаковыми координатами: всё, что за экраном, молча
 * отбрасывается. Вся геометрия ниже отсекает фигуры именно так — поэтому
 * фигура, наполовину уехавшая за край, рисуется наполовину, а не
 * заворачивается на другую сторону.
 *
 * Без знаковых координат этого не выразить: "левее нуля" в uint8_t
 * неотличимо от "у правого края". */
static void fb_pixel(int16_t x, int16_t y, uint8_t color)
{
	if (x < 0 || y < 0 || x >= HEXA_OLED_WIDTH || y >= HEXA_OLED_HEIGHT) {
		return;
	}

	fb_apply(fb_index(x, y), (uint8_t)(1u << (y & 7)), color);
}

static int16_t iabs(int16_t v) { return v < 0 ? (int16_t)-v : v; }

/* Целочисленный корень перебором. Считается один раз на строку заливки
 * круга, радиусы тут в пределах полуэкрана — цикл короче, чем стоил бы
 * вызов sqrt() из libm, которой в сборке всё равно нет. */
static int16_t isqrt32(int32_t v)
{
	int16_t r = 0;
	while ((int32_t)(r + 1) * (r + 1) <= v) {
		r++;
	}
	return r;
}

/* 5x7 font, ASCII 0x20-0x7E, one byte per column, LSB = top row */
static const uint8_t font5x7[][5] = {
	{ 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 0x20 space */
	{ 0x00, 0x00, 0x5F, 0x00, 0x00 }, /* ! */
	{ 0x00, 0x07, 0x00, 0x07, 0x00 }, /* " */
	{ 0x14, 0x7F, 0x14, 0x7F, 0x14 }, /* # */
	{ 0x24, 0x2A, 0x7F, 0x2A, 0x12 }, /* $ */
	{ 0x23, 0x13, 0x08, 0x64, 0x62 }, /* % */
	{ 0x36, 0x49, 0x55, 0x22, 0x50 }, /* & */
	{ 0x00, 0x05, 0x03, 0x00, 0x00 }, /* ' */
	{ 0x00, 0x1C, 0x22, 0x41, 0x00 }, /* ( */
	{ 0x00, 0x41, 0x22, 0x1C, 0x00 }, /* ) */
	{ 0x14, 0x08, 0x3E, 0x08, 0x14 }, /* * */
	{ 0x08, 0x08, 0x3E, 0x08, 0x08 }, /* + */
	{ 0x00, 0x50, 0x30, 0x00, 0x00 }, /* , */
	{ 0x08, 0x08, 0x08, 0x08, 0x08 }, /* - */
	{ 0x00, 0x60, 0x60, 0x00, 0x00 }, /* . */
	{ 0x20, 0x10, 0x08, 0x04, 0x02 }, /* / */
	{ 0x3E, 0x51, 0x49, 0x45, 0x3E }, /* 0 */
	{ 0x00, 0x42, 0x7F, 0x40, 0x00 }, /* 1 */
	{ 0x42, 0x61, 0x51, 0x49, 0x46 }, /* 2 */
	{ 0x21, 0x41, 0x45, 0x4B, 0x31 }, /* 3 */
	{ 0x18, 0x14, 0x12, 0x7F, 0x10 }, /* 4 */
	{ 0x27, 0x45, 0x45, 0x45, 0x39 }, /* 5 */
	{ 0x3C, 0x4A, 0x49, 0x49, 0x30 }, /* 6 */
	{ 0x01, 0x71, 0x09, 0x05, 0x03 }, /* 7 */
	{ 0x36, 0x49, 0x49, 0x49, 0x36 }, /* 8 */
	{ 0x06, 0x49, 0x49, 0x29, 0x1E }, /* 9 */
	{ 0x00, 0x36, 0x36, 0x00, 0x00 }, /* : */
	{ 0x00, 0x56, 0x36, 0x00, 0x00 }, /* ; */
	{ 0x00, 0x08, 0x14, 0x22, 0x41 }, /* < */
	{ 0x14, 0x14, 0x14, 0x14, 0x14 }, /* = */
	{ 0x41, 0x22, 0x14, 0x08, 0x00 }, /* > */
	{ 0x02, 0x01, 0x51, 0x09, 0x06 }, /* ? */
	{ 0x32, 0x49, 0x79, 0x41, 0x3E }, /* @ */
	{ 0x7E, 0x11, 0x11, 0x11, 0x7E }, /* A */
	{ 0x7F, 0x49, 0x49, 0x49, 0x36 }, /* B */
	{ 0x3E, 0x41, 0x41, 0x41, 0x22 }, /* C */
	{ 0x7F, 0x41, 0x41, 0x22, 0x1C }, /* D */
	{ 0x7F, 0x49, 0x49, 0x49, 0x41 }, /* E */
	{ 0x7F, 0x09, 0x09, 0x01, 0x01 }, /* F */
	{ 0x3E, 0x41, 0x49, 0x49, 0x7A }, /* G */
	{ 0x7F, 0x08, 0x08, 0x08, 0x7F }, /* H */
	{ 0x00, 0x41, 0x7F, 0x41, 0x00 }, /* I */
	{ 0x20, 0x40, 0x41, 0x3F, 0x01 }, /* J */
	{ 0x7F, 0x08, 0x14, 0x22, 0x41 }, /* K */
	{ 0x7F, 0x40, 0x40, 0x40, 0x40 }, /* L */
	{ 0x7F, 0x02, 0x0C, 0x02, 0x7F }, /* M */
	{ 0x7F, 0x04, 0x08, 0x10, 0x7F }, /* N */
	{ 0x3E, 0x41, 0x41, 0x41, 0x3E }, /* O */
	{ 0x7F, 0x09, 0x09, 0x09, 0x06 }, /* P */
	{ 0x3E, 0x41, 0x51, 0x21, 0x5E }, /* Q */
	{ 0x7F, 0x09, 0x19, 0x29, 0x46 }, /* R */
	{ 0x46, 0x49, 0x49, 0x49, 0x31 }, /* S */
	{ 0x01, 0x01, 0x7F, 0x01, 0x01 }, /* T */
	{ 0x3F, 0x40, 0x40, 0x40, 0x3F }, /* U */
	{ 0x1F, 0x20, 0x40, 0x20, 0x1F }, /* V */
	{ 0x3F, 0x40, 0x38, 0x40, 0x3F }, /* W */
	{ 0x63, 0x14, 0x08, 0x14, 0x63 }, /* X */
	{ 0x07, 0x08, 0x70, 0x08, 0x07 }, /* Y */
	{ 0x61, 0x51, 0x49, 0x45, 0x43 }, /* Z */
	{ 0x00, 0x7F, 0x41, 0x41, 0x00 }, /* [ */
	{ 0x02, 0x04, 0x08, 0x10, 0x20 }, /* \ */
	{ 0x00, 0x41, 0x41, 0x7F, 0x00 }, /* ] */
	{ 0x04, 0x02, 0x01, 0x02, 0x04 }, /* ^ */
	{ 0x40, 0x40, 0x40, 0x40, 0x40 }, /* _ */
	{ 0x00, 0x01, 0x02, 0x04, 0x00 }, /* ` */
	{ 0x20, 0x54, 0x54, 0x54, 0x78 }, /* a */
	{ 0x7F, 0x48, 0x44, 0x44, 0x38 }, /* b */
	{ 0x38, 0x44, 0x44, 0x44, 0x20 }, /* c */
	{ 0x38, 0x44, 0x44, 0x48, 0x7F }, /* d */
	{ 0x38, 0x54, 0x54, 0x54, 0x18 }, /* e */
	{ 0x08, 0x7E, 0x09, 0x01, 0x02 }, /* f */
	{ 0x0C, 0x52, 0x52, 0x52, 0x3E }, /* g */
	{ 0x7F, 0x08, 0x04, 0x04, 0x78 }, /* h */
	{ 0x00, 0x44, 0x7D, 0x40, 0x00 }, /* i */
	{ 0x20, 0x40, 0x44, 0x3D, 0x00 }, /* j */
	{ 0x7F, 0x10, 0x28, 0x44, 0x00 }, /* k */
	{ 0x00, 0x41, 0x7F, 0x40, 0x00 }, /* l */
	{ 0x7C, 0x04, 0x18, 0x04, 0x78 }, /* m */
	{ 0x7C, 0x08, 0x04, 0x04, 0x78 }, /* n */
	{ 0x38, 0x44, 0x44, 0x44, 0x38 }, /* o */
	{ 0x7C, 0x14, 0x14, 0x14, 0x08 }, /* p */
	{ 0x08, 0x14, 0x14, 0x18, 0x7C }, /* q */
	{ 0x7C, 0x08, 0x04, 0x04, 0x08 }, /* r */
	{ 0x48, 0x54, 0x54, 0x54, 0x20 }, /* s */
	{ 0x04, 0x3F, 0x44, 0x40, 0x20 }, /* t */
	{ 0x3C, 0x40, 0x40, 0x20, 0x7C }, /* u */
	{ 0x1C, 0x20, 0x40, 0x20, 0x1C }, /* v */
	{ 0x3C, 0x40, 0x30, 0x40, 0x3C }, /* w */
	{ 0x44, 0x28, 0x10, 0x28, 0x44 }, /* x */
	{ 0x0C, 0x50, 0x50, 0x50, 0x3C }, /* y */
	{ 0x44, 0x64, 0x54, 0x4C, 0x44 }, /* z */
	{ 0x00, 0x08, 0x36, 0x41, 0x00 }, /* { */
	{ 0x00, 0x00, 0x7F, 0x00, 0x00 }, /* | */
	{ 0x00, 0x41, 0x36, 0x08, 0x00 }, /* } */
	{ 0x08, 0x04, 0x08, 0x10, 0x08 }, /* ~ 0x7E */
};


hexa_status_t hexa_oled_init(void)
{
	static const uint8_t init_cmds[] = {
		0xAE,
		0xD5, 0x80,
		0xA8, 0x3F,
		0xD3, 0x00,
		0x40,
		0x8D, 0x14,
		0x20, 0x00,
		0xA1,
		0xC8,
		0xDA, 0x12,
		0x81, 0xCF,
		0xD9, 0xF1,
		0xDB, 0x40,
		0xA4,
		0xA6,
		0xAF,
	};

	if (hexa_i2c_write(HEXA_OLED_ADDR, 0x00, init_cmds, sizeof(init_cmds)) != HEXA_OK) {
		return HEXA_ERR;
	}

	return HEXA_OK;
}

hexa_status_t hexa_oled_print(const char* str, uint8_t x, uint8_t y, uint8_t color)
{
	/* Глиф целиком вне экрана по вертикали — 35 отброшенных попыток
	 * на символ. Дешевле отказаться сразу. */
	if (y >= HEXA_OLED_HEIGHT) {
		return HEXA_OK;
	}

	for (const char* p = str; *p != '\0'; p++) {
		if (x >= HEXA_OLED_WIDTH) {
			break;
		}

		unsigned char c = (unsigned char)*p;
		if (c < 0x20 || c > 0x7E) {
			c = 0x20;
		}

		const uint8_t* glyph = font5x7[c - 0x20];
		for (uint8_t col = 0; col < 5; col++) {
			uint8_t bits = glyph[col];
			for (uint8_t row = 0; row < 7; row++) {
				if (bits & (1u << row)) {
					fb_pixel(x + col, y + row, color);
				}
			}
		}

		x += 6;
	}

	return HEXA_OK;
}

/* Публичный пиксель остаётся на uint8_t: так он описан в доке и так его
 * зовут примеры. Геометрия ниже работает знаковыми координатами — ей
 * нужно отличать "левее нуля" от "правее 255". */
hexa_status_t hexa_oled_pixel(uint8_t x, uint8_t y, uint8_t color)
{
	if (x >= HEXA_OLED_WIDTH || y >= HEXA_OLED_HEIGHT) {
		return HEXA_ERR;
	}

	fb_pixel((int16_t)x, (int16_t)y, color);
	return HEXA_OK;
}

hexa_status_t hexa_oled_clear(void)
{
	for (uint16_t i = FB_OFFSET; i < FB_SIZE; i++) {
		fb[i] = 0x00;
	}
	return HEXA_OK;
}

hexa_status_t hexa_oled_flush(void)
{
        return hexa_i2c_write_raw(HEXA_OLED_ADDR, fb, sizeof(fb));
}


/* ---- Геометрия ------------------------------------------------------- */

hexa_status_t hexa_oled_hline(int16_t x, int16_t y, int16_t w, uint8_t color)
{
	if (w <= 0) {
		return HEXA_ERR_PARAM;
	}
	if (y < 0 || y >= HEXA_OLED_HEIGHT) {
		return HEXA_OK;
	}

	/* Отсечение по краям: сдвигаем начало и укорачиваем длину. */
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (x + w > HEXA_OLED_WIDTH) {
		w = HEXA_OLED_WIDTH - x;
	}
	if (w <= 0) {
		return HEXA_OK;
	}

	/* Строка лежит в одной странице видеопамяти, значит и маска на все
	 * столбцы одна — считаем её один раз и идём по буферу подряд. */
	uint16_t idx  = fb_index(x, y);
	uint8_t  mask = (uint8_t)(1u << (y & 7));

	for (int16_t i = 0; i < w; i++) {
		fb_apply(idx + (uint16_t)i, mask, color);
	}

	return HEXA_OK;
}

hexa_status_t hexa_oled_vline(int16_t x, int16_t y, int16_t h, uint8_t color)
{
	if (h <= 0) {
		return HEXA_ERR_PARAM;
	}
	if (x < 0 || x >= HEXA_OLED_WIDTH) {
		return HEXA_OK;
	}

	if (y < 0) {
		h += y;
		y = 0;
	}
	if (y + h > HEXA_OLED_HEIGHT) {
		h = HEXA_OLED_HEIGHT - y;
	}
	if (h <= 0) {
		return HEXA_OK;
	}

	/* Вертикальный отрезок — тот случай, ради которого видеопамять и
	 * устроена столбиками: на каждую страницу приходится ровно один байт.
	 * Собираем маску из попавших в отрезок битов и пишем байт целиком,
	 * вместо восьми проходов через fb_pixel(). */
	const int16_t y_end = y + h;    /* не включая */

	while (y < y_end) {
		int16_t page_end = (y | 7) + 1;         /* первая строка след. страницы */
		uint8_t bit_lo   = y & 7;
		uint8_t bit_hi   = (page_end > y_end) ? (uint8_t)((y_end - 1) & 7) : 7;

		uint8_t mask = (uint8_t)((0xFFu << bit_lo) & (0xFFu >> (7 - bit_hi)));
		fb_apply(fb_index(x, y), mask, color);

		y = page_end;
	}

	return HEXA_OK;
}

hexa_status_t hexa_oled_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
	/* Вырожденные случаи отдаём байтовым версиям: то же самое, но без
	 * попиксельного цикла. */
	if (y0 == y1) {
		return hexa_oled_hline(x0 < x1 ? x0 : x1, y0, (int16_t)(iabs(x1 - x0) + 1), color);
	}
	if (x0 == x1) {
		return hexa_oled_vline(x0, y0 < y1 ? y0 : y1, (int16_t)(iabs(y1 - y0) + 1), color);
	}

	/* Брезенхэм в форме с одной ошибкой на оба направления: dy взят со
	 * знаком минус, тогда шаг по обеим осям проверяется симметрично и
	 * ветвления на октанты не нужны.
	 *
	 * Отсечение тут попиксельное, внутри fb_pixel(). Для отрезка, почти
	 * целиком ушедшего за экран, это трата тактов на заведомо
	 * отброшенные точки — но дёшево и без отдельного алгоритма
	 * отсечения, которому здесь взяться неоткуда. */
	int16_t dx = iabs(x1 - x0),  sx = (x0 < x1) ? 1 : -1;
	int16_t dy = (int16_t)-iabs(y1 - y0), sy = (y0 < y1) ? 1 : -1;
	int32_t err = dx + dy;

	for (;;) {
		fb_pixel(x0, y0, color);

		if (x0 == x1 && y0 == y1) {
			break;
		}

		int32_t e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}

	return HEXA_OK;
}

hexa_status_t hexa_oled_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
	if (w <= 0 || h <= 0) {
		return HEXA_ERR_PARAM;
	}

	hexa_oled_hline(x, y, w, color);
	if (h == 1) {
		return HEXA_OK;
	}
	hexa_oled_hline(x, (int16_t)(y + h - 1), w, color);

	/* Боковины укорочены на пиксель сверху и снизу: углы уже нарисованы
	 * горизонталями. Пройтись по ним дважды означало бы, что рамка в
	 * режиме инверсии потеряет углы. */
	if (h > 2) {
		hexa_oled_vline(x, (int16_t)(y + 1), (int16_t)(h - 2), color);
		if (w > 1) {
			hexa_oled_vline((int16_t)(x + w - 1), (int16_t)(y + 1),
			                (int16_t)(h - 2), color);
		}
	}

	return HEXA_OK;
}

hexa_status_t hexa_oled_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
	if (w <= 0 || h <= 0) {
		return HEXA_ERR_PARAM;
	}

	for (int16_t i = 0; i < h; i++) {
		hexa_oled_hline(x, (int16_t)(y + i), w, color);
	}

	return HEXA_OK;
}

/* Восемь симметричных точек окружности за один шаг алгоритма.
 * Проверки не косметические: при x == 0 и при x == y часть точек
 * совпадает, и без них инверсия (color > 1) гасила бы сама себя. */
static void circle_points(int16_t cx, int16_t cy, int16_t x, int16_t y, uint8_t color)
{
	fb_pixel((int16_t)(cx + x), (int16_t)(cy + y), color);
	fb_pixel((int16_t)(cx + x), (int16_t)(cy - y), color);

	if (x != 0) {
		fb_pixel((int16_t)(cx - x), (int16_t)(cy + y), color);
		fb_pixel((int16_t)(cx - x), (int16_t)(cy - y), color);
	}

	if (x != y) {
		fb_pixel((int16_t)(cx + y), (int16_t)(cy + x), color);
		fb_pixel((int16_t)(cx - y), (int16_t)(cy + x), color);

		if (x != 0) {
			fb_pixel((int16_t)(cx + y), (int16_t)(cy - x), color);
			fb_pixel((int16_t)(cx - y), (int16_t)(cy - x), color);
		}
	}
}

hexa_status_t hexa_oled_circle(int16_t cx, int16_t cy, int16_t r, uint8_t color)
{
	if (r < 0) {
		return HEXA_ERR_PARAM;
	}

	/* Алгоритм средней точки: считаем только верхний октант, остальное
	 * достраивается симметрией. Ошибка d — знак выражения окружности в
	 * точке между двумя кандидатами на следующий шаг. */
	int16_t x = 0;
	int16_t y = r;
	int16_t d = (int16_t)(1 - r);

	while (x <= y) {
		circle_points(cx, cy, x, y, color);

		if (d < 0) {
			d += (int16_t)(2 * x + 3);
		} else {
			d += (int16_t)(2 * (x - y) + 5);
			y--;
		}
		x++;
	}

	return HEXA_OK;
}

hexa_status_t hexa_oled_fill_circle(int16_t cx, int16_t cy, int16_t r, uint8_t color)
{
	if (r < 0) {
		return HEXA_ERR_PARAM;
	}

	/* Заливка строками, а не симметрией средней точки: у симметрии
	 * горизонтали перекрываются, и в режиме инверсии круг пошёл бы
	 * пятнами. Здесь каждая строка рисуется ровно один раз. */
	const int32_t rr = (int32_t)r * r;

	for (int16_t dy = (int16_t)-r; dy <= r; dy++) {
		int16_t dx = isqrt32(rr - (int32_t)dy * dy);
		hexa_oled_hline((int16_t)(cx - dx), (int16_t)(cy + dy),
		                (int16_t)(2 * dx + 1), color);
	}

	return HEXA_OK;
}

hexa_status_t hexa_oled_bitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                               const uint8_t *bits, uint8_t color)
{
	if (bits == NULL || w <= 0 || h <= 0) {
		return HEXA_ERR_PARAM;
	}

	/* Строка картинки выровнена на байт, хвостовые биты последнего байта
	 * не значат ничего и пропускаются сами: цикл идёт до w, а не до
	 * конца байта. */
	const uint16_t stride = (uint16_t)((w + 7) / 8);

	for (int16_t row = 0; row < h; row++) {
		int16_t py = (int16_t)(y + row);
		if (py < 0 || py >= HEXA_OLED_HEIGHT) {
			continue;       /* строка за экраном — байты даже не читаем */
		}

		const uint8_t *line = bits + (uint16_t)row * stride;

		for (int16_t col = 0; col < w; col++) {
			if (line[col >> 3] & (0x80u >> (col & 7))) {
				fb_pixel((int16_t)(x + col), py, color);
			}
		}
	}

	return HEXA_OK;
}
