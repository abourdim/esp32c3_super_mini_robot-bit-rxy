#ifndef __DEF_INCLUDE_OLED_H__
#define __DEF_INCLUDE_OLED_H__

extern void oled_init(void);

// Text pushed from the app's OLED field, drawn as the screen's top line.
// Empty means "no override" and the usual banner is shown instead.
extern bool        oled_text_active(void);
extern const char* oled_text_get(void);
extern void        oled_text_set(const char* s);
extern void oled_update(void);

#endif // __DEF_INCLUDE_OLED_H__