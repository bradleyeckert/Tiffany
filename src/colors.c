#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "colors.h"
#include "vm.h"
#include "tiff.h"

// Hilighting colors

int theme (void) {                      // 0 = mono, 1 = color
    return FetchByte(THEME);
}

void ColorHilight(void){                // set color to hilighted
  if (theme()) {
	printf("\033[1;35m");				// magenta
  }
}

void ColorNormal(void){                	// set color to normal
  if (theme()) {
	printf("\033[1;37;40m");		    // bright, white with black background
  }
}

void ColorError(void){                	// set color to error
  if (theme()) {
	printf("\033[1;31m");				// red
  }
}

void ColorDef(void){                	// set color to defining/defined
  if (theme()) {
	printf("\033[1;31m");				// red
  }
}

void ColorCompiled(void){               // set color to compiled
  if (theme()) {
	printf("\033[1;32m");				// green
  }
}

void ColorImmediate(void){              // set color to immediate data
  if (theme()) {
	printf("\033[1;33m");				// yellow
  }
}

void ColorImmAddress(void){             // set color to immediate address
  if (theme()) {
	printf("\033[0;33m");				// yellow dim
  }
}

void ColorOpcode(void){                 // set color to opcode
  if (theme()) {
	printf("\033[0m");					// plain
  }
}

void ColorFilePath(void){               // set color to file path
  if (theme()) {
	printf("\033[0;35m");				// cyan
  }
}

void ColorFileLine(void){               // set color to file path
  if (theme()) {
	printf("\033[0;32m");				// green
  }
}

void ColorNone(void){                	// set color to ACCEPT mode
  if (theme()) {
	printf("\033[0m");          		// reset colors
  }
}

static char WordColors[] = {
    36, 45, // call-only + anonymous
    36, 40, // call-only
    32, 45, // anonymous
    32, 40, // typical - green
    31, 40  // smudged
};

void WordColor(int color) {             // set color based on word type
    if (theme()) {
        if (color & 1) {
            color = 8;
        } else {
            color *= 2;
        }
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
