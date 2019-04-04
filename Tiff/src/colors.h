//===============================================================================
// colors.h
//===============================================================================
#ifndef __COLORS_H__
#define __COLORS_H__

// Hilighting colors

extern int ColorTheme;

void ColorHilight(void);                // set color to hilighted
void ColorNormal(void);                	// set color to normal
void ColorError(void);                	// set color to error
void ColorDef(void);                	// set color to defining/defined
void ColorCompiled(void);               // set color to compiled
void ColorImmediate(void);              // set color to immediate data
void ColorImmAddress(void);             // set color to immediate address
void ColorOpcode(void);                 // set color to opcode
void ColorFilePath(void);               // set color to file path
void ColorFileLine(void);               // set color to file path
void ColorNone(void);                	// set color to ACCEPT mode
void WordColor(int color);              // set color based on word type

#endif
