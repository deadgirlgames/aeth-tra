#include <citro2d.h>
#include <citro3d.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <3ds.h>
#include <time.h>

#define MAX_SPRITES   768
#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

	
//player overworld variables
int playerX = 200;
int playerY = 90;
int cameraX = 0;
int cameraY = 0;
float playerGravity = 0; //how much gravity to enact this frame
float playerXAcc = 0;
bool isMoving = false; //is player moving
int jumpBoost = 0; //allows for increasing jump height by holding the jump button
int attackTimer = 0; //frames for attack to be on screen
int tempX = 0; //mostly for moving sprites around and anims

//player battle variables	
int playerCharacter = 0; //who the player is playing as
	
//scene variables
int frames = 0; //just a frame counter
u32 tileMap1[400][400]; //tile map
u32 collisionMap[400][400];
u32 sceneType = 0; //0 menu, 1 game. 2 cutscene
u32 sceneNumber = 0; //
bool paused = false;
u32 menuSelect = 0;
u32 menuOptions = 1; //number of menu options (starts at 0)
int menuID = 0; //what menu the player is in 
int menuType = 0; //0 normal, 1 big side menu

//location variables
u32 convID = 0; //conversation ID

//textbox variables
int textboxDelay = 10; //delay between each letter on a textbox
int textboxTimer = 0; //called each time a new textbox is called
bool textboxOpen = false; //so you don't reset the timer each frame
bool textboxDone = false; //when the textbox is done typing

//battle variables
int battleType = 0; //0 no battle, 1 reg battle, 2 boss, 3 story, 4 tutorial
int battleID = 0; //more granular battletype

//sprite variables
typedef struct { //simple sprite sheet struct
	C2D_Sprite spr;
} Sprite;

static C2D_SpriteSheet mainSheet;
static C2D_SpriteSheet uiSheet;
static C2D_SpriteSheet fontSheet;
static C2D_SpriteSheet bgSheet;
static C2D_SpriteSheet tileSheet;
static Sprite mainSprites[MAX_SPRITES];
static Sprite uiSprites[MAX_SPRITES];
static Sprite fontSprites[MAX_SPRITES];
static Sprite bgSprites[MAX_SPRITES];
static Sprite tileSprites[MAX_SPRITES];

//sound variables
u8* buffer;
u32 size;
bool musicChange = true;

//loaded variables (r/w romfs)
int testArray[16];

//declare functions
void initSprites();
void drawMainSprite(int spriteNumber, int x, int y);
void drawUISprite(int spriteNumber, int x, int y);
void drawBGSprite(int spriteNumber, int x, int y);
void drawTileSprite(int spriteNumber, int x, int y);
void drawTileArray(int tileType, int x, int y, int w, int h);
void drawTiles(float x, float y);
void createMap1();
void drawFontSprite(int spriteNumber, int x, int y, float scale);
void drawWordSprite(char input[], int x, int y, float scale);
void drawTextbox(char input[], float speed);
void resetTextboxTimer();
int textboxInteract (int type);
int parseLetter (char charIn);
void drawBottomScreen();
void audio_stop(void);
void audio_ch8(const char *audio);
void audio_ch9(const char *audio);
void audio_ch10(const char *audio);
void audio_ch11(const char *audio);
void loadFiles();
void drawScene();
			
//---------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
//---------------------------------------------------------------------------------
	//init libs
	romfsInit();
	gfxInitDefault();
	srvInit();
	aptInit();
	hidInit();
	csndInit();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	loadFiles();
	
	//load spritesheet
	mainSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
	if (!mainSheet) svcBreak(USERBREAK_PANIC);
	uiSheet = C2D_SpriteSheetLoad("romfs:/gfx/ui-buttons.t3x");
	if (!uiSheet) svcBreak(USERBREAK_PANIC);
	fontSheet = C2D_SpriteSheetLoad("romfs:/gfx/font.t3x");
	if (!uiSheet) svcBreak(USERBREAK_PANIC);
	bgSheet = C2D_SpriteSheetLoad("romfs:/gfx/backgrounds.t3x");
	if (!uiSheet) svcBreak(USERBREAK_PANIC);
	tileSheet = C2D_SpriteSheetLoad("romfs:/gfx/tiles.t3x");
	if (!uiSheet) svcBreak(USERBREAK_PANIC);

	//initialize sprites
	initSprites();
	createMap1();

	//create screens
	C3D_RenderTarget* top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(top, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTarget* bot = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(bot, GFX_BOTTOM, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	//main loop
	while (aptMainLoop())
	{
		hidScanInput();
		
		// Create colors (background)
		u32 clrClear = C2D_Color32(0x00, 0x00, 0x00, 0x00); //background color
	
		//clear screens
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(top, clrClear);
		C2D_TargetClear(bot, clrClear);
		
		//bottom screen
		C2D_SceneBegin(bot);
		C3D_FrameDrawOn(bot);
		drawBottomScreen();
		
		//top screen
		C2D_SceneBegin(top);
		C3D_FrameDrawOn(top);
		
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();
		
		//game loop
		if (paused == false && sceneType == 1) {
		
		isMoving = false;
		if (playerGravity < 5) playerGravity = playerGravity + 1;
		int playerTileY = (int) (playerY + 20) / 16;
		int playerTileX = (int) (playerX + 0) / 16;
		//controls
		if (kDown & KEY_START && battleType == 0) { //pause game
			paused = true; 
			menuSelect = 0;
		}
		if (kDown & KEY_X && battleType == 0) { //pause game
			paused = true; 
			menuSelect = 0;
		}
		if (kHeld & KEY_DRIGHT) { //pad right
			cameraX = cameraX + 10;
		} 
		if (kHeld & KEY_DLEFT) { //pad left
			cameraX = cameraX - 10;

		} 
		if (kHeld & KEY_DUP) {
			cameraY = cameraY - 10;
		}
		if (kHeld & KEY_DDOWN) {
			cameraY = cameraY + 10;
		}
		if (kHeld & KEY_CPAD_RIGHT) { //pad right
			if (playerXAcc <= 0) playerXAcc = 0;
			playerXAcc = playerXAcc + 1;
		} 
		if (kHeld & KEY_CPAD_LEFT) { //pad left
			if (playerXAcc >= 0) playerXAcc = 0;
			playerXAcc = playerXAcc - 1;
		} 
		if (kDown & KEY_B) { //B
			if (collisionMap[playerTileX][playerTileY] == 1) {
				playerGravity = -4;
			} else if (collisionMap[playerTileX][playerTileY-1] == 1) {
				playerGravity = -5;
			}
		} else if (kHeld & KEY_B) {
			if (jumpBoost > 0) {
				playerGravity = playerGravity - 1.5;
				jumpBoost--;
			}
		}
		if (kDown & KEY_Y) { //Y
		
		}
		if (kDown & KEY_L) { //L
			
		}
			//collisions
			if (collisionMap[playerTileX][playerTileY] == 1 && playerGravity >= 0) { //tile below
				playerGravity = 0;
				playerY = (playerTileY * 16) - 20;
				if (kHeld & KEY_B) {} else jumpBoost = 5;
			} else if (collisionMap[playerTileX][playerTileY-1] == 1) { //inside tile
				playerY = ((playerTileY-1) * 16) - 20;
				playerGravity = -1;
			}
			if (collisionMap[playerTileX+1][playerTileY-1] == 1) { //right bump
				if (playerXAcc >= 0) {
					playerXAcc = 0;
					playerX = playerTileX * 16;
				}
			}
			if (collisionMap[playerTileX-1][playerTileY-1] == 1) { //left bump
				if (playerXAcc <= 0) {
					playerXAcc = 0;
					playerX = playerTileX * 16;
				}
			}
			playerX = playerX + playerXAcc;
			if (playerXAcc > 0) playerXAcc = playerXAcc - 0.5;
			else if (playerXAcc < 0) playerXAcc = playerXAcc + 0.5;
			if (playerXAcc > 5) playerXAcc = 5;
			else if (playerXAcc < -5) playerXAcc = -5;
			playerY = playerY + playerGravity;
			drawScene();
		}

		 //end of active loop
		else if (sceneType == 1) { //paused
		menuOptions = 3;
		
		if (kDown & KEY_START || kDown & KEY_A) {
			if (menuSelect == 0) paused = false;
			if (menuSelect == 1) {
				sceneType = 0;
				menuSelect = 0;
				sceneNumber = 0;
			}
			if (menuSelect == 2) {
				sceneType = 0;
				menuSelect = 0;
			}
			if (menuSelect == 3) break;
		}

		if (kDown & KEY_X) {
		paused = false;
			}
			if ((kDown & KEY_DUP && menuSelect > 0) || (kDown & KEY_CPAD_UP && menuSelect > 0)) {
				menuSelect--;
				audio_ch10("romfs:/sfx/ui/cursor-move.bin");
			}
			if ((kDown & KEY_DDOWN && menuSelect < menuOptions) || (kDown & KEY_CPAD_DOWN && menuSelect < menuOptions)) {
				menuSelect++;
				audio_ch10("romfs:/sfx/ui/cursor-move.bin");
			}	
		} 
		else if (sceneType == 0) { //main menu
			
			if (sceneNumber == 0) { //main menu menu
				menuOptions = 3;
				drawBGSprite(1, 0, 0);
				if (menuSelect == 0) drawUISprite(4, 100, 50);
				else if (menuSelect == 1) drawUISprite(4, 100, 80);
				else if (menuSelect == 2) drawUISprite(4, 100, 110);
				else if (menuSelect == 3) drawUISprite(4, 100, 140);
				drawWordSprite("Aeth Tra TI", 90, 2, 1);
				drawWordSprite("singleplayer", 130, 50, 0.5);
				drawWordSprite("multiplayer", 130, 80, 0.5);
				drawWordSprite("options", 130, 110, 0.5);
				drawWordSprite("quit", 130, 140, 0.5);
			}

			else if (sceneNumber == 1) { //single player mode menu
				menuOptions = 5;
				drawBGSprite(1, 0, 0);
				if (menuSelect == 0) drawUISprite(4, 100, 50);
				else if (menuSelect == 1) drawUISprite(4, 100, 80);
				else if (menuSelect == 2) drawUISprite(4, 100, 110);
				else if (menuSelect == 3) drawUISprite(4, 100, 140);
				else if (menuSelect == 4) drawUISprite(4, 100, 170);
				else if (menuSelect == 5) drawUISprite(4, 100, 200);
				drawWordSprite("Select Mode", 90, -5, 1);
				drawWordSprite("tournaments", 130, 50, 0.5);
				drawWordSprite("single battle", 130, 80, 0.5);
				drawWordSprite("practice mode", 130, 110, 0.5);
				drawWordSprite("-", 130, 140, 0.5);
				drawWordSprite("-", 130, 170, 0.5);
				drawWordSprite("back", 130, 200, 0.5);
			}
			if (sceneNumber == 2) { //character select
				menuOptions = 5;
				drawBGSprite(1, 0, 0);
				if (menuSelect == 0) drawUISprite(4, 100, 50);
				else if (menuSelect == 1) drawUISprite(4, 100, 80);
				else if (menuSelect == 2) drawUISprite(4, 100, 110);
				else if (menuSelect == 3) drawUISprite(4, 100, 140);
				else if (menuSelect == 4) drawUISprite(4, 100, 170);
				else if (menuSelect == 5) drawUISprite(4, 100, 200);
				drawWordSprite("Character", 90, -5, 1);
				drawWordSprite("karlyn", 130, 50, 0.5);
				drawWordSprite("forte", 130, 80, 0.5);
				drawWordSprite("eliza", 130, 110, 0.5);
				drawWordSprite("joanna", 130, 140, 0.5);
				drawWordSprite("minerva", 130, 170, 0.5);
				drawWordSprite("back", 130, 200, 0.5);
			}
			if ((kDown & KEY_DUP && menuSelect > 0) || (kDown & KEY_CPAD_UP && menuSelect > 0)) {
				menuSelect--;
				audio_ch10("romfs:/sfx/ui/cursor-move.bin");
			}
			if ((kDown & KEY_DDOWN && menuSelect < menuOptions) || (kDown & KEY_CPAD_DOWN && menuSelect < menuOptions)) {
				menuSelect++;
				audio_ch10("romfs:/sfx/ui/cursor-move.bin");
			}
			
			if (kDown & KEY_A || kDown & KEY_START) {
			audio_ch10("romfs:/sfx/ui/confirm.bin");
				
				if (sceneNumber == 0) { //main menu
					if (menuSelect == 3) break;
					else if (menuSelect == 0) sceneNumber = 1; //mode menu
				}
				 else if (sceneNumber == 1) { //mode menu
					if (menuSelect == 0) sceneNumber = 2; //tournaments
					else if (menuSelect == 1) sceneNumber = 2; //single battle
					else if (menuSelect == 2) sceneNumber = 2; //practice mode
					else if (menuSelect == 3) ;
					else if (menuSelect == 4) ;
					else if (menuSelect == 5) sceneNumber = 0;
					
				}
				else if (sceneNumber == 2) { //character select
					if (menuSelect == 0) {sceneType = 1; playerCharacter = 0;} //karlyn
					else if (menuSelect == 1) ;//forte
					else if (menuSelect == 2) ; //eliza
					else if (menuSelect == 3) ; //joanna
					else if (menuSelect == 4) ; //minerva
					else if (menuSelect == 5) sceneNumber = 1; //back
				}
				else if (sceneNumber == 3) { //map menu

				}
				menuSelect = 0; //reset cursor pos
			}
			if (kDown & KEY_B) {
				if (sceneNumber == 1) sceneNumber = 0;
				else if (sceneNumber == 2) sceneNumber = 1;
			}
			drawWordSprite("v 0.02", 1, 215, 0.5);
		} else if (sceneType == 2) { 
		
		}
		
		if (musicChange == true && frames > 60) {
			audio_ch8("romfs:/music/forest-of-sorrow-mono.bin");
			musicChange = false;
		}
		
		frames++;
		textboxTimer++;
		C3D_FrameEnd(0);
	
	}
	//clear audio
	audio_stop();
	audio_stop();
	csndExit();
	hidExit();
	aptExit();
	srvExit();
	//clear sprite sheets
	C2D_SpriteSheetFree(mainSheet);
	C2D_SpriteSheetFree(uiSheet);
	//deinit libs
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}

void initSprites() {
	for (int i = 0; i < C2D_SpriteSheetCount(mainSheet); i++) {
		srand(time(NULL));
		Sprite* mainsprite = &mainSprites[i];
		C2D_SpriteFromSheet(&mainsprite->spr, mainSheet, i);
		C2D_SpriteSetCenter(&mainsprite->spr, 0, 0);
	}
	for (int i = 0; i < C2D_SpriteSheetCount(uiSheet); i++) {
		srand(time(NULL));
		Sprite* uisprite = &uiSprites[i];
		C2D_SpriteFromSheet(&uisprite->spr, uiSheet, i);
		C2D_SpriteSetCenter(&uisprite->spr, 0, 0);
	}
	for (int i = 0; i < C2D_SpriteSheetCount(fontSheet); i++) {
		srand(time(NULL));
		Sprite* fontsprite = &fontSprites[i];
		C2D_SpriteFromSheet(&fontsprite->spr, fontSheet, i);
		C2D_SpriteSetCenter(&fontsprite->spr, 0, 0);
	}
	for (int i = 0; i < C2D_SpriteSheetCount(bgSheet); i++) {
		srand(time(NULL));
		Sprite* bgsprite = &bgSprites[i];
		C2D_SpriteFromSheet(&bgsprite->spr, bgSheet, i);
		C2D_SpriteSetCenter(&bgsprite->spr, 0, 0);
	}
	for (int i = 0; i < C2D_SpriteSheetCount(tileSheet); i++) {
		srand(time(NULL));
		Sprite* tilesprite = &tileSprites[i];
		C2D_SpriteFromSheet(&tilesprite->spr, tileSheet, i);
		C2D_SpriteSetCenter(&tilesprite->spr, 0, 0);
	}
}

void drawMainSprite (int spriteNumber, int x, int y) {
	Sprite* playerSprite = &mainSprites[spriteNumber];
	C2D_SpriteSetPos(&playerSprite->spr, x, y);
	C2D_DrawSprite(&mainSprites[spriteNumber].spr);
}

void drawUISprite (int spriteNumber, int x, int y) {
	Sprite* uiSprite = &uiSprites[spriteNumber];
	C2D_SpriteSetPos(&uiSprite->spr, x, y);
	C2D_DrawSprite(&uiSprites[spriteNumber].spr);
}

void drawBGSprite (int spriteNumber, int x, int y) {
	Sprite* bgSprite = &bgSprites[spriteNumber];
	C2D_SpriteSetPos(&bgSprite->spr, x, y);
	C2D_DrawSprite(&bgSprites[spriteNumber].spr);
}

void drawFontSprite (int spriteNumber, int x, int y, float scale) {
	Sprite* fontSprite = &fontSprites[spriteNumber];
	C2D_SpriteSetPos(&fontSprite->spr, x, y);
	C2D_SpriteSetScale(&fontSprite->spr, scale, scale);
	C2D_DrawSprite(&fontSprites[spriteNumber].spr);
}

void drawTileSprite (int spriteNumber, int x, int y) {
	Sprite* tileSprite = &tileSprites[spriteNumber];
	C2D_SpriteSetPos(&tileSprite->spr, x, y);
	C2D_DrawSprite(&tileSprites[spriteNumber].spr);
}

void drawTileArray (int tileType, int x, int y, int w, int h) {
	int indexX = x;
	int indexY = y;
	int indexW = x + w;
	int indexH = y + h;
	for (int i = indexX; i < indexW; i++) {
		for (int e = indexY; e < indexH; e++) {
			if (i >= 0 && i <= 400 && e >= 0 && e <= 400)
			tileMap1[i][e] = tileType;
		}
	}
}

void drawCollisionArray (int tileType, int x, int y, int w, int h) {
	int indexX = x;
	int indexY = y;
	int indexW = x + w;
	int indexH = y + h;
	for (int i = indexX; i < indexW; i++) {
		for (int e = indexY; e < indexH; e++) {
			if (i >= 0 && i <= 400 && e >= 0 && e <= 400)
			collisionMap[i][e] = tileType;
		}
	}
}

void drawTiles (float x, float y) {
	int indexX = x;
	int indexY = y;
	for (int i = 0; i < 400; i++) {
		for (int e = 0; e < 400; e++) {
			if (i >= 0 && i <= 400 && e >= 0 && e <= 400) {
			int adjX = i * 16;
			adjX = adjX - indexX;
			int adjY = e * 16;
			adjY = adjY - indexY;
			if (adjY > -32 && adjY < 272 && adjX > -32 && adjX < 432) {
				drawTileSprite(tileMap1[i][e], adjX, adjY);
				}
			
			}
		}
	}
}

void createMap1() {
	drawTileArray(0, 0, 0, 400, 400);
	drawCollisionArray(0, 0, 0, 400, 400);
	drawTileArray(14, 0, 9, 100, 1);
	drawCollisionArray(1, 0, 9, 100, 1);
	drawTileArray(14, 20, 7, 2, 2);
	drawCollisionArray(1, 20, 7, 2, 2);
	/*
	drawTileArray(1, 16, 4, 3, 3);
	tileMap1[17][7] = 1;
	tileMap1[18][7] = 1;
	tileMap1[18][8] = 1;
	
	drawTileArray(14, 4, 9, 18, 1);
	drawTileArray(7, 4, 10, 18, 3);
	drawTileArray(8, 10, 6, 4, 6);
	tileMap1[10][6] = 11;
	tileMap1[13][6] = 12;
	tileMap1[11][9] = 13;
	tileMap1[12][9] = 13;
	tileMap1[12][8] = 13;
	drawTileArray(9, 10, 9, 1, 6);
	tileMap1[10][9] = 10;
	*/
}

void drawWordSprite (char input[], int x, int y, float scale) {
size_t n = strlen(input);
u32 offset = 12;
if (scale == 1) offset = 18;
	for (int i = 0; i < n; i++) {
		u32 combOffset = offset * i;
		int indexX = x + combOffset;
		int indexY = y;
		drawFontSprite(parseLetter(input[i]), indexX, indexY, scale);
	}
}

void drawTextbox (char input[], float speed) {
float scale = 0.5;
size_t stringLength = strlen(input);
textboxDelay = 18 * speed;
float f = textboxTimer / textboxDelay;
int charToDisplay = (int)f;
if (charToDisplay >= stringLength) {charToDisplay = stringLength; textboxDone = true;}
u32 offset = 12;
int x = 15;
int y = 170;
drawUISprite(10, 0, 155);
drawUISprite(9, 0, 155);
	for (int i = 0; i < charToDisplay; i++) {
		if (i > 29) {y = 190; x = -345;}
		u32 combOffset = offset * i;
		int indexX = x + combOffset;
		int indexY = y;
		drawFontSprite(parseLetter(input[i]), indexX, indexY, scale);
	}
	if ((textboxDone == false) && (input[charToDisplay] != ' ') && (frames % 5 == 0)) audio_ch10("romfs:/sfx/ui/textbox.bin");
}

void resetTextboxTimer() {
	textboxOpen = true;
	textboxDone = false;
	textboxTimer = 0;
	tempX = 0;
}

int textboxInteract (int type) {
	if (textboxOpen == false) resetTextboxTimer();
	u32 kDown = hidKeysDown();
	if (type == 0) {
		if (kDown & KEY_B || kDown & KEY_START) {
			textboxOpen = false;
			return 0; //move on
		}
		if (kDown & KEY_A) {//not necessarily move on 
			if (textboxDone == true) {textboxOpen = false; return 0;} //move on
			if (textboxDone == false) { //move textbox on
				textboxTimer = 9000;
				textboxDone = true;
				return 1;
			}
		}
	}
	return 1;
}

int parseLetter (char charIn) {
	if (charIn == '0') return 0;
	if (charIn == '1') return 1;
	if (charIn == '2') return 2;
	if (charIn == '3') return 3;
	if (charIn == '4') return 4;
	if (charIn == '5') return 5;
	if (charIn == '6') return 6;
	if (charIn == '7') return 7;
	if (charIn == '8') return 8;
	if (charIn == '9') return 9;
	if (charIn == 'a') return 10;
	if (charIn == 'b') return 11;
	if (charIn == 'c') return 12;
	if (charIn == 'd') return 13;
	if (charIn == 'e') return 14;
	if (charIn == 'f') return 15;
	if (charIn == 'g') return 16;
	if (charIn == 'h') return 17;
	if (charIn == 'i') return 18;
	if (charIn == 'j') return 19;
	if (charIn == 'k') return 20;
	if (charIn == 'l') return 21;
	if (charIn == 'm') return 22;
	if (charIn == 'n') return 23;
	if (charIn == 'o') return 24;
	if (charIn == 'p') return 25;
	if (charIn == 'q') return 26;
	if (charIn == 'r') return 27;
	if (charIn == 's') return 28;
	if (charIn == 't') return 29;
	if (charIn == 'u') return 30;
	if (charIn == 'v') return 31;
	if (charIn == 'w') return 32;
	if (charIn == 'x') return 33;
	if (charIn == 'y') return 34;
	if (charIn == 'z') return 35;
	if (charIn == 'A') return 36;
	if (charIn == 'B') return 37;
	if (charIn == 'C') return 38;
	if (charIn == 'D') return 39;
	if (charIn == 'E') return 40;
	if (charIn == 'F') return 41;
	if (charIn == 'G') return 42;
	if (charIn == 'H') return 43;
	if (charIn == 'I') return 44;
	if (charIn == 'J') return 45;
	if (charIn == 'K') return 46;
	if (charIn == 'L') return 47;
	if (charIn == 'M') return 48;
	if (charIn == 'N') return 49;
	if (charIn == 'O') return 50;
	if (charIn == 'P') return 51;
	if (charIn == 'Q') return 52;
	if (charIn == 'R') return 53;
	if (charIn == 'S') return 54;
	if (charIn == 'T') return 55;
	if (charIn == 'U') return 56;
	if (charIn == 'V') return 57;
	if (charIn == 'W') return 58;
	if (charIn == 'X') return 59;
	if (charIn == 'Y') return 60;
	if (charIn == 'Z') return 61;
	if (charIn == ' ') return 62;
	if (charIn == '.') return 63;
	if (charIn == ':') return 64;
	if (charIn == ';') return 65;
	if (charIn == '!') return 66;
	if (charIn == '?') return 67;
	if (charIn == '-') return 68;
	if (charIn == '"') return 69;
	if (charIn == '$') return 70;
	if (charIn == '<') return 71;
	if (charIn == '>') return 72;
	if (charIn == '(') return 73;
	if (charIn == ')') return 74;
	if (charIn == ',') return 75;
	if (charIn == '`') return 76;
	if (charIn == '[') return 77;
	if (charIn == ']') return 78;
	if (charIn == '+') return 79;
	if (charIn == '%') return 80;
	return 62;
}

void drawBottomScreen() {
	if (sceneType == 0) { //menu
		if (sceneNumber == 0) drawBGSprite(6, 0, 0);
		else if (sceneNumber == 1) drawBGSprite(6, 0, 0);
		else if (sceneNumber == 2) {
			drawBGSprite(7, 0, 0);
			if (menuSelect == 0) { //karlyn
				drawWordSprite("karlyn", 5, 5, 1);
				drawWordSprite("she hits fast but dies", 12, 50, 0.5);
				drawWordSprite("just as fast", 12, 70, 0.5);
			}
		}
		}
	if (sceneType == 1) {
		drawBGSprite(7, 0, 0);
	}
	if (sceneType == 2) {

	}
}

void audio_stop (void) {
	csndExecCmds(true);
	CSND_SetPlayState(0x8, 0);
	memset(buffer, 0, size);
	GSPGPU_FlushDataCache(buffer, size);
	linearFree(buffer);
}

void audio_ch8 (const char *audio) {
	FILE *file = fopen(audio, "rb");
	fseek(file, 0, SEEK_END);
	off_t size = ftell(file);
	fseek(file, 0, SEEK_SET);
	buffer = linearAlloc(size);
	off_t bytesRead = fread(buffer, 1, size, file);
	if (bytesRead == 0) bytesRead = 0;
	fclose(file);
	csndPlaySound(8, SOUND_FORMAT_16BIT | SOUND_REPEAT, 44100, 1, 0, buffer, buffer, size);
	linearFree(buffer);
}

void audio_ch9 (const char *audio) {
	FILE *file = fopen(audio, "rb");
	fseek(file, 0, SEEK_END);
	off_t size = ftell(file);
	fseek(file, 0, SEEK_SET);
	buffer = linearAlloc(size);
	off_t bytesRead = fread(buffer, 1, size, file);
	if (bytesRead == 0) bytesRead = 0;
	fclose(file);
	csndPlaySound(9, SOUND_FORMAT_16BIT | SOUND_REPEAT, 44100, 1, -9, buffer, buffer, size);
	linearFree(buffer);
}

void audio_ch10 (const char *audio) {
	FILE *file = fopen(audio, "rb");
	fseek(file, 0, SEEK_END);
	off_t size = ftell(file);
	fseek(file, 0, SEEK_SET);
	buffer = linearAlloc(size);
	off_t bytesRead = fread(buffer, 1, size, file);
	if (bytesRead == 0) bytesRead = 0;
	fclose(file);
	csndPlaySound(10, SOUND_FORMAT_16BIT, 44100, 2, 0, buffer, buffer, size);
	linearFree(buffer);
}

void audio_ch11 (const char *audio) {
	FILE *file = fopen(audio, "rb");
	fseek(file, 0, SEEK_END);
	off_t size = ftell(file);
	fseek(file, 0, SEEK_SET);
	buffer = linearAlloc(size);
	off_t bytesRead = fread(buffer, 1, size, file);
	if (bytesRead == 0) bytesRead = 0;
	fclose(file);
	csndPlaySound(10, SOUND_FORMAT_16BIT, 44100, 2, 0, buffer, buffer, size);
	linearFree(buffer);
}

void loadFiles() {
	FILE *myFile;
	myFile = fopen("romfs:/bins/test.cara", "r");
	for (int i = 0; i < 16; i++){
		fscanf(myFile, "%d,", &testArray[i] );
    }
	fclose(myFile);
}

void drawScene() {
	if (battleType == 0) { //offline
		drawTiles(cameraX, cameraY);
		drawMainSprite(0, playerX - cameraX, playerY - cameraY);
	}
}