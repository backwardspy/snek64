#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <c64/vic.h>
#include <c64/joystick.h>

#define W 40
#define H 25

typedef uint8_t u8;
typedef uint16_t u16;

enum ScreenCodes {
    SC_BG = 32,
    SC_SNAKE = 81,
    SC_FOOD = 83,
};

const char msg_status[] = s" score: 000000  high: 000000            ";
const char msg_prompt[] = s"-- fire to begin --";
const char msg_died[] = s"-- you died. fire to try again. --";

u16 food;

u8 *const screen = (u8*)0x0400;
u8 *const colors = (u8*)0xD800;

u16 index(u8 x, u8 y) {
    return y * W + x;
}

void undex(u16 addr, u8 *const x, u8 *const y) {
    *x = addr % W;
    *y = addr / W;
}

typedef struct {
    u16 buffer[256];
    u8 head;
    u8 tail;
} deque_t;

void deque_init(deque_t *const deque) {
    deque->head = 0x00;
    deque->tail = 0xFF;
}

bool deque_empty(const deque_t *const deque) {
    return deque->head == deque->tail + 1;
}

bool deque_full(const deque_t *const deque) {
    return deque->head == deque->tail;
}

u16 deque_front(const deque_t *const deque) {
    if (deque_empty(deque)) {
        return 0;
    }
    return deque->buffer[deque->head - 1];
}

u16 deque_back(const deque_t *const deque) {
    if (deque_empty(deque)) {
        return 0;
    }
    return deque->buffer[deque->tail + 1];
}

void deque_push_front(deque_t *const deque, u16 value) {
    if (deque_full(deque)) {
        return;
    }
    deque->buffer[deque->head++] = value;
}

void deque_push_back(deque_t *const deque, u16 value) {
    if (deque_full(deque)) {
        return;
    }
    deque->buffer[deque->tail--] = value;
}

u16 deque_pop_front(deque_t *const deque) {
    if (deque_empty(deque)) {
        return 0;
    }
    return deque->buffer[--deque->head];
}

u16 deque_pop_back(deque_t *const deque) {
    if (deque_empty(deque)) {
        return 0;
    }
    return deque->buffer[++deque->tail];
}

void putsat(const char *const msg, u8 x, u8 y) {
    u16 start = index(x, y);
    for (u8 i = 0; i < strlen(msg); ++i) {
        screen[start + i] = msg[i];
    }
}

void status_init() {
    putsat(msg_status, 0, 0);
}

void score_add() {
    // score digits sit at characters 8-12
    for (u8 i = 12; i >= 8; --i) {
        u8 digit = screen[i] - '0';
        if (digit == 9) {
            screen[i] = '0';
        } else {
            screen[i] = '0' + digit + 1;
            break;
        }
    }
}

void score_reset() {
    // copy digits from 8-12 to 22-26
    for (u8 i = 0; i <= 4; ++i) {
        screen[22+i] = screen[8+i];
        screen[8+i] = '0';
    }
}

void food_move() {
    // we don't need to worry about clearing the food position because the
    // snake will do it for us.
    food = index(rand() % W, 1 + (rand() % (H - 1)));
    screen[food] = SC_FOOD;
}

typedef struct {
    u8 dx, dy;
    deque_t body;
} snake_t;
snake_t snake;

void snake_init() {
    snake.dx = 1;
    snake.dy = 0;
    deque_init(&snake.body);
    deque_push_front(&snake.body, index(W / 3, H / 2));
}

void snake_input() {
    if (snake.dx == 0) {
        if (joyx[0] == -1) {
            snake.dx = -1;
            snake.dy = 0;
        } else if (joyx[0] == 1) {
            snake.dx = 1;
            snake.dy = 0;
        }
    } else if (snake.dy == 0) {
        if (joyy[0] == -1) {
            snake.dy = -1;
            snake.dx = 0;
        } else if (joyy[0] == 1) {
            snake.dy = 1;
            snake.dx = 0;
        }
    }
}

bool snake_move() {
    // move the snake. return true if the snake is still alive.
    u8 x, y;
    undex(deque_front(&snake.body), &x, &y);
    x += snake.dx;
    y += snake.dy;

    if (x >= W || y >= H || y < 1) {
        return false;
    }

    u16 new_head = index(x, y);
    deque_push_front(&snake.body, new_head);
    screen[new_head] = SC_SNAKE;

    if (new_head == food) {
        score_add();
        food_move();
    } else {
        screen[deque_pop_back(&snake.body)] = SC_BG;
    }
    
    return true;
}

void cls() {
    #pragma unroll(page)
    for (u16 i = 0; i < W * H; ++i) {
        screen[i] = ' ';
        colors[i] = VCOL_BROWN;
    }
}

void cls_board() {
    #pragma unroll(page)
    for (u16 i = W; i < W * H; ++i) {
        screen[i] = ' ';
    }
}

void delay() {
    #pragma unroll(full)
    for (u8 i = 0; i < 4; ++i) {
        vic_waitFrame();
    }
}

void wait_fire() {
    joy_poll(0);
    while (!joyb[0]) joy_poll(0);
}

int main() {
    // first start
    vic.color_border = VCOL_GREEN;
    vic.color_back = VCOL_LT_GREEN;
    cls();
    status_init();
    putsat(msg_prompt, (W - strlen(msg_prompt)) / 2, H / 3);
    wait_fire();

    while (true) {
        // prepare game
        cls_board();
        snake_init();
        food_move();

        // game loop
        while (true) {
            joy_poll(0);
            snake_input();
            if (!snake_move()) break;
            delay();
        }

        // died, prompt to try again
        score_reset();
        putsat(msg_died, (W - strlen(msg_died)) / 2, H / 3);
        wait_fire();
    }

    return 0;
}
