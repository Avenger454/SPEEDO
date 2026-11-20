from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parents[1] / 'main/tach_ui/fonts'
FONT = '/usr/share/fonts/truetype/liberation/LiberationSans-BoldItalic.ttf'

def generate(name, size, chars):
    font = ImageFont.truetype(FONT, size)
    ascent, descent = font.getmetrics()
    bitmap, descriptors = [], ['{0}']
    for char in chars:
        left, top, right, bottom = font.getbbox(char, anchor='ls')
        w, h = right-left, bottom-top
        image = Image.new('L', (max(1,w), max(1,h)))
        ImageDraw.Draw(image).text((-left,-top), char, font=font, fill=255, anchor='ls')
        pixels = list(image.getdata()) if w and h else []
        start = len(bitmap)
        for i in range(0,len(pixels),2):
            bitmap.append(((pixels[i]>>4)<<4) | (pixels[i+1]>>4 if i+1<len(pixels) else 0))
        descriptors.append('{.bitmap_index=%d,.adv_w=%d,.box_w=%d,.box_h=%d,.ofs_x=%d,.ofs_y=%d}' %
                           (start, round(font.getlength(char)*16),w,h,left,(0 if name == "speedo_digits_170" else -bottom)))
    result = '#include "lvgl.h"\n'
    result += 'static const uint8_t pixels[]={' + ','.join(map(str,bitmap)) + '};\n'
    result += 'static const lv_font_fmt_txt_glyph_dsc_t glyphs[]={' + ','.join(descriptors) + '};\n'
    result += 'static const lv_font_fmt_txt_cmap_t maps[]={{.range_start=%d,.range_length=%d,.glyph_id_start=1,.type=LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}};\n' % (ord(chars[0]), len(chars))
    result += 'static lv_font_fmt_txt_dsc_t desc={.glyph_bitmap=pixels,.glyph_dsc=glyphs,.cmaps=maps,.cmap_num=1,.bpp=4};\n'
    result += 'const lv_font_t %s={.get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,.get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,.line_height=%d,.base_line=%d,.dsc=&desc};\n' % (name,128 if name == "speedo_digits_170" else ascent+descent,0 if name == "speedo_digits_170" else descent)
    (OUT/(name+'.c')).write_text(result)

generate('speedo_digits_170',170,'0123456789')
generate('speedo_text_28',28,''.join(chr(i) for i in range(32,127)))
