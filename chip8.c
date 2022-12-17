#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <sys/time.h>

#define NREG (0X10)

#define NMEM (0X10000)
#define USERMEM (0X200)

#define SCREEN_X (0X40)
#define SCREEN_Y (0X20)
#define XCOORD(xc) (((xc) % SCREEN_X))
#define YCOORD(yc) (((yc) % SCREEN_Y))

FILE *logfile;
#define LOGP(message, ...) ({					\
	fprintf(logfile, message, ##__VA_ARGS__); 	\
	fflush(logfile); 							\
})
#define LOG(message, ...) LOGP(message "\n", ##__VA_ARGS__)

const uint8_t F = 0XF;
uint8_t V[NREG];
uint16_t I;
uint8_t delay, sound;
uint8_t mem[NMEM] = {
	/* Fonts */
	0XF0, 0X90, 0X90, 0X90, 0XF0,
	0X20, 0X60, 0X20, 0X20, 0X70,
	0XF0, 0x10, 0XF0, 0X80, 0XF0,
	0XF0, 0x10, 0XF0, 0X10, 0XF0,
	0X90, 0x90, 0XF0, 0X10, 0X10,
	0XF0, 0x80, 0XF0, 0X10, 0XF0,
	0XF0, 0x80, 0XF0, 0X90, 0XF0,
	0XF0, 0x10, 0X20, 0X40, 0X40,
	0XF0, 0x90, 0XF0, 0X90, 0XF0,
	0XF0, 0x90, 0XF0, 0X10, 0XF0,
	0XF0, 0x90, 0XF0, 0X90, 0X90,
	0XE0, 0x90, 0XE0, 0X90, 0XE0,
	0XF0, 0x80, 0X80, 0X80, 0XF0,
	0XE0, 0x90, 0X90, 0X90, 0XE0,
	0XF0, 0x80, 0XF0, 0X80, 0XF0,
	0XF0, 0x80, 0XF0, 0X80, 0X80,
	0
};
uint8_t screen[SCREEN_Y][SCREEN_X];

#define STACKSZ (12)
uint16_t retaddr[STACKSZ];
int retp;

uint8_t rseed;

struct timespec epoch; // time of program start.
struct {
	uint32_t when; // ticks when the key was pressed.
} pressed[0X10];

#define MEM(n0, n1, n2) ((n0 << 8) | (n1 << 4) | n2)
#define BYTE(n0, n1) ((n0 << 4) | n1)

#define RESPONSE_TIME (250000)
#define POLL_WAIT_TIME (0)

void moveto(int y, int x)
{
	printf("\x1b[%d;%dH", y, x);
	fflush(stdout);
}

void setup_chip8()
{
	printf("\x1b[2J");
	fflush(stdout);
	rseed = 0x3b;
	clock_gettime(CLOCK_REALTIME, &epoch);
}

uint32_t now()
{
	struct timespec n;
	clock_gettime(CLOCK_REALTIME, &n);
	return (n.tv_sec - epoch.tv_sec) * 1000000 + (n.tv_nsec / 1000);
}

void clear_screen()
{
	for (int y = 0; y < SCREEN_Y; ++y) {
		for (int x = 0; x < SCREEN_X; ++x) {
			screen[y][x] = 0;
		}
	}
	moveto(SCREEN_Y, SCREEN_X);
	printf("\x1b[1J");
	fflush(stdout);
}

void draw_sprite(uint8_t Y, uint8_t X, uint8_t N)
{
	uint8_t sprite[15][8];
	V[F] = 0;
	for (int j = 0; j < 8; ++j) {
		int bitshift = 7 - j;
		uint8_t xc = XCOORD(V[X] + j);
		for (int i = 0; i < N; ++i) {
			uint8_t yc = YCOORD(V[Y] + i);
			sprite[i][j] = (mem[I+i] & (1 << bitshift)) >> bitshift;
			if (screen[yc][xc]) {
				if (sprite[i][j]) {
					V[F] = 1;
					screen[yc][xc] = 0;
				}
			} else {
				screen[yc][xc] = sprite[i][j];
			}
			moveto(yc+1, xc+1);
			if (screen[yc][xc]) {
				printf("\u2588");
			} else {
				printf(" ");
			}
		}
	}
	fflush(stdout);
}

void bell()
{
	printf("\a");
	fflush(stdout);
}

uint8_t mapkey(uint8_t key)
{
	switch (key) {
		case '1': return 0X01;
		case '2': return 0X02;
		case '3': return 0X03;
		case '4': return 0X0C;
		case 'q': return 0X04;
		case 'w': return 0X05;
		case 'e': return 0X06;
		case 'r': return 0X0D;
		case 'a': return 0X07;
		case 's': return 0X08;
		case 'd': return 0X09;
		case 'f': return 0X0E;
		case 'z': return 0X0A;
		case 'x': return 0X00;
		case 'c': return 0X0B;
		case 'v': return 0X0F;
	}
}

void gather_keys()
{
	for (int k = 0; k < 0X10; ++k) {
		if (now() - pressed[k].when > RESPONSE_TIME) {
			pressed[k].when = 0;
		}
	}
	struct pollfd fd;
	fd.fd = 0;
	fd.events = POLLIN;
	for (int ii = 1; ii < 3; ++ii) {
		if (poll(&fd, 1, POLL_WAIT_TIME)) {
			if (fd.revents & POLLIN) {
				int kk;
				read(0, &kk, 1);
				uint8_t k = mapkey(kk);
				pressed[k].when = now();
			}
		}
	}
}

uint8_t getkey()
{
	int kk;
	read(0, &kk, 1);
	return mapkey(kk);
}

bool is_key(uint8_t key)
{
	return pressed[key].when > 0;
}

bool is_not_key(uint8_t key)
{
	return pressed[key].when == 0;
}

uint8_t randbyte()
{
	rseed = (13*rseed + 5) % 0X100;
	return rseed;
}

void load_rom(const char *romfile)
{
	FILE *rom = fopen(romfile, "rb");
	fread(mem+USERMEM, 1, NMEM-USERMEM, rom);
}

int main(int argc, const char *argv[])
{
	uint16_t ip;
	uint8_t i, X, Y, Z, N, NN;
	uint16_t NNN;
	const uint8_t F = 0XF;
	int frame_rate = 60;
	uint32_t current, prev;

	if (argc < 1) {
		exit(0);
	}

	logfile = fopen("chip8.log", "w");
	LOG("loading rom %s", argv[1]);
	load_rom(argv[1]);
	if (argc > 2) {
		frame_rate = atoi(argv[2]);
	}
	setup_chip8();
	ip = USERMEM;
	current = now();
    prev = 0;
	LOG("Starting program.");
	while (1) {
		current = now();
		int d = 1000000/frame_rate - (current - prev);
		if (d > 0) {
			usleep(d);
		}
		prev = current;
		gather_keys();
		if (delay > 0) {
			--delay;
		}
		if (sound > 0) {
			--sound;
			if (!sound) { bell(); }
		}
		i = (mem[ip] & 0xf0) >> 4;
		X = mem[ip] & 0x0f;
		Y = (mem[ip+1] & 0xf0) >> 4;
		Z = N = mem[ip+1] & 0x0f;
		NNN = MEM(X, Y, Z);
		NN = BYTE(Y, Z);
		ip += 2;
		switch (i) {
			case 0:
				if (NNN == 0X0E0) {
					LOG("CLR");
					clear_screen();
				} else if (NNN == 0X0EE) {
					LOG("RET");
					--retp;
					ip = retaddr[retp];
				}
				break;
			case 1:
				LOG("JUMP %x", NNN);
				ip = NNN;
				break;
			case 2:
				LOG("CALL %x", NNN);
				assert (retp < STACKSZ);
				retaddr[retp++] = ip;
				ip = NNN;
				break;
			case 3:
				if (V[X] == NN) {
					ip += 2;
				}
				break;
			case 4:
				if (V[X] != NN) {
					ip += 2;
				}
				break;
			case 5:
				if (V[X] == V[Y]) {
					ip += 2;
				}
				break;
			case 6:
				V[X] = NN;
				break;
			case 7:
				V[X] += NN;
				break;
			case 8:
				switch (Z) {
					case 0:
						V[X] = V[Y];
						break;
					case 1:
						V[X] |= V[Y];
						break;
					case 2:
						V[X] &= V[Y];
						break;
					case 3:
						V[X] ^= V[Y];
						break;
					case 4:
						V[F] = (V[X] = V[X] + V[Y]) > 255;
						break;
					case 5:
						V[F] = V[X] >= V[Y];
						V[X] -= V[Y];
						break;
					case 6:
						V[F] = V[Y] & 0X1;
						V[X] = V[Y] >> 1;
						break;
					case 7:
						V[F] = V[Y] >= V[X];
						V[X] = V[Y] - V[X];
						break;
					case 0XE:
						V[F] = (V[Y] & 0X80) >> 7;
						V[X] = V[Y] << 1;
						break;
				}
				break;
			case 9:
				if (V[X] != V[Y]) {
					ip += 2;
				}
				break;
			case 0XA:
				I = NNN;
				break;
			case 0XB:
				ip = NNN + V[0];
				break;
			case 0XC:
				V[X] = randbyte() & NN;
				break;
			case 0XD:
				LOG("DRAW %x, %x, %d (I = %x)", V[Y], V[X], N, I);
				draw_sprite(Y, X, N);
				break;
			case 0XE:
				if (NN == 0X9E) {
					LOGP("KEY %x? ", V[X]);
					if (is_key(V[X])) {
						LOG("Yes.");
						ip += 2;
					} else {
						LOG("No.");
					}
				} else if (NN == 0XA1) {
					LOGP("KEYNOT %x? ", V[X]);
					if (is_not_key(V[X])) {
						LOG("Yes.");
						ip += 2;
					} else {
						LOG("No.");
					}
				}
				break;
			case 0XF:
				if (NN == 0X07) {
					V[X] = delay;
				} else if (NN == 0X0A) {
					V[X] = getkey();
				} else if (NN == 0X15) {
					delay = V[X];
				} else if (NN == 0X18) {
					sound = V[X];
				} else if (NN == 0X1E) {
					I += V[X];
				} else if (NN == 0X29) {
					I = V[X] * 5;
				} else if (NN == 0X33) {
					uint8_t v = V[X];
					mem[I+2] = v % 10;
					v /= 10;
					mem[I+1] = v%10;
					v /= 10;
					mem[I] = v%10;
				} else if (NN == 0X55) {
					for (int i = 0; i <= X; ++i) {
						mem[I+i] = V[i];
					}
					I = I + X + 1;
				} else if (NN == 0X65) {
					for (int i = 0; i <= X; ++i) {
						V[i] = mem[I+i];
					}
					I = I + X + 1;
				}
				break;
		}
	}
}
