/*
	Atoms for the ZX Spectrum

	Atoms is a turn based game about taking over all the other players atoms.
	Players take turns in putting atoms down on a 10x7 grid, which they can do on empty squares
	or squares that contain the players atom(s).

	When the number of atoms is equal to the max size of the square (2 in corner, 3 edges 4 in the middle)
	the atom will explode and take over the squares directly around it, this will increase their size by 1
	and make them that players atoms at the same time.

	this continues until 1 player is left (huge chain reactions will happen).

	This code is based on more or less pure z88dk, there are a few places where ASM has been used and will be commented why.

	All the graphics will be kept in a single header, apart from any scr files which will be pulled in as well.

	There is also a small header of simple functions to make it easier to use (and to be copied to other projects)


	Compile:

	zcc.exe +zx -lndos -create-app main.c -o atoms
*/



#include <spectrum.h>
#include <graphics.h>
#include <games.h>
#include <sound.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h> 

#include "SentiaZX.h"

#include "sprites.h"

int LastCursorX=0;
int LastCursorY=0;

int CursorX=0;
int CursorY=0;

int m_TurnCount = 0;


#define Times8 << 3
#define Times16 << 4

#define Divide8 >> 3
#define Divide16 >> 4

// The colours of players (including no player 0).
uchar m_AttribPlayerMap[5];

// lazy array to map the sprites to their sizes.
char* m_Atoms[4];

// Holds the number of atoms that player has (including no player 0), and thus is a cheaty way to work out of the player is alive.
uchar m_Alive[5];

uchar m_CurrentPlayer;
uchar m_GameFinished = 0;

// 0 for not player, 1 for Human, 2 For AI
uchar m_PlayerSetup[5];


struct GridSquare
{
	uchar Player;
	uchar Size;
	uchar GrowSize;
	uchar Changed;
	uchar MaxSize;
} m_PlayerGrid[70];

// to move a grid square to a screen location its x *24, or x * 8 + x * 16 (which simplifies to x << 8 + x << 16).
// This could be made faster with a table lookup.
// This is a macro because its going to be used in a number of loops and it saves on jmp's with a funciton call
#define GridToScreen(z) (z << 3) + (z << 4)

// Draw the grid onscreen, 1 8x8 block at a time :/
// The screen should be empty so we can just OR the data directly to the screen.
void DrawGrid()
{
	int x = 0;
	int y = 0;

	

	for(y=0;y<7;y++)
	{
		for(x = 0;x<11;x++)
		{
			putsprite(spr_or, GridToScreen(x) , GridToScreen(y) + 8, sprite0);
			putsprite(spr_or, GridToScreen(x) , GridToScreen(y) + 16, sprite0);
		}
	}

	for(x = 0;x<10;x++)
	{
		for(y=0;y<8;y++)
		{
			putsprite(spr_or, GridToScreen(x) + 8, GridToScreen(y), sprite1);
			putsprite(spr_or, GridToScreen(x) + 16, GridToScreen(y), sprite1);
		}
	}


	for(x = 0;x<11;x++)
	{
		for(y=0;y<8;y++)
		{
			putsprite(spr_or, GridToScreen(x), GridToScreen(y),sprite2);
		}
	}
}

// Draw the cursor, but first reset the previous cursor back to being the grid.
// The force is there so we can force the drawing of the cursor even if it hasn't moved.
// Useful for the first draw and player changes.
void DrawCursor(int force)
{
	if(LastCursorX != CursorX || LastCursorY != CursorY || force)
	{
		int cursorXBase = (CursorX*8)+(CursorX*16);
		int cursorYBase = (CursorY*8)+(CursorY*16);
		int attribX = cursorXBase >> 3;
		int attribY = cursorYBase >> 3;


		int lastxBase = (LastCursorX*8)+(LastCursorX*16);
		int lastyBase = (LastCursorY*8)+(LastCursorY*16);
		int lastAttribX = lastxBase >> 3;
		int lastAttribY = lastyBase >> 3;

		// clear last position and put the bits back.

		if(LastCursorX != CursorX || LastCursorY != CursorY)
		{
			// Left Bars
			clga(lastxBase,lastyBase+8,8,16);
			putsprite(spr_or,lastxBase,lastyBase+8,sprite0);
			putsprite(spr_or,lastxBase,lastyBase+16,sprite0);

			SetAttrib(lastAttribY + 1,lastAttribX,PAPER_BLUE | INK_WHITE | BRIGHT);
			SetAttrib(lastAttribY + 2,lastAttribX,PAPER_BLUE | INK_WHITE | BRIGHT);



			// Right Bars
			clga(lastxBase+24,lastyBase+8,8,16);
			putsprite(spr_or,lastxBase+24,lastyBase+8,sprite0);
			putsprite(spr_or,lastxBase+24,lastyBase+16,sprite0);

			SetAttrib(lastAttribY + 1,lastAttribX +3,PAPER_BLUE | INK_WHITE | BRIGHT);
			SetAttrib(lastAttribY + 2,lastAttribX +3,PAPER_BLUE | INK_WHITE | BRIGHT);


			// Top Bars
			clga(lastxBase+8,lastyBase,16,8);
			putsprite(spr_or,lastxBase+8,lastyBase,sprite1);
			putsprite(spr_or,lastxBase+16,lastyBase,sprite1);
			SetAttrib(lastAttribY,lastAttribX+1,PAPER_BLUE | INK_WHITE | BRIGHT);
			SetAttrib(lastAttribY,lastAttribX+2,PAPER_BLUE | INK_WHITE | BRIGHT);



			// bottom Bars
			clga(lastxBase+8,lastyBase+24,16,8);
			putsprite(spr_or,lastxBase+8,lastyBase+24,sprite1);
			putsprite(spr_or,lastxBase+16,lastyBase+24,sprite1);
			SetAttrib(lastAttribY+3,lastAttribX+1,PAPER_BLUE | INK_WHITE | BRIGHT);
			SetAttrib(lastAttribY+3,lastAttribX+2,PAPER_BLUE | INK_WHITE | BRIGHT);
		}



		// draw the cursor at the right place.


		// Left Bars
		clga(cursorXBase,cursorYBase+8,8,16);
		putsprite(spr_or,cursorXBase,cursorYBase+8,sprite6);
		putsprite(spr_or,cursorXBase,cursorYBase+16,sprite7);

		SetAttrib(attribY + 1,attribX, m_AttribPlayerMap[m_CurrentPlayer] | BRIGHT);
		SetAttrib(attribY + 2,attribX, m_AttribPlayerMap[m_CurrentPlayer] | BRIGHT);



		// Right Bars
		clga(cursorXBase+24,cursorYBase+8,8,16);
		putsprite(spr_or,cursorXBase+24,cursorYBase+8,sprite4);
		putsprite(spr_or,cursorXBase+24,cursorYBase+16,sprite5);

		SetAttrib(attribY + 1,attribX +3, m_AttribPlayerMap[m_CurrentPlayer] | BRIGHT);
		SetAttrib(attribY + 2,attribX +3, m_AttribPlayerMap[m_CurrentPlayer] | BRIGHT);


		// Top Bars
		clga(cursorXBase+8,cursorYBase,16,8);
		putsprite(spr_or,cursorXBase+8,cursorYBase,sprite8);
		putsprite(spr_or,cursorXBase+16,cursorYBase,sprite9);
		SetAttrib(attribY,attribX+1, m_AttribPlayerMap[m_CurrentPlayer] | BRIGHT);
		SetAttrib(attribY,attribX+2, m_AttribPlayerMap[m_CurrentPlayer] | BRIGHT);



		// bottom Bars
		clga(cursorXBase+8,cursorYBase+24,16,8);
		putsprite(spr_or,cursorXBase+8,cursorYBase+24,sprite11);
		putsprite(spr_or,cursorXBase+16,cursorYBase+24,sprite10);
		SetAttrib(attribY+3,attribX+1, m_AttribPlayerMap[m_CurrentPlayer] | BRIGHT);
		SetAttrib(attribY+3,attribX+2, m_AttribPlayerMap[m_CurrentPlayer] | BRIGHT);


		// set all the attribs


		LastCursorX = CursorX;
		LastCursorY = CursorY;
	}
}


// Reset all the game base variables so we can start fresh.
void SetupGame()
{
	// Clear the Grid
	int i = 0;

	for(i=0; i < 70; i++)
	{
		m_PlayerGrid[i].Changed = 0;
		m_PlayerGrid[i].GrowSize = 0;
		m_PlayerGrid[i].Player = 0;
		m_PlayerGrid[i].Size = 0;
	}

	// Reset the Player
	// Player 0 is no player
	m_CurrentPlayer = 0;
	
	m_GameFinished = 0;

	m_TurnCount = 0;

	for(i=0;i<5;i++)
	{
		m_Alive[i] = 0;

		if (m_CurrentPlayer == 0 && m_PlayerSetup[i] != 0)
		{
			m_CurrentPlayer = i;
		}
	}



	CursorX = 0;
	CursorY = 0;
	LastCursorY = 0;
	LastCursorX = 0;
}


// Use the mask we defined above to remove the changed flag and the square state
// with that we can just return it directly.
uchar PlayerAtSquare(uchar x, uchar y)
{
	return m_PlayerGrid[(y * 10) + x].Player;
}

// Use the STATE mask to remove everything but the state and then move it 3 bits to the right so
// it will return to being the same number it was before we pushed it into the bitmask
uchar SizeAtSquare(uchar x, uchar y)
{
	return m_PlayerGrid[(y * 10) + x].Size;
}


void IncrementSquare(uchar x, uchar y, uchar player)
{
	if (x > 9 || y > 6)
	{

	}

	m_PlayerGrid[(y * 10) + x].GrowSize++;
	m_PlayerGrid[(y * 10) + x].Changed = 1;
	m_PlayerGrid[(y * 10) + x].Player = player;
}


// Wait for user input and do work based on it.
// This is effectively a state as it won't finish till the user has placed an atom
void PlayerInput()
{
	uchar moveOn = 1;
	while(moveOn)
	{
		int k = getk();
		switch( k ) 
		{
			case 11:
			case 51:			
			{
				LastCursorX=CursorX;
				LastCursorY=CursorY;
				CursorY--;
				bit_fx(0);
				break;
			}

			case 10:
			case 50:
			{
				LastCursorX=CursorX;
				LastCursorY=CursorY;
				CursorY++;
				bit_fx(0);
				break;
			}

			break;

			case 9:
			case 52:
			{
				LastCursorX=CursorX;
				LastCursorY=CursorY;
				CursorX++;
				bit_fx(0);
				break;
			}


			case 8:
			case 49:
			{
				LastCursorX=CursorX;
				LastCursorY=CursorY;
				CursorX--;
				bit_fx(0);
				break;
			}

			case 32:
			{
				uchar player = PlayerAtSquare(CursorX,CursorY);
				if(player == 0 || player == m_CurrentPlayer)
				{
					IncrementSquare(CursorX, CursorY,m_CurrentPlayer);
					moveOn = 0;
					//bit_fx(1);
				}
				break;
			}

#ifdef DEBUG
			// Helper to get some details out for debugging.
			case 99:
			{
				int j=0;
				printf("Turn: %d\n",m_TurnCount);
				printf("current Player: %d\n",m_CurrentPlayer);
				for(j=0;j<5;j++)
				{
					printf("Alive: %d = %d\n",j,m_Alive[j]);
				}

				printf("GameFinished: %d\n",m_GameFinished);

				printf("PlayerSetup: ");
				for (j = 0; j < 5; j++)
				{
					printf("%d,", m_PlayerSetup[j]);
				}
				break;
			}
#endif
		}

		if(CursorX > 9)
		{
			CursorX = 9;
		}
		else if (CursorX < 0)
		{
			CursorX = 0;
		}

		if(CursorY > 6)
		{
			CursorY = 6;
		}
		else if (CursorY < 0)
		{
			CursorY = 0;
		}

		Halt();
		DrawCursor(0);
	}
}


void DrawSquare(uchar x, uchar y, uchar size, uchar player, uchar bright)
{
	if (size)
	{
		uchar attrib = m_AttribPlayerMap[player];
		x = (x * 8) + (x * 16) + 8;
		y = (y * 8) + (y * 16) + 8;

		if (bright)
		{
			attrib = attrib | BRIGHT;
		}

		attrib = attrib | BRIGHT;

		clga(x, y, 16, 16);

		putsprite(SPR_OR, x, y, m_Atoms[size - 1]);

		// fun fact, doing ">> 3" is the same as "/ 8" but the Z80 can do ">> 3" easily where division are Sloooow
		SetAttribArea(x >> 3, (x >> 3) + 1, y >> 3, (y >> 3) + 1, attrib);
	}
}

void AnimateScreen()
{	
	uchar animating = 1;
	int x=0;
	int y=0;
	uchar j;
	uchar allSame = 1;
	char lastPlayer = -1;
	char exploded = 0;
	char done = 0;
	char grow = 0;

	while (animating)
	{
		Halt();

		lastPlayer = -1;
		allSame = 1;
		exploded = 0;
		grow = 0;
		done = 1;
		for (j = 0; j < 70;j++)
		{
			if (m_PlayerGrid[j].GrowSize && m_PlayerGrid[j].Size != 5)
			{
				m_PlayerGrid[j].Changed = 1;
				if (m_PlayerGrid[j].GrowSize)
				{
					done = 0;
				}

				m_PlayerGrid[j].Size += m_PlayerGrid[j].GrowSize;
				if (m_PlayerGrid[j].Size > m_PlayerGrid[j].MaxSize)
				{
					m_PlayerGrid[j].GrowSize = m_PlayerGrid[j].Size - m_PlayerGrid[j].MaxSize;
					m_PlayerGrid[j].Size = m_PlayerGrid[j].MaxSize;
				}
				else
				{
					m_PlayerGrid[j].GrowSize = 0;
				}
			}
		}

		for(j=0;j<5;j++)
		{
			m_Alive[j] = 0;
		}

		for(y = 0; y < 7; y++)
		{
			for(x = 0; x < 10; x++)
			{
				int i  = (y*10) + x;

				uchar size = m_PlayerGrid[i].Size;
				uchar player = m_PlayerGrid[i].Player;
				
				if(lastPlayer == -1 && player != 0)
				{
					lastPlayer = player;
				}
				else if(lastPlayer != player && player != 0)
				{
					allSame = 0;
				}

				m_Alive[player]++;
				
				if(m_PlayerGrid[i].Changed)
				{
					animating = 2;

					if(size == m_PlayerGrid[i].MaxSize)
					{
						DrawSquare(x,y,4, player,1);
						m_PlayerGrid[i].Changed = 1;
						m_PlayerGrid[i].Size = 5;
						exploded=1;
					}
					else if (size == 5)
					{						
						uchar attribX = (GridToScreen(x) + 8) >> 3;
						uchar attribY = (GridToScreen(y) + 8) >> 3;

						m_PlayerGrid[i].Size = 0;
						m_PlayerGrid[i].Player = 0;
						m_PlayerGrid[i].Changed = 0;
						// Clear it and just set the Changed flag

						// Clear the square, don't bother clearing the pixels, lets just hide it :)
						SetAttribArea(attribX,attribX+1,attribY,attribY+1,m_AttribPlayerMap[0]);

						// Do explosion logic!

						if(y == 0 && x == 0)
						{
							IncrementSquare(x+1,y,player);
							IncrementSquare(x,y+1,player);
						}
						else if(y == 0 && x == 9)
						{
							IncrementSquare(x-1,y,player);
							IncrementSquare(x,y+1,player);
						}
						else if(y == 0)
						{
							IncrementSquare(x+1,y,player);
							IncrementSquare(x-1, y,player);
							IncrementSquare(x,y+1,player);
						}
						else if(y == 6 && x == 0)
						{
							IncrementSquare(x+1,y,player);
							IncrementSquare(x,y-1,player);
						}
						else if (x == 0)
						{
							IncrementSquare(x+1,y,player);
							IncrementSquare(x, y-1,player);
							IncrementSquare(x,y+1,player);
						}
						else if(y == 6 && x == 9)
						{
							IncrementSquare(x-1,y,player);
							IncrementSquare(x,y-1,player);
						}
						else if(y == 6)
						{
							IncrementSquare(x+1,y,player);
							IncrementSquare(x-1, y,player);
							IncrementSquare(x,y-1,player);
						}
						else if(x == 9)
						{
							IncrementSquare(x-1,y,player);
							IncrementSquare(x, y-1,player);
							IncrementSquare(x,y+1,player);
						}
						 else
						{
							IncrementSquare(x+1,y,player);
							IncrementSquare(x, y-1,player);
							IncrementSquare(x,y+1,player);
							IncrementSquare(x-1,y,player);
						}
					}
					else
					{
						if (size)
						{
							DrawSquare(x, y, size, player, 0);
							m_PlayerGrid[i].Player = player;
							m_PlayerGrid[i].Changed = 0;
							grow = 1;
						}
					}
				}
			}
		}	
		if(done)
		{
			animating--;
		}

		if(exploded)
		{
			bit_fx2(2);
		}
		else if (grow)
		{
			bit_fx(1);
		}

		// Allow for early out!
		if(allSame && m_TurnCount)
		{
			m_GameFinished = 1;
			animating = 0;
		}

		if (getk() == 99)
		{
			printf("Turn: %d\n", m_TurnCount);
			printf("current Player: %d\n", m_CurrentPlayer);

			printf("GameFinished: %d\n", m_GameFinished);
			printf("allSame: %d\n", allSame);
			printf("animating: %d\n", animating);
			printf("done: %d\n", done);

			while (1)
			{

			}
		}

	}
}


// Changes player and does a check to see if a player has won.
void CheckForFinished()
{
	uchar startingPlayer = m_CurrentPlayer;
	while (1)
	{
		m_CurrentPlayer++;
		
		if(m_CurrentPlayer > 4)
		{
			m_CurrentPlayer = 1;
			m_TurnCount++;
		}

		if(m_TurnCount)
		{
			if(m_CurrentPlayer == startingPlayer)
			{
				m_GameFinished = 1;
				break;
			}
			else if(m_Alive[m_CurrentPlayer] && m_PlayerSetup[m_CurrentPlayer] != 0)
			{
				break;
			}
		}
		else
		{
			if (m_PlayerSetup[m_CurrentPlayer] != 0)
			{
				break;
			}
		}
	}	
}


// Show the winner screen
void Winner()
{
	uchar i = 0;
	uchar attrib = PAPER_MAGENTA | INK_BLACK;
	clg();
	getk();

	Halt();
	DrawScrWithAttribs(WinnerScr);


	switch (m_CurrentPlayer)
	{
		case 1:
			attrib = PAPER_MAGENTA | INK_BLACK;
			break;

		case 2:
			attrib = PAPER_RED | INK_BLACK;
			break;

		case 3:
			attrib = PAPER_GREEN | INK_BLACK;
			break;

		case 4:
			attrib = PAPER_YELLOW | INK_BLACK;
			break;
	default:
		break;
	}

	attrib = attrib | BRIGHT;

	SetAttribArea(3, 28, 1, 6, attrib);
	SetAttrib(5, 2, attrib);	
	SetAttribArea(4, 5, 6, 6, PAPER_BLUE | INK_BLACK | BRIGHT);
	SetAttribArea(12, 13, 1, 1, PAPER_BLUE | INK_BLACK | BRIGHT);
	SetAttrib(1, 17, PAPER_BLUE | INK_BLACK | BRIGHT);
	SetAttrib(6, 10, PAPER_BLUE | INK_BLACK | BRIGHT);
	SetAttrib(6, 12, PAPER_BLUE | INK_BLACK | BRIGHT);
	SetAttrib(6, 17, PAPER_BLUE | INK_BLACK | BRIGHT);

	
	SetAttribArea(7, 7, 13, 15, attrib);
	SetAttribArea(8, 8, 13, 17, attrib);
	SetAttribArea(9, 9, 13, 18, attrib);
	SetAttribArea(10, 10, 13, 19, attrib);
	SetAttribArea(11, 11, 14, 20, attrib);
	SetAttribArea(11, 11, 22, 23, attrib);
	SetAttribArea(12, 12, 18, 23, attrib);
	SetAttribArea(13, 13, 19, 23, attrib);
	SetAttribArea(14, 16, 20, 23, attrib);
	SetAttribArea(17, 17, 19, 23, attrib);
	SetAttribArea(18, 18, 18, 23, attrib);
	SetAttribArea(19, 19, 15, 23, attrib);
	SetAttribArea(20, 20, 13, 20, attrib);
	SetAttribArea(21, 21, 13, 18, attrib);
	SetAttribArea(22, 22, 13, 16, attrib);
	SetAttribArea(23, 23, 13, 14, attrib);

	
	for (i = 0; i < 100; i++)
	{
		getk();
		Halt();
	}

	while (1)
	{
		if (getk() != 0)
		{
			bit_fx4(7);
			break;
		}
	}
}


// Setup the game tables that won't change.
// Could be setup manually and stored but that's more work.
void SetupDefaults()
{
	int x = 0;
	int y = 0;
	int i = 0;

	m_Atoms[0] = sprite3;
	m_Atoms[1] = sprite13;
	m_Atoms[2] = sprite12;
	m_Atoms[3] = sprite14;

	m_AttribPlayerMap[0] = PAPER_BLUE | INK_BLUE | BRIGHT;
	m_AttribPlayerMap[1] = PAPER_BLUE | INK_MAGENTA | BRIGHT;
	m_AttribPlayerMap[2] = PAPER_BLUE | INK_RED | BRIGHT;
	m_AttribPlayerMap[3] = PAPER_BLUE | INK_GREEN | BRIGHT;
	m_AttribPlayerMap[4] = PAPER_BLUE | INK_YELLOW | BRIGHT;

	for (i = 0; i < 5; i++)
	{
		m_PlayerSetup[i] = 0;
	}

	i = 0;
	for(y=0;y<7;y++)
	{
		for(x = 0;x<10;x++)
		{



			if(y == 0 && x == 0)
			{				
				m_PlayerGrid[i].MaxSize = 2;
			}
			else if(y == 0 && x == 9)
			{
				m_PlayerGrid[i].MaxSize = 2;
			}
			else if(y == 0)
			{
				m_PlayerGrid[i].MaxSize = 3;
			}
			else if(y == 6 && x == 0)
			{
				m_PlayerGrid[i].MaxSize = 2;
			}
			else if (x == 0)
			{
				m_PlayerGrid[i].MaxSize = 3;
			}
			else if(y == 6 && x == 9)
			{
				m_PlayerGrid[i].MaxSize = 2;
			}
			else if(y == 6)
			{
				m_PlayerGrid[i].MaxSize = 3;
			}
			else if(x == 9)
			{
				m_PlayerGrid[i].MaxSize = 3;
			}
			else
			{
				m_PlayerGrid[i].MaxSize = 4;
			}

			i++;
		}
	}
}

// Test function for showing all the Atoms.
void DrawAllSizes()
{
	uchar player = 1;
	uchar x,y;
	uchar defaultAttrib = PAPER_BLUE | INK_WHITE;
	Halt();
	
	SetAttribArea(0,32,0,24, defaultAttrib);		
	clg();

	DrawGrid();

	Halt();
	for(y = 0; y < 7; y++)
	{
		for(x = 0; x < 10; x++)
		{
			DrawSquare(x,y,m_PlayerGrid[(y*10) + x].MaxSize,player++,0);

			if(player > 4)
			{
				player = 1;
			}
		}
	}


	while (1)
	{
		Halt();
	}
}

void AIInput()
{
	uchar attempt = 5;
	uchar done = 0;
	uchar rndx, rndy, player;

	while (attempt > 0 && done == 0)
	{		
		do
		{
			rndx = rand();
			rndx = (rndx & 0xF);
		} while (rndx > 9);

		do
		{
			rndy = rand();
			rndy = rndy & 0x7;
		} while (rndy > 6);

		player = PlayerAtSquare(rndx, rndy);

		if (player == 0 || player == m_CurrentPlayer)
		{
			IncrementSquare(rndx, rndy, m_CurrentPlayer);
			done = 1;
		}

		attempt--;
	}

	while (!done)
	{
		rndx++;

		if (rndx > 9)
		{
			rndx = 0;
			rndy++;
		}

		if (rndy > 6)
		{
			rndy = 0;
			rndx = 0;
		}

		player = PlayerAtSquare(rndx, rndy);

		if (player == 0 || player == m_CurrentPlayer)
		{
			IncrementSquare(rndx, rndy, m_CurrentPlayer);
			done = 1;
		}
	}

	CursorX = rndx;
	CursorY = rndy;
	DrawCursor(0);

}

void GameplayLoop()
{
	uchar defaultAttrib = PAPER_BLUE | INK_WHITE | BRIGHT;
	char a[16];
	SetAttribArea(0,32,0,24, defaultAttrib);		
	clg();
	DrawGrid();	

	DrawString(16,176,"turn: 1",Font2);

	SetupGame();
	getk(); // Clear the keyboard buffer

	while(!m_GameFinished)
	{
		DrawCursor(1);

		if (m_PlayerSetup[m_CurrentPlayer] == 1)
		{
			PlayerInput();
		}
		else if (m_PlayerSetup[m_CurrentPlayer] == 2)
		{
			AIInput();
		}


		AnimateScreen();
		CheckForFinished();

		clga(16, 176, 128, 8);

		sprintf(a, "turn: %d", (m_TurnCount+1));
		//DrawString(16, 176, "turn: ", Font2);
		//a[0] = m_TurnCount + '0';
		//a[1] = 0;
		DrawString(22, 176, a, Font2);
	}

	Winner();
}

uchar MenuX[] = {192, 16, 96, 192 };
uchar MenuY[] = { 0, 96, 96, 96 };


void DrawMenu(uchar* setup, char changed)
{
	
	uchar i = 0;
	for (i = 0; i < 4; i++)
	{
		if (changed == -1 || i == changed)
		{
			uchar state = setup[i];
			uchar x = (MenuX[i] >> 3);
			uchar y = (MenuY[i] >> 3);

			clga(MenuX[i], MenuY[i], 48, 48);
			SetAttribArea(x, x + 5, y, y + 5, INK_BLACK | PAPER_BLUE | BRIGHT);

			switch (state)
			{
			case 0:
			{
				putsprite(SPR_OR, MenuX[i], MenuY[i], Empty);
				SetAttribArea(x + 2, x + 4, y, y + 5, INK_BLACK | PAPER_WHITE | BRIGHT);
				SetAttribArea(x + 1, x + 1, y + 4, y + 5, INK_BLACK | PAPER_WHITE | BRIGHT);
				SetAttrib(y + 5, x + 5, INK_BLACK | PAPER_WHITE | BRIGHT);
				break;
			}

			case 1:
			{
				putsprite(SPR_OR, MenuX[i], MenuY[i], Prof);
				SetAttribArea(x + 2, x + 4, y, y + 5, INK_BLACK | PAPER_WHITE | BRIGHT);
				SetAttribArea(x + 1, x + 1, y + 4, y + 5, INK_BLACK | PAPER_WHITE | BRIGHT);
				SetAttrib(y + 5, x + 5, INK_BLACK | PAPER_WHITE | BRIGHT);
				break;
			}

			case 2:
			{
				putsprite(SPR_OR, MenuX[i], MenuY[i], Robo);
				SetAttribArea(x+2, x+4, y, y+3, INK_BLACK | PAPER_WHITE | BRIGHT);
				SetAttribArea(x, x + 5, y + 4, y + 5, INK_BLACK | PAPER_WHITE | BRIGHT);
				break;
			}
			default:
				break;
			}
		}
	}
	
}

void ColourStand(uchar i, uchar attrib)
{
	int x = (MenuX[i] >> 3);
	int y = ((MenuY[i] + 48) >> 3);
	SetAttribArea(x, x + 5, y, y + 3, attrib | INK_BLACK | BRIGHT);
	SetAttribArea(x + 1, x + 4, y + 4, y + 5, attrib | INK_BLACK | BRIGHT);
}




void MainMenu()
{
	unsigned int counter = 0;
	uchar defaultAttrib = PAPER_BLUE | INK_BLACK | BRIGHT;
	int i = 0;
	uchar setup[4];
	//int size = 768;
	//uchar attribs[768];

	setup[0] = 1;
	setup[1] = 2;
	setup[2] = 0;
	setup[3] = 0;
	

	//for (i = 0; i < size; i++)
	{
		//attribs[i] = PAPER_BLUE | INK_BLACK | BRIGHT;
	}

	Halt();
	clg();
	SetAttribArea(0, 31, 0, 23, defaultAttrib);

	putsprite(SPR_OR, 0, 0, Blackboard);
	SetAttribArea(0, 19, 0, 9, PAPER_WHITE | INK_BLACK | BRIGHT);

	putsprite(SPR_OR, MenuX[0], MenuY[0] + 48, Player1);
	ColourStand(0, PAPER_MAGENTA);

	putsprite(SPR_OR, MenuX[1], MenuY[1] + 48, Player2);
	ColourStand(1, PAPER_RED);

	putsprite(SPR_OR, MenuX[2], MenuY[2] + 48, Player3);
	ColourStand(2, PAPER_GREEN);

	putsprite(SPR_OR, MenuX[3], MenuY[3] + 48, Player4);
	ColourStand(3, PAPER_YELLOW);

	//SetAttribArrayA(0, 31, 0, 23, attribs, defaultAttrib);

	
	DrawString(8, 16, "use number keys to", Font2);
	DrawString(24, 24, "setup the game", Font2);

	DrawString(32, 40, "then press", Font2);
	DrawString(24, 48, "space to play", Font2);
	
	DrawMenu(setup,-1);

	getk();

	while (1)
	{
		uchar k = 0;
		char p= -1;
		counter++;
		Halt();
		k = getk();

		switch (k)
		{
			case 49:
			{
				p = 0;
				break;
			}

			case 50:
			{
				p = 1;
				break;
			}

			case 51:
			{
				p = 2;
				break;
			}

			case 52:
			{
				p = 3;
				break;
			}

			case 32:
			{
				uchar playerCount = 0;

				m_PlayerSetup[0] = 0;
				for (i = 1; i < 5; i++)
				{
					m_PlayerSetup[i] = setup[i-1];

					if (setup[i - 1] != 0)
					{
						playerCount++;
					}
				}

				if (playerCount > 1)
				{
					bit_fx4(7);
					srand(clock());
					return;
				}
				
				break;
			}
		}

		if (p != -1)
		{
			setup[p]++;

			if (setup[p] > 2)
			{
				setup[p] = 0;
			}


			Halt();
			bit_fx(0);
			DrawMenu(setup,p);
		}
	}
}



void main ()
{
	int x = 0;
	int y = 0;
	int i =0;	

	zx_border (BLUE);
	SetupDefaults();

	//TitleScreen();
	//DrawAllSizes();

	getk(); // Clear the keyboard buffer
	while(1)
	{
		MainMenu();
		GameplayLoop();
	}	
}
