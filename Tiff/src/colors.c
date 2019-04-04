#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "colors.h"

// Hilighting colors

int ColorTheme = StartupTheme;

void ColorHilight(void){                // set color to hilighted
  if (ColorTheme) {
	printf("\033[1;35m");				// magenta
  }
}

void ColorNormal(void){                	// set color to normal
  if (ColorTheme) {
	printf("\033[1;37;40m");		    // bright, white with black background
  }
}

void ColorError(void){                	// set color to error
  if (ColorTheme) {
	printf("\033[1;31m");				// red
  }
}

void ColorDef(void){                	// set color to defining/defined
  if (ColorTheme) {
	printf("\033[1;31m");				// red
  }
}

void ColorCompiled(void){               // set color to compiled
  if (ColorTheme) {
	printf("\033[1;32m");				// green
  }
}

void ColorImmediate(void){              // set color to immediate data
  if (ColorTheme) {
	printf("\033[1;33m");				// yellow
  }
}

void ColorImmAddress(void){             // set color to immediate address
  if (ColorTheme) {
	printf("\033[0;33m");				// yellow dim
  }
}

void ColorOpcode(void){                 // set color to opcode
  if (ColorTheme) {
	printf("\033[0m");					// plain
  }
}

void ColorFilePath(void){               // set color to file path
  if (ColorTheme) {
	printf("\033[0;35m");				// cyan
  }
}

void ColorFileLine(void){               // set color to file path
  if (ColorTheme) {
	printf("\033[0;32m");				// green
  }
}

void ColorNone(void){                	// set color to ACCEPT mode
  if (ColorTheme) {
	printf("\033[0m");          		// reset colors
  }
}

static char WordColors[16] = {          // FG  BG
30, 44,                                 // 30, 40 = Black
31, 44, // bit 0 = smudged              // 31, 41 = Red
30, 40,                                 // 32, 42 = Green
31, 40,                                 // 33, 43 = Yellow
33, 44, // bit 2 = call-only (when '0') // 34, 44 = Blue
31, 44,                                 // 35, 45 = Magenta
33, 40, // bit 1 = public               // 36, 46 = Cyan
31, 40                                  // 37, 47 = White
};

void WordColor(int color) {             // set color based on word type
  if (ColorTheme) {
    printf("\033[1;%d;%dm", WordColors[color], WordColors[color+1]);
  }
}
/* colorForth colors, for reference:
_Color_			_Time_		_Purpose_
White			Ignored		Comment (ignored by assembler)
Gray			Compile		Literal Instruction (packed by assembler)
Red				Compile		Add name/address pair to dictionary.
Yellow number	Compile		Immediately push number (assembly-time)
Yellow word		Compile		Immediately call (i.e. assembly-time macro)
Green number	Execute		Compile literal (pack @p and value)
Green word		Execute		Compile call/jump (to defined red word)
Blue			Display		Format word (editor display-time)
*/
