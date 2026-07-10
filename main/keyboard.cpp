#include "keyboard.h"
#include <M5Unified.h>
#include <driver/gpio.h>
#include <rom/ets_sys.h>
#include <string.h>

// --- Key code definitions (from M5Cardputer Keyboard_def.h) ---------------
#define KEY_LEFT_CTRL  0x80
#define KEY_LEFT_SHIFT 0x81
#define KEY_LEFT_ALT   0x82
#define KEY_FN         0xff
#define KEY_OPT        0x00
#define KEY_BACKSPACE  0x2a
#define KEY_TAB_CODE   0x2b
#define KEY_ENTER_CODE 0x28
#define KEY_ESCAPE     0x29
#define KEY_DELETE     0x4c
#define KEY_RIGHT_CODE 0x4F
#define KEY_LEFT_CODE  0x50
#define KEY_DOWN_CODE  0x51
#define KEY_UP_CODE    0x52
#define KEY_NONE       0x00

// Matrix wiring: three demux-select outputs and seven row inputs.
static const gpio_num_t OUTPUT_PINS[3] = {GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_11};
static const gpio_num_t INPUT_PINS[7]  = {GPIO_NUM_13, GPIO_NUM_15, GPIO_NUM_3,
                                          GPIO_NUM_4,  GPIO_NUM_5,  GPIO_NUM_6,
                                          GPIO_NUM_7};

// Column index per input row, selected by whether the demux value is >3.
// Chart_t{value, x_1, x_2} from IOMatrix.h.
static const uint8_t X_MAP_X1[7] = {0, 2, 4, 6, 8, 10, 12};
static const uint8_t X_MAP_X2[7] = {1, 3, 5, 7, 9, 11, 13};

// Per-position key values: {value_first, value_second (Aa), value_third (fn)}.
struct KeyValue3 { uint8_t first, second, third; };
static const KeyValue3 KEY_MAP[4][14] = {
    {{'`','~',KEY_ESCAPE},{'1','!',0x3A},{'2','@',0x3B},{'3','#',0x3C},
     {'4','$',0x3D},{'5','%',0x3E},{'6','^',0x3F},{'7','&',0x40},
     {'8','*',0x41},{'9','(',0x42},{'0',')',0x43},{'-','_',0x44},
     {'=','+',0x45},{KEY_BACKSPACE,KEY_BACKSPACE,KEY_DELETE}},
    {{KEY_TAB_CODE,KEY_TAB_CODE,KEY_NONE},{'q','Q',KEY_NONE},{'w','W',KEY_NONE},
     {'e','E',KEY_NONE},{'r','R',KEY_NONE},{'t','T',KEY_NONE},{'y','Y',KEY_NONE},
     {'u','U',KEY_NONE},{'i','I',KEY_NONE},{'o','O',KEY_NONE},{'p','P',KEY_NONE},
     {'[','{',KEY_NONE},{']','}',KEY_NONE},{'\\','|',KEY_NONE}},
    {{KEY_FN,KEY_FN,KEY_FN},{KEY_LEFT_SHIFT,KEY_LEFT_SHIFT,KEY_NONE},
     {'a','A',KEY_NONE},{'s','S',KEY_NONE},{'d','D',KEY_NONE},{'f','F',KEY_NONE},
     {'g','G',KEY_NONE},{'h','H',KEY_NONE},{'j','J',KEY_NONE},{'k','K',KEY_NONE},
     {'l','L',KEY_NONE},{';',':',KEY_UP_CODE},{'\'','"',KEY_NONE},
     {KEY_ENTER_CODE,KEY_ENTER_CODE,KEY_NONE}},
    {{KEY_LEFT_CTRL,KEY_LEFT_CTRL,KEY_NONE},{KEY_OPT,KEY_OPT,KEY_NONE},
     {KEY_LEFT_ALT,KEY_LEFT_ALT,KEY_NONE},{'z','Z',KEY_NONE},{'x','X',KEY_NONE},
     {'c','C',KEY_NONE},{'v','V',KEY_NONE},{'b','B',KEY_NONE},{'n','N',KEY_NONE},
     {'m','M',KEY_NONE},{',','<',KEY_LEFT_CODE},{'.','>',KEY_DOWN_CODE},
     {'/','?',KEY_RIGHT_CODE},{' ',' ',KEY_NONE}},
};

#define KB_MAX_TRACK 16
struct KeyPos { int8_t x, y; };

static bool s_present = false;
static KeyPos s_prev[KB_MAX_TRACK];
static int s_prev_count = 0;

void keyboard_init(void) {
    auto b = M5.getBoard();
    // Only the classic Cardputer uses the GPIO matrix keyboard handled here.
    if (b != m5::board_t::board_M5Cardputer) {
        s_present = false;
        return;
    }

    for (int i = 0; i < 3; i++) {
        gpio_reset_pin(OUTPUT_PINS[i]);
        gpio_set_direction(OUTPUT_PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(OUTPUT_PINS[i], 0);
    }
    for (int i = 0; i < 7; i++) {
        gpio_reset_pin(INPUT_PINS[i]);
        gpio_set_direction(INPUT_PINS[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(INPUT_PINS[i], GPIO_PULLUP_ONLY);
    }
    s_present = true;
    s_prev_count = 0;
}

bool keyboard_present(void) {
    return s_present;
}

static void set_output(uint8_t value) {
    gpio_set_level(OUTPUT_PINS[0], (value & 0x01) ? 1 : 0);
    gpio_set_level(OUTPUT_PINS[1], (value & 0x02) ? 1 : 0);
    gpio_set_level(OUTPUT_PINS[2], (value & 0x04) ? 1 : 0);
}

// Read the seven row inputs; a pressed key pulls its line low (active low).
static uint8_t get_input(void) {
    uint8_t buffer = 0;
    for (int i = 0; i < 7; i++) {
        if (gpio_get_level(INPUT_PINS[i]) == 0) {
            buffer |= (1 << i);
        }
    }
    return buffer;
}

// Scan the full matrix into out[] (positions into KEY_MAP). Returns count.
static int scan_matrix(KeyPos *out, int max_out) {
    int count = 0;
    for (int i = 0; i < 8; i++) {
        set_output((uint8_t)i);
        ets_delay_us(5); // let the demux line and pull-ups settle
        uint8_t input = get_input();
        if (!input) continue;
        for (int j = 0; j < 7; j++) {
            if (!(input & (1 << j))) continue;
            KeyPos p;
            p.x = (i > 3) ? X_MAP_X1[j] : X_MAP_X2[j];
            int y = (i > 3) ? (i - 4) : i;
            p.y = 3 - y;
            if (count < max_out) out[count++] = p;
        }
    }
    return count;
}

static bool pos_in(const KeyPos *list, int n, KeyPos p) {
    for (int i = 0; i < n; i++) {
        if (list[i].x == p.x && list[i].y == p.y) return true;
    }
    return false;
}

int keyboard_poll(kb_event_t *events, int max_events) {
    if (!s_present) return 0;

    KeyPos cur[KB_MAX_TRACK];
    int cur_count = scan_matrix(cur, KB_MAX_TRACK);

    // First pass: resolve active modifiers over the whole current key set.
    bool shift = false, fn = false;
    for (int i = 0; i < cur_count; i++) {
        uint8_t first = KEY_MAP[cur[i].y][cur[i].x].first;
        if (first == KEY_LEFT_SHIFT) shift = true;
        else if (first == KEY_FN)    fn = true;
    }

    // Second pass: emit events only for keys newly pressed this scan.
    int out = 0;
    for (int i = 0; i < cur_count && out < max_events; i++) {
        if (pos_in(s_prev, s_prev_count, cur[i])) continue; // held, not new
        const KeyValue3 &kv = KEY_MAP[cur[i].y][cur[i].x];

        // Modifiers produce no event of their own.
        if (kv.first == KEY_FN || kv.first == KEY_LEFT_SHIFT ||
            kv.first == KEY_LEFT_CTRL || kv.first == KEY_LEFT_ALT ||
            kv.first == KEY_OPT) {
            continue;
        }

        // Structural keys ignore shift/fn.
        if (kv.first == KEY_ENTER_CODE) { events[out++] = {KB_ENTER, 0}; continue; }
        if (kv.first == KEY_BACKSPACE)  { events[out++] = {KB_BACKSPACE, 0}; continue; }
        if (kv.first == KEY_TAB_CODE)   { events[out++] = {KB_TAB, 0}; continue; }

        if (fn) {
            switch (kv.third) {
                case KEY_UP_CODE:    events[out++] = {KB_UP, 0}; break;
                case KEY_DOWN_CODE:  events[out++] = {KB_DOWN, 0}; break;
                case KEY_LEFT_CODE:  events[out++] = {KB_LEFT, 0}; break;
                case KEY_RIGHT_CODE: events[out++] = {KB_RIGHT, 0}; break;
                case KEY_ESCAPE:     events[out++] = {KB_ESC, 0}; break;
                default: break; // F-keys / delete: not used by the app
            }
            continue;
        }

        uint8_t code = shift ? kv.second : kv.first;
        if (code >= 32 && code <= 126) {
            events[out++] = {KB_CHAR, (char)code};
        }
    }

    // Remember the current key set for next-call edge detection.
    memcpy(s_prev, cur, sizeof(KeyPos) * cur_count);
    s_prev_count = cur_count;

    return out;
}
