#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "colors.h"

// Hilighting colors

void ColorHilight(void){                // set color to hilighted
#ifndef MONOCHROME
	printf("\033[1;35m");				// magenta
#endif
}

void ColorNormal(void){                	// set color to normal
#ifndef MONOCHROME
	printf("\033[1;37;40m");		    // bright, white with black background
#endif
}

void ColorError(void){                	// set color to error
#ifndef MONOCHROME
	printf("\033[1;31m");				// red
#endif
}

void ColorDef(void){                	// set color to defining/defined
#ifndef MONOCHROME
	printf("\033[1;31m");				// red
#endif
}

void ColorCompiled(void){               // set color to compiled
#ifndef MONOCHROME
	printf("\033[1;32m");				// green
#endif
}

void ColorImmediate(void){              // set color to immediate data
#ifndef MONOCHROME
	printf("\033[1;33m");				// yellow
#endif
}

void ColorImmAddress(void){             // set color to immediate address
#ifndef MONOCHROME
	printf("\033[0;33m");				// yellow dim
#endif
}

void ColorOpcode(void){                 // set color to opcode
#ifndef MONOCHROME
	printf("\033[0m");					// plain
#endif
}

void ColorFilePath(void){               // set color to file path
#ifndef MONOCHROME
	printf("\033[0;35m");				// cyan
#endif
}

void ColorFileLine(void){               // set color to file path
#ifndef MONOCHROME
	printf("\033[0;32m");				// green
#endif
}

void ColorNone(void){                	// set color to ACCEPT mode
#ifndef MONOCHROME
	printf("\033[0m");          		// reset colors
#endif
}

#ifndef MONOCHROME
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
#endif

void WordColor(int color) {             // set color based on word type
#ifndef MONOCHROME
    printf("\033[1;%d;%dm", WordColors[color], WordColors[color+1]);
#endif
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
