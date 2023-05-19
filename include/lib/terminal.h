#ifndef _TERMINAL_H_
#define _TERMINAL_H_

#define ESC "\x1b"
#define CSI "\x1b["

/**
 * @brief ANSI 选择图形再现（Select Graphic Rendition）
 */
#define SGR(config) CSI config "m"

#define SGR_RESET SGR("0")
#define SGR_BOLD SGR("1")
#define SGR_FAINT SGR("2")
#define SGR_ITALIC SGR("3")
#define SGR_UNDERLINE SGR("4")
#define SGR_SLOW_BLINK SGR("5")
#define SGR_RAPID_BLINK SGR("6")

#define SGR_BLACK SGR("30")
#define SGR_RED SGR("31")
#define SGR_GREEN SGR("32")
#define SGR_YELLOW SGR("33")
#define SGR_BLUE SGR("34")
#define SGR_MAGENTA SGR("35")
#define SGR_CYAN SGR("36")
#define SGR_WHITE SGR("37")

/**
 * @brief FarmOS Terminal
 */
#define FARM_INFO SGR_BOLD SGR_BLUE
#define FARM_WARN SGR_BOLD SGR_YELLOW
#define FARM_ERROR SGR_BOLD SGR_RED

#endif // _TERMINAL_H_
