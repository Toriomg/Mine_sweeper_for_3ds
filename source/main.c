#include <citro2d.h>
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define SCREEN_WIDTH_TOP  400
#define SCREEN_WIDTH_BOTTOM  320
#define SCREEN_HEIGHT 240
#define MINE_NUMBER 90
#define COLS 24 //j
#define ROWS 24 //i
#define SPRITE_SHIFT 15

#define NUMBER_WIDTH 40.0f
#define SIZE_MARKER 120.0f
//E:\3ds_c\ms3ds

typedef struct{
	C2D_Sprite spr;
	float x, y; // position
} Sprite;

typedef struct{
	int value;// -1 is mine, otherwise, number neighbor mines
	int swept; //0 when not, 1 swept, 2 for the flag
	//Sprite cell_sprite; not sure if correct, i will not use it until it is needed
} Cell;

typedef struct {
		int x;
		int y;
	} Coords;

//Sprites
static C2D_SpriteSheet spriteSheet;
static Sprite sprites[ROWS][COLS];
static Sprite timer;
static Sprite marker;
static Sprite victory_sprite;
static Sprite defeat_sprite;
//coords
static Cell board[ROWS][COLS];
static Coords square_cursor_coords;
//game rules
static int mines_left;
static char game_over;
static char game_won;
//time
static unsigned int frame_counter = 0;  // Global or static variable to track frames
static int minutes;
static int seconds;



//----------------------------------------------------------------------------------------------------------------------------------
//init functions
//----------------------------------------------------------------------------------------------------------------------------------
void init_global_var(){
	mines_left = MINE_NUMBER;
	game_over = 0;
	game_won = 0;

	minutes = 0;
	seconds = 0;
}

void mergemines()
{
    // Create a flat list of all cells in the board
    size_t total_cells = ROWS * COLS;
    int flat_board[total_cells];
    
    // Initialize all cells as non-mine (value 0)
    for (size_t i = 0; i < total_cells; i++)
        flat_board[i] = 0;

    // Set the first MINE_NUMBER cells as mines (-1)
    for (size_t i = 0; i < MINE_NUMBER; i++)
        flat_board[i] = -1;

    // Shuffle the flat_board array using Fisher-Yates algorithm
    for (size_t i = total_cells - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        // Swap flat_board[i] and flat_board[j]
        int temp = flat_board[i];
        flat_board[i] = flat_board[j];
        flat_board[j] = temp;
    }

    // Map the shuffled flat_board back to the 2D board
    size_t index = 0;
    for (size_t i = 0; i < ROWS; i++) {
        for (size_t j = 0; j < COLS; j++) {
            board[i][j].value = flat_board[index];
            board[i][j].swept = 0;  // Initialize swept status
            index++;
        }
    }
}

void init_sprites_board() {
	/*indez 0 flag, 1-9 numbermines, 10 mine, 11 victory, 12 defeat*/
    for (size_t i = 0; i < ROWS; i++) {
        for (size_t j = 0; j < COLS; j++) {
            Sprite* sprite = &sprites[i][j];
            
            // Set sprite from the sprite sheet with an initial sprite (e.g., empty or covered cell)
            C2D_SpriteFromSheet(&sprite->spr, spriteSheet, 9);  // Use the empty cell sprite initially (index 9)
            C2D_SpriteSetCenter(&sprite->spr, 0.0f, 0.0f); // Set top-left corner as the center of the sprite
            // Position sprites based on i and j
			float posX = 40.0f + i * 10.0f; // x position
			float posY = j * 10.0f;         // y position
			
			// Store positions in the sprite struct
            sprite->x = posX;
            sprite->y = posY;

			//printf("\x1b[8;1HSprite[%zu][%zu] init at pos: (%f, %f)\n", i, j, posX, posY);
			C2D_SpriteSetPos(&sprite->spr, posX, posY);  // Set sprite position
        }
    }
	
}

void init_sprites_all(){
	init_sprites_board();
	C2D_SpriteFromSheet(&victory_sprite.spr, spriteSheet, 11);//As fulfill the screen no coord is touched
	C2D_SpriteFromSheet(&defeat_sprite.spr, spriteSheet, 12);
	//top HUD
	C2D_SpriteFromSheet(&marker.spr, spriteSheet, 13);
	C2D_SpriteFromSheet(&timer.spr, spriteSheet, 14);
}

void init_square_coords(){
	square_cursor_coords.x = 40;
	square_cursor_coords.y = 0;
}

int count_next_mines(){	
	int mine_counts[ROWS][COLS] = {0};
	for (size_t i = 0; i < ROWS; i++)
	{
		for (size_t j = 0; j < COLS; j++)
		{
			if (board[i][j].value == -1) { // If there's a mine at (i, j), skip it
                continue;
            }
			int mine_count = 0;

			for (int x = -1; x <= 1; x++) {
    			for (int y = -1; y <= 1; y++) {
        			// Evitar la posición del elemento central (row, col)
        			if (x == 0 && y == 0)
						continue;

					int nei_row = i + x;//access to the neighbours
        			int nei_col = j + y;
					if (nei_row >= 0 && nei_row < ROWS && nei_col >= 0 && nei_col < COLS) {
						if (board[nei_row][nei_col].value == -1) { // Count the neighboring mines
							mine_count++;
						}
					}
            	}
			}
			// Store the mine count for the current Cell
			mine_counts[i][j] = mine_count;
			
		}
	}
	for (size_t i = 0; i < ROWS; i++) {
        for (size_t j = 0; j < COLS; j++) {
            if (board[i][j].value != -1) { // Don't overwrite mines to be sure no overwriting is being done.
                board[i][j].value = mine_counts[i][j];
            }
        }
    }
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------
//Normal functions
//----------------------------------------------------------------------------------------------------------------------------------


int sweep_land(int x_sec,int y_sec){
	if(board[x_sec][y_sec].swept != 0)//End recursion
		return 0;
	board[x_sec][y_sec].swept = 1;// Mark current cell as swept
	if(board[x_sec][y_sec].value != 0)// Stop recursion if the current cell is non-zero (it has a value)
		return 0;
	for (int x = -1; x <= 1; x++) {
    	for (int y = -1; y <= 1; y++) {
        	// Evitar la posición del elemento central (row, col)
        	if(x == 0 && y == 0)
				continue;
			int new_x = x_sec + x;
			int new_y = y_sec + y;
			if (new_x >= 0 && new_x < ROWS && new_y >= 0 && new_y < COLS) {
				if(board[new_x][new_y].swept == 0)
					sweep_land(new_x, new_y); //Recursively calls the func
			}	
		}
	}
	return 0;
}

void place_flag(int x_sec, int y_sec){
	if(board[x_sec][y_sec].swept == 0){
		board[x_sec][y_sec].swept = 2;
		mines_left--;
	}else if(board[x_sec][y_sec].swept == 2){
		board[x_sec][y_sec].swept = 0;
		mines_left++;
	}
}

void explote_mines(){
	int i,j;
	for(i = 0; i < ROWS; i++){
		for(j = 0; j < COLS; j++){
			if(board[i][j].value == -1 && board[i][j].swept != 2){
				board[i][j].swept = 1;
			}
		}
	}
	game_over = 1;
}

void modify_land(int x_sector, int y_sector, char flag_mode){
	printf("\x1b[25;8Hx: %d | y: %d", x_sector, y_sector);
	if(flag_mode){
		place_flag(x_sector, y_sector);
	}else if(board[x_sector][y_sector].swept == 0){
		if(board[x_sector][y_sector].value == -1){
			explote_mines();
		} else {
			sweep_land(x_sector, y_sector);	
		}
	}
}

void touch_land(touchPosition* touch, char flag_mode){
	if(40 > touch->px || touch->px > 280){ // Out of bounds of the board
		printf("\x1b[1;1HOut of bounds");
		init_square_coords();
		return;
	}
	int x_sector = (touch->px - 40) / 10;//10 size of the mine
	int y_sector = touch->py / 10; // Made to find the coords
	
	//printf("\x1b[25;8Hx: %d | y: %d", x_sector, y_sector);
	modify_land(x_sector, y_sector, flag_mode);
	square_cursor_coords.x = touch->px / 10 * 10;
	square_cursor_coords.y = touch->py / 10 * 10;
}

char check_win(){
	for (size_t i = 0; i < ROWS; i++){
		for (size_t j = 0; j < COLS; j++){
			if (board[i][j].swept != 1 && board[i][j].value != -1){ //If there's one non-mine cell not sweeped, game not won
				return 0;
			}
		}
	}
	return 1;
}

void move_square_cursor(){
	u32 kDown_repeat = hidKeysDownRepeat();
	int size_square = 10;
	int limit_right = 240 + 40 - size_square;
	int limit_left = 40;
	if (kDown_repeat & KEY_UP && square_cursor_coords.y > 0){
		square_cursor_coords.y -= size_square;
	} else if (kDown_repeat & KEY_DOWN && square_cursor_coords.y < SCREEN_HEIGHT - size_square) {
		square_cursor_coords.y += size_square;
	} else if (kDown_repeat & KEY_LEFT && square_cursor_coords.x > limit_left) {
		square_cursor_coords.x -= size_square;
	} else if (kDown_repeat & KEY_RIGHT && square_cursor_coords.x < limit_right) {
		square_cursor_coords.x += size_square;
	}
}

void restart(char flag_mode, char touch_hold, touchPosition touch, time_t start_time){
	start_time = time(NULL); //set initial time
	init_global_var();
	mergemines();
	count_next_mines();
	init_square_coords();
	init_sprites_board();
	flag_mode = 0;
	touch_hold = 0;
	touch.px = 0;
	touch.py = 0;
}

void time_counter(time_t start_time, time_t current_time){
	current_time = time(NULL); // actual time
    double elapsed = difftime(current_time, start_time); 
	//Convert time to minutes and seconds
	minutes = (int)elapsed / 60;
    seconds = (int)elapsed % 60;
}

//----------------------------------------------------------------------------------------------------------------------------------
//drawing functions
//----------------------------------------------------------------------------------------------------------------------------------


void draw_square(float x, float y, float w, u32 color){
	C2D_DrawLine(x, y, color, x, y + w, color, 2, 1);
	C2D_DrawLine(x, y, color, x + w, y, color, 2, 1);
	C2D_DrawLine(x, y + w, color, x + w, y + w, color, 2, 1);
	C2D_DrawLine(x + w, y, color, x + w, y + w, color, 2, 1);
}

void draw_square_cursor(float x, float y, char flag_mode){
	if(40 > x || x > 280)
		return;
	u32 blue_square = C2D_Color32(0xAD, 0xD8, 0xE6, 0xFF);
	u32 red_square = C2D_Color32(0xFF, 0x70, 0x74, 0xFF);
	if(flag_mode){
		draw_square(x, y, 10, red_square);
	}else{
		draw_square(x, y, 10, blue_square);
	}
}

void draw_counter_number_pair(int num_pair, float x_point, float y_point){
	Sprite num_pair_sprs[2]; //number pair sprites
	char fst_digit = num_pair % 10;
	char secnd_digit = num_pair % 100 / 10;
		
	C2D_SpriteFromSheet(&num_pair_sprs[0].spr, spriteSheet, secnd_digit + SPRITE_SHIFT);
	C2D_SpriteFromSheet(&num_pair_sprs[1].spr, spriteSheet, fst_digit + SPRITE_SHIFT);
	C2D_SpriteSetPos(&num_pair_sprs[0].spr, x_point, y_point);
	C2D_SpriteSetPos(&num_pair_sprs[1].spr, x_point + NUMBER_WIDTH, y_point);
	for(size_t i = 0; i < 2; i++){
		C2D_DrawSprite(&num_pair_sprs[i].spr);
	}
}

void draw_top_squares(char sense){
	/*1 makes it move down-right, 0 makes it move up-left*/
	u32 COLOR_SQUARES_BACKGROUND_TOP = C2D_Color32(0x43, 0x66, 0x3a, 0x88);
	int x, y;
	int square_size = 30;
	int time_div = 3;
	int offset = (frame_counter % (time_div * square_size)) / time_div;
	for (int i = -6*square_size; i < SCREEN_WIDTH_TOP + 6*square_size; i += square_size) { //Little squares
		for (int j = -6*square_size; j < SCREEN_HEIGHT + 6*square_size; j += square_size) {
			if((i % (2*square_size) == 0 && j % (2*square_size) == 0) || (i % (2*square_size) != 0 && j % (2*square_size) != 0)){
				if(sense){
					x = i + offset;
					y = j + offset; 
				} else {
					x = i - offset + square_size/2;
					y = j - offset - square_size/2; 
				}
				C2D_DrawRectSolid(x, y, 0.0f, (float)square_size, (float)square_size, COLOR_SQUARES_BACKGROUND_TOP);
			}
		}
	}
}

void draw_board_bottom(u32 SAND_GREEN, u32 SAND_GREEN_DARK){
	draw_top_squares(0);
	C2D_DrawRectSolid(40.0f, 0.0f, 0.0f, 240.0f, 240.0f, SAND_GREEN); //Cuadrado del tablero
	//Cuadricula del buscaminas. 10 es le tamaño de la cuadricula y son 240x240, 24 de lado
	for (size_t i = 0; i < ROWS; i++) {
		for (size_t j = 0; j < COLS; j++) {
			Sprite* sprite = &sprites[i][j];
			size_t sprite_index;
			if(board[i][j].swept != 1){
				// Chess pattern board
				if((i % 2 == 0 && j % 2 == 0) || (i % 2 != 0 && j % 2 != 0)){
					C2D_DrawRectSolid(sprite->x, sprite->y, 0.0f, 10.0f, 10.0f, SAND_GREEN_DARK);
				}else{
					C2D_DrawRectSolid(sprite->x, sprite->y, 0.0f, 10.0f, 10.0f, SAND_GREEN);
				}
			}
			if(board[i][j].swept != 0){
				// Choose the correct sprite index based on the board's value
				if (board[i][j].swept == 2){
					sprite_index = 0;  // Index for flag sprite
				}else if(board[i][j].value == -1){ 
					sprite_index = 10;  // Index for mine sprite
				} else if (board[i][j].value == 0) {
					sprite_index = 9;  // Index for empty cell sprite
				} else {
					sprite_index = board[i][j].value;  // Use the number of neighboring mines
				}
				// Update sprite's texture based on the calculated sprite_index
				C2D_SpriteFromSheet(&sprite->spr, spriteSheet, sprite_index);
				C2D_SpriteSetPos(&sprite->spr, sprite->x, sprite->y);
				C2D_DrawSprite(&sprite->spr);// Draw the sprite
				}
			}
		}
}

void draw_top() {
	//background of the screen
	draw_top_squares(0);
	C2D_SpriteSetPos(&timer.spr, 0.0f, SCREEN_HEIGHT / 3);
	C2D_DrawSprite(&timer.spr);
	C2D_SpriteSetPos(&marker.spr, SCREEN_WIDTH_TOP - SIZE_MARKER, SCREEN_HEIGHT / 3);
	C2D_DrawSprite(&marker.spr);
	//Counters
	int draw_mines_left;
	if (mines_left >= 0) { //To make sure to show a non-negative number
		draw_mines_left = mines_left;
	} else {
		draw_mines_left = 0;
	}
	draw_counter_number_pair(draw_mines_left, NUMBER_WIDTH * 8, SCREEN_HEIGHT / 3);
	draw_counter_number_pair(minutes, NUMBER_WIDTH * 1, SCREEN_HEIGHT / 3);
	draw_counter_number_pair(seconds, NUMBER_WIDTH * 4, SCREEN_HEIGHT / 3);
}

//----------------------------------------------------------------------------------------------------------------------------------
//MAIN
//----------------------------------------------------------------------------------------------------------------------------------

int main(int argc, char** argv)
{
	srand(time(NULL)); // initialize seed
	romfsInit();
	gfxInitDefault();
	// Initialize Citro2D
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	// no consol consoleInit(GFX_TOP, NULL);
	// Create screens
	C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
		
	// Load graphics
	spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
	if (!spriteSheet) svcBreak(USERBREAK_PANIC);

	char flag_mode = 0;
	time_t start_time, current_time; // initial time and final time
	start_time = time(NULL); //set initial time
	// colors
	u32 SAND_GREEN = C2D_Color32(0xA0, 0xBC, 0xAC, 0xFF);
	u32 SAND_GREEN_DARK = C2D_Color32(0x80, 0xa4, 0x83, 0xFF);
	u32 COLOR_BACKGROUND_BOTTOM = C2D_Color32(0x1a, 0x2e, 0x1a, 0xFF);
	//u32 COLOR_BACKGROUND_TOP = C2D_Color32(0x8b, 0xBD, 0x7a, 0xFF); //lightier one
	u32 COLOR_BACKGROUND_TOP = C2D_Color32(0x1a, 0x2e, 0x1a, 0xFF);
	// touch cursor
	touchPosition touch = {0,0};
	char touch_hold = 0; //To avoid two continued sweeps;

	init_global_var();
	mergemines();
	count_next_mines();
	init_sprites_all();
	init_square_coords();
	// Main loop
	while (aptMainLoop())
	{
		hidScanInput();
		u32 kDown = hidKeysDown();
		if (game_won == 1 || game_over == 1) { //END GAME
			if (kDown & (KEY_A | KEY_B | KEY_Y | KEY_X))
				restart(flag_mode, touch_hold, touch, start_time);
		} else { //GAME
			if (kDown & (KEY_X | KEY_Y | KEY_L | KEY_R)) //Set flag mode
				flag_mode = !flag_mode;
			move_square_cursor();
			if (kDown & KEY_A) { //With a sweeping
				modify_land((square_cursor_coords.x - 40) / 10, square_cursor_coords.y / 10, flag_mode);
			} else if (hidKeysHeld() & KEY_TOUCH) { //Touch sweeping
				if(!touch_hold){
					hidTouchRead(&touch);
					touch_hold = 1;
					touch_land(&touch, flag_mode);
				}
			} else if(touch_hold)
				touch_hold = 0;
			if (check_win())
				game_won = 1;
		}

		frame_counter++;
		time_counter(start_time, current_time);

		// Start rendering on top screen
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(top, COLOR_BACKGROUND_TOP);
		C2D_SceneBegin(top);
		if (game_won){
			C2D_DrawSprite(&victory_sprite.spr);
		} else if (game_over){
			C2D_DrawSprite(&defeat_sprite.spr);
		} else {
			draw_top();
		}

		// Start rendering on bottom screen
		C2D_TargetClear(bottom, COLOR_BACKGROUND_BOTTOM); //Fondo de la escena con color
		C2D_SceneBegin(bottom);
		draw_board_bottom(SAND_GREEN, SAND_GREEN_DARK);
		draw_square_cursor(square_cursor_coords.x, square_cursor_coords.y, flag_mode);// Rounding to 0 
		

		C3D_FrameEnd(0);//End rendering
	}
	// Delete graphics
	C2D_SpriteSheetFree(spriteSheet);
	C2D_Fini();
	C3D_Fini(); //Ns si es necesario
	gfxExit();
	romfsExit();
	return 0;
}
