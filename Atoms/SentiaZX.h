#ifndef _SENTIAZX_H
#define _SENTIAZX_H

#include <spectrum.h>
#include <graphics.h>
#include <games.h>
#include <stdio.h>


// Set the Attribute at the row col (in attribute space)
void SetAttrib(uchar row, uchar col, uchar attrib)
{
	uchar* att = 0;
	att = zx_cyx2aaddr(row,col);

	if(att != 0)
	{
		(*att) = attrib;
	}
}

// Set the attributes of a whole area, to the same attribute.
void SetAttribArea(uchar x1, uchar x2, uchar y1, uchar y2, uchar attrib)
{
	uchar x, y;
	//uchar* att = 0;

	for(x = x1 ; x <= x2 ; x++)
	{
		for(y =y1;y <= y2;y++)
		{
			*zx_cyx2aaddr(y,x) = attrib;
		}
	}
}

void DrawString(uchar x, uchar y, char* line, uchar * font)
{
	int i=0;
	
	

	while(line[i] != 0)
	{
		if (line[i] != 32)
		{
			int a = font[0];
			int letter = line[i] - a;
			int toUse = (letter * 10) + 1;

			//printf("%d %d %d\n",a,letter,toUse);
			putsprite(spr_or, x, y, font+toUse);			
		}
		i++;
		x+=8;
	}
}

// Function that will draw an .SCR to screen, including the attributes.
// this works because of the __FASTCALL__ which pushes the single argument into the HL register.
// we then load the screen location ($4000) and the screen size (6912) into the needed registers
// and copy.
void __FASTCALL__ DrawScrWithAttribs(void* scr)
{
#asm
	ld de, $4000
	ld bc, 6912
	ldir
#endasm
}

void __FASTCALL__ SetAttribsA(void* scr)
{
#asm
	ld de, $5800
	ld bc, 768
	ldir
#endasm
}


// the ASM call halt will block the code till the next frame is about to start.
// useful for trying to work with in a redraw frame.
void Halt()
{
#asm
	halt
#endasm
}

#endif