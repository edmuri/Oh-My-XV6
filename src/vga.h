/* switch to vga Mode 13 */
void vgaMode13(void);

/* set graphics mode palette value */
void vgaSetPalette(int index, int r, int g, int b);

/* switch to vga Mode 3 (text) */
void vgaMode3(void);

/* load 256 raw 8x16 glyphs into VGA text-mode font memory */
void vgaLoadFont(char*);

/* restore the built-in 8x16 font */
void vgaLoadDefaultFont(void);
