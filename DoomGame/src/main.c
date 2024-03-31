#include <raylib.h>
#include <raymath.h>
#include <stdint.h>
#include <assert.h>
#define EASE_IMPLEMENTATION
#include "ease.h"
#define DEGREE_TO_RADIAN(d) ((d / 360.0f)*2*PI)
#define DEFAULT_FOV DEGREE_TO_RADIAN(30.0f)
#define DEGREE_1 DEGREE_TO_RADIAN(1.0f)
#define U32_TO_COLOR(col) CLITERAL(Color) {.a=col & 0xff, .b=(col >> 8) & 0xff, .g=(col >> 16) & 0xff, .r=(col >> 24) & 0xff}
#define SCALE 60
#define W_RATIO 16
#define H_RATIO 12
#define WIDTH (SCALE*W_RATIO)
#define HEIGHT (SCALE*H_RATIO)
#define TITLE "Doom Game"
#define PLAYER_WIDTH_2D 20
#define PLAYER_HEIGHT_2D 20
#define COLISION_BEFORE_WALL 20
#define DOF 8
#define DEFAULT_HORIZON (DOF * TILE_WIDTH)
#define DEFAULT_BRIGHTNESS 0.5f
#define BRIGHTNESS_RANGE 300.0f

#define DEFAULT_MOVE_SPEED 70
#define DEFAULT_SPRINT_SPEED (DEFAULT_MOVE_SPEED+60)
#define CUBE_SIZE 64



//#define RAYS (60*72) // 4k
//#define RAYS (60*36) // 2160 rays
#define RAYS (60*18) // 1028 rays
//#define RAYS (60*12) // 720 rays
//#define RAYS (60*8)  // 480 rays
//#define RAYS (60*6)  // 360 rays
//#define RAYS (60*1)  // 60 rays
//#define RAYS (60/2)  // 30 rays
//#define RAYS (60/4)  // 15 rays
#define BACKGROUND U32_TO_COLOR(0x212121ff)
#define TILE_WIDTH 64
#define TILE_HEIGHT TILE_WIDTH

typedef struct {
	easeFunction func;
	const char* name;
} EaseFunction;
#define X(f) {.name=#f, .func=f}
EaseFunction easeFunctions[] = {
	X(easeLinear),
	X(easeInCubic),
	X(easeOutSine),
	X(easeInOutQuad),
};

#define EASE_FUNCTION_COUNT (sizeof(easeFunctions) / sizeof(*easeFunctions))


#define PI2 (PI/2)
#define PI3 ((PI*3)/2)

#define _min(a,b) a > b ? b : a
float minf(float a, float b) {
	return _min(a, b);
}
typedef struct {
	uint64_t x;
	uint64_t y;
} Vector2u64;
typedef struct {
	size_t x;
	size_t y;
} Vector2us;
typedef struct {
	int8_t x;
	int8_t y;
} Vector2i8;
typedef Vector2 InputMap;


static Vector2 vu64_as_vf32(Vector2u64 v) {
	return CLITERAL(Vector2) { .x = v.x, .y = v.y };
}
typedef struct {
	Vector2 pos;
	

	float rot;
	// For optimization
	float cos_rot, sin_rot;
	float dx, dy;
	float hcos_rot, hsin_rot;
	float dxh, dyh;

	float FOV;
	float moveSpeed;
	float distantHorizon;
	float brightness;
} Player;


size_t mapWidth=8, mapHeight=8;
uint8_t mapW[] = {
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 0, 0, 1, 0, 0, 0, 1,
	1, 0, 0, 1, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 1, 0, 1,
	1, 0, 0, 0, 0, 1, 0, 1,
	1, 0, 0, 0, 0, 1, 0, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
};
void drawMap2D(Rectangle boundary) {
	//float tileWidth=0;
	//float tileHeight = 0;
	//if (boundary.width < boundary.height) {
	//	boundary.height = boundary.width;
	//	tileWidth = boundary.width / mapWidth;
	//	tileHeight = tileWidth;
	//}
	//else {
	//	boundary.width = boundary.height;
	//	tileHeight = boundary.height / mapHeight;
	//	tileWidth = tileHeight;
	//}
	assert(boundary.width / mapWidth >= TILE_WIDTH);
	assert(boundary.height / mapHeight >= TILE_HEIGHT);
	//TraceLog(LOG_INFO, "tileWidth: %f, tileHeight: %f", tileWidth, tileHeight);

	for (size_t y = 0; y < mapHeight; ++y) {
		for (size_t x = 0; x < mapWidth; ++x) {
			uint8_t tile = mapW[y * mapWidth + x];
			Rectangle tileRect = { 
				.x = boundary.x + TILE_WIDTH * x,
				.y = boundary.y + TILE_HEIGHT * y,
				.width = TILE_WIDTH,
				.height = TILE_HEIGHT
			};
			switch (tile) {
			case 0:
				DrawRectangleRec(tileRect, BLACK);
				break;
			case 1:
				DrawRectangleRec(tileRect, RAYWHITE);
				break;
			default:
				TraceLog(LOG_FATAL, "Unknown tile: %d", (int)tile);
				assert(false && "unreachable");
			}
		}
	}
}
void drawRays3D(Player player, Rectangle map3d, bool Enabled2d, easeFunction f) {
	float angle = player.rot - player.FOV;
	float angleDelta = (player.FOV * 2) / RAYS;
	float distance = 0.0f;
	float rayWidth = map3d.width / RAYS;
	if (angle < 0) {
		angle += 2 * PI;
	} 
	if (angle > 2 * PI) {
		angle -= 2 * PI;
	}
	Vector2 offset = { 0,0 };
	for (size_t i = 0; i < RAYS; ++i) {
		float distanceH;
		float distanceV;
		bool hasHitH;
		bool hasHitV;
		Vector2 Hpos={0};
		Vector2 Vpos={0};

		size_t dof = 0;
		float aTan;
		dof = 0;
		aTan = -1 / tanf(angle);
		Vector2 rayPos = {0};	
		
		int64_t mapX;
		int64_t mapY;
		int64_t mapI;
		
		if (angle > PI) {
			rayPos.y = (float)(((int64_t)player.pos.y / 64) * 64) - 0.0001f;
			rayPos.x = (float)((player.pos.y - rayPos.y) * aTan) + player.pos.x;
			offset.y = -64;
			offset.x = -offset.y * aTan;
		}
		else if (angle < PI) {
			rayPos.y = (float)(((int64_t)player.pos.y / 64) * 64) + 64;
			rayPos.x = (float)((player.pos.y - rayPos.y) * aTan) + player.pos.x;
			offset.y = 64;
			offset.x = -offset.y * aTan;
		}
		//if (angle == 0 || angle == PI) {
		//	rayPos = player.pos;
		//	dof = 8;
		//}
		while (dof < DOF) {
			//DrawCircleV(rayPos, 5, ORANGE);
			mapX = (int64_t)rayPos.x / 64;
			mapY = (int64_t)rayPos.y / 64;
			mapI = mapY * mapWidth + mapX;
			if (mapI >= 0 && mapI < mapWidth * mapHeight && mapW[mapI] == 1) break;
			rayPos.x += offset.x;
			rayPos.y += offset.y;
			dof++;
		}
		
		distanceH = Vector2Distance(player.pos, rayPos);
		hasHitH = dof < 8;
		Hpos = rayPos;
		//DrawLineV(player.pos, rayPos, GREEN);



		dof = 0;
		aTan = -tanf(angle);
		rayPos = player.pos;

		
		if (angle > PI2 && angle < PI3) {
			rayPos.x = (float)(((int64_t)player.pos.x / 64) * 64) - 0.0001f;
			rayPos.y = (float)((player.pos.x - rayPos.x) * aTan) + player.pos.y;
			offset.x = -64;
			offset.y = -offset.x * aTan;
		}
		else if (angle < PI2 || angle > PI3) {
			rayPos.x = (float)(((int64_t)player.pos.x / 64) * 64) + 64;
			rayPos.y = (float)((player.pos.x - rayPos.x) * aTan) + player.pos.y;
			offset.x = 64;
			offset.y = -offset.x * aTan;
		}
		//if (angle == 0 || angle == PI) {
		//	rayPos = player.pos;
		//	dof = 8;
		//}
		while (dof < 8) {
			//DrawCircleV(rayPos, 5, BLUE);
			mapX = (int64_t)rayPos.x / 64;
			mapY = (int64_t)rayPos.y / 64;
			mapI = mapY * mapWidth + mapX;
			//if (mapI < 0 || mapI > mapWidth * mapHeight) { dof = 8; break; }
			if (mapI >= 0 && mapI < mapWidth * mapHeight && mapW[mapI] == 1) break;
			rayPos.x += offset.x;
			rayPos.y += offset.y;
			dof++;
		}
		Vpos = rayPos;
		hasHitV = dof < 8;
		distanceV = Vector2Distance(player.pos, rayPos);
		//DrawLineV(player.pos, rayPos, RED);
		Color color = RED;
		if (distanceH < distanceV) {
			rayPos = Hpos;
			distance = distanceH;
			//color = ColorAlpha(color, 0.9f);
		}
		else {
			rayPos = Vpos;
			distance = distanceV;
			//color = ColorAlpha(color, 0.7f);
		}

		bool hasHit = (hasHitH || hasHitV) && distance < player.distantHorizon;


		float diff = player.rot - angle;
		if (diff < 0) {
			diff += 2 * PI;
		}
		else if (diff > 2 * PI){
			diff -= 2 * PI;
		}
		distance *= cosf(diff);
		if(Enabled2d) DrawLineV(player.pos, rayPos, GREEN);
		if (hasHit) {
			angle += angleDelta;
			if (angle < 0) {
				angle += 2 * PI;
			}
			if (angle > 2 * PI) {
				angle -= 2 * PI;
			}
			float wallHeight = CUBE_SIZE * map3d.height / distance;
			
			if (wallHeight > map3d.height) wallHeight = map3d.height;
			float off = 160 - wallHeight / 2;//wallHeight;
			
			float prog = minf(Clamp(distance - (player.brightness * BRIGHTNESS_RANGE), 0.0, player.distantHorizon) / player.distantHorizon, 1.0f);
			float update = f(prog);
			float result = 1.0-(update);
			
			color = ColorAlpha(color, result);
			Rectangle wall3d = {
				.x = i * rayWidth + map3d.x,
				.y = map3d.y + off,
				.height = wallHeight,
				.width = rayWidth
			};

			DrawRectangleRec(wall3d, color);
			
		}
/*
		DrawLineEx(
			CLITERAL(Vector2) {
			 .x=i * 8 + map3d.x,
			 .y=map3d.y+off 
		    },
			CLITERAL(Vector2) {
			 .x=i * 8 + map3d.x,
			 .y=map3d.y+off+wallHeight 
		    }, 8, RED);
*/
	}
}
void drawPlayer(Player player) {

	Rectangle finRect = {
		.x=player.pos.x,
		.y=player.pos.y,
		.width=PLAYER_WIDTH_2D,
		.height=PLAYER_HEIGHT_2D
	};
	DrawRectangleV(CLITERAL(Vector2){.x=finRect.x, .y=finRect.y}, CLITERAL(Vector2){20, 20}, YELLOW);
	
}

InputMap GetKeyboardInputVec() {
	return CLITERAL(Vector2) {
		(int)IsKeyDown(KEY_W) - (int)IsKeyDown(KEY_S),
		(int)IsKeyDown(KEY_D) - (int)IsKeyDown(KEY_A)
	};
}
typedef enum {
	INPUT_DEVICE_KEYBOARD,
	INPUT_DEVICE_COUNT
} InputDevice;
InputMap input_map[INPUT_DEVICE_COUNT] = { 0 };
static void UpdateInputMap() {
	input_map[0] = GetKeyboardInputVec();
}

InputDevice selectedInputDevice = INPUT_DEVICE_KEYBOARD;

static InputMap GetInputVec() {
	return input_map[selectedInputDevice];
}
int main(void) {
	//InputMap maps[] = { GetKeyboardInputVec() };
	//TraceLog(LOG_INFO, "maps: %zu",sizeof(maps)/sizeof(*maps));
	//return 1;
	//TraceLog(LOG_INFO, "DEGREE_1: %f",DEGREE_1);
	//return 1;
	InitWindow(WIDTH, HEIGHT, TITLE);
	size_t easeF_index = 0;
	Player player = {0};
	player.FOV = DEFAULT_FOV;
	player.pos = CLITERAL(Vector2) { TILE_WIDTH * 4, TILE_HEIGHT * 4 };	
	player.rot = 0;
	player.moveSpeed = DEFAULT_MOVE_SPEED;
	player.distantHorizon = DEFAULT_HORIZON;
	player.brightness = DEFAULT_BRIGHTNESS;
	SetTargetFPS(60);
	bool debugInfoEnabled = false;
	HideCursor();
	while (!WindowShouldClose()) {
		UpdateInputMap();
		if (IsKeyDown(KEY_LEFT_SHIFT)) {
			player.moveSpeed = DEFAULT_SPRINT_SPEED;
		}
		else {
			player.moveSpeed = DEFAULT_MOVE_SPEED;
		}
		float dt = GetFrameTime();
		float w = GetScreenWidth();
		float h = GetScreenHeight();
		Vector2 delta = GetMouseDelta();
		// TODO: Add velocity based movement
		//if (IsKeyDown(KEY_A)) player.rot -= 1.0f * dt;
		//if (IsKeyDown(KEY_D)) player.rot += 1.0f * dt;
		if(IsWindowFocused()) SetMousePosition(w/2, h/2);
		//SetMouseCursor(MOUSE_CURSOR_)
		player.rot += delta.x * dt * 0.1f;
		if (player.rot > 2 * PI) player.rot -= 2 * PI;
		if (player.rot < 0) player.rot += 2 * PI;


		
		player.sin_rot = sinf(player.rot);
		player.cos_rot = cosf(player.rot);
		player.hsin_rot = sinf(player.rot + PI2);
		player.hcos_rot = cosf(player.rot + PI2);
		player.dx  = player.cos_rot  * player.moveSpeed;
		player.dy  = player.sin_rot  * player.moveSpeed;
		player.dxh = player.hcos_rot * player.moveSpeed;
		player.dyh = player.hsin_rot * player.moveSpeed;





		float xo  = player.dx < 0 ? -COLISION_BEFORE_WALL : COLISION_BEFORE_WALL;
		float yo  = player.dy < 0 ? -COLISION_BEFORE_WALL : COLISION_BEFORE_WALL;
		float xoh = player.dxh < 0 ? -COLISION_BEFORE_WALL : COLISION_BEFORE_WALL;
		float yoh = player.dyh < 0 ? -COLISION_BEFORE_WALL : COLISION_BEFORE_WALL;
		Vector2 input = GetInputVec();
		int64_t px = (int64_t)player.pos.x / TILE_WIDTH;
		int64_t py = (int64_t)player.pos.y / TILE_HEIGHT;
		//TraceLog(LOG_INFO, "Input: {%f, %f}", input.x, input.y);
		{
			Vector2 pn = player.pos;
			
			pn.x += (player.dx * dt) * input.x;
			pn.y += (player.dy * dt) * input.x;

			Vector2 cpn = pn;
			cpn.x += input.x > 0 ? xo : -xo;
			cpn.y += input.x > 0 ? yo : -yo;

			int64_t cpnx = (int64_t)cpn.x / TILE_WIDTH;
			int64_t cpny = (int64_t)cpn.y / TILE_HEIGHT;
			if (
				cpnx >= 0 && cpnx < mapWidth &&
				mapW[py * mapWidth + cpnx] == 0
				) player.pos.x = pn.x;

			if (
				cpny >= 0 && cpny < mapHeight &&
				mapW[cpny * mapHeight + px] == 0
				) player.pos.y = pn.y;
		}
		{
			Vector2 pn = player.pos;
			pn.x += (player.dxh * dt) * input.y;
			pn.y += (player.dyh * dt) * input.y;

			Vector2 cpn = pn;
			
			cpn.x += input.y > 0 ? xoh : -xoh;
			cpn.y += input.y > 0 ? yoh : -yoh;

			int64_t cpnx = (int64_t)cpn.x / TILE_WIDTH;
			int64_t cpny = (int64_t)cpn.y / TILE_HEIGHT;

			if (
				cpnx >= 0 && cpnx < mapWidth &&
				mapW[py * mapWidth + cpnx] == 0
				) player.pos.x = pn.x;

			if (
				cpny >= 0 && cpny < mapHeight &&
				mapW[cpny * mapHeight + px] == 0
				) player.pos.y = pn.y;
		}

		px = (int64_t)player.pos.x / TILE_WIDTH;
		py = (int64_t)player.pos.y / TILE_HEIGHT;
		if (px < 0 || px > mapWidth || py < 0 || py > mapHeight) {
			player.pos.x = TILE_WIDTH * mapWidth / 2;
			player.pos.y = TILE_HEIGHT * mapHeight / 2;
		}
		else if (mapW[py * mapWidth + px]) {
			player.pos.x = TILE_WIDTH * mapWidth / 2;
			player.pos.y = TILE_HEIGHT * mapHeight / 2;
		}
		

		if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
			player.brightness += GetMouseWheelMove() * dt;
			player.brightness = Clamp(player.brightness, 0, 1.0f);
		}
		if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
			if (IsKeyPressed(KEY_UP)) {
				easeF_index = (easeF_index + 1) % EASE_FUNCTION_COUNT;
			}
			if (IsKeyPressed(KEY_DOWN)) {
				easeF_index = easeF_index > 0 ? easeF_index-1 : EASE_FUNCTION_COUNT-1;
			}
		}
		
		if (IsKeyPressed(KEY_F2)) {
			TakeScreenshot(TextFormat("%dr.png",RAYS));
		}
		if (IsKeyPressed(KEY_F3)) {
			debugInfoEnabled = !debugInfoEnabled;
		}
		Rectangle screen = { .x = 0, .y = 0, .width=GetScreenWidth(), .height=GetScreenHeight()};
		Rectangle map2d = { .x = 0, .y = 0, .width = TILE_WIDTH * mapWidth, .height = TILE_HEIGHT * mapHeight};
		Rectangle map3d = { .x = map2d.x + map2d.width, .y = 0, .width=screen.width-map2d.width, .height=screen.height};
		
		BeginDrawing();
			ClearBackground(BACKGROUND);
#if 0
			drawMap2D(map2d);
			drawRays3D(player, map3d, false, easeFunctions[easeF_index].func);
			drawPlayer(player);
#else
			drawRays3D(player, screen, false, easeFunctions[easeF_index].func);
#endif
			SetTextLineSpacing(20);
			if(debugInfoEnabled)
				DrawText(
					TextFormat(
						"Resolution: %d rays\n"
						"Ease Function: %s\n"
						"Brightness: %f%%\n"
						"Player:\n"
						" pos={%f, %f}\n"
						" rot=%f \n",RAYS, easeFunctions[easeF_index].name,player.brightness * 100.0f, player.pos.x, player.pos.y, player.rot), 0, 0, 20, RAYWHITE);
		EndDrawing();
	}
	CloseWindow();
}
