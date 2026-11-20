#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "input.h"
#include "input_map.h"

static uint64_t now_ms = 1000;
static unsigned scans;
static char preferred[128] = "auto";

// Keep this fixture independent of any physical controllers present on the host.
static SDL_JoystickID *virtual_gamepads(int *n) {
    SDL_JoystickID *ids = SDL_GetGamepads(n);
    int kept = 0;
    for (int i = 0; i < *n; i++)
        if (SDL_IsJoystickVirtual(ids[i])) ids[kept++] = ids[i];
    *n = kept;
    scans++;
    return ids;
}
#define SDL_GetGamepads virtual_gamepads
#include "input.c"
#undef SDL_GetGamepads

uint64_t plat_now_ns(void) { return now_ms * 1000000u; }
void plat_mutex_lock(PlatMutex *m) { (void)m; }
void plat_mutex_unlock(PlatMutex *m) { (void)m; }
const char *cfg_get(const char *key) { (void)key; return NULL; }
const char *cfg_value(const char *key) { (void)key; return preferred; }
unsigned cfg_version(void) { return 1; }
int cfg_bool(const char *key) { (void)key; return 1; }
MotionFusion *motion_global(void) { return NULL; }
void motion_restart(MotionFusion *motion) { (void)motion; }
void rt_log(const char *fmt, ...) { (void)fmt; }


typedef struct { SDL_JoystickID id; Uint16 rumble; unsigned stops; } Fixture;

static bool SDLCALL record_rumble(void *arg, Uint16 low, Uint16 high) {
    Fixture *f = arg;
    f->rumble = low;
    if (!low && !high) f->stops++;
    return true;
}

static void attach(Fixture *f) {
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.name = "Co-op test pad";
    desc.vendor_id = 0x057E;
    desc.product_id = 0x2009;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    desc.Rumble = record_rumble;
    desc.userdata = f;
    f->id = SDL_AttachVirtualJoystick(&desc);
    assert(f->id);
    SDL_UpdateGamepads();
}

static void detach(Fixture *f) {
    assert(SDL_DetachVirtualJoystick(f->id));
    SDL_UpdateGamepads();
}

static void seats(SDL_JoystickID p1, SDL_JoystickID p2) {
    assert((pad ? SDL_GetGamepadID(pad) : 0) == p1);
    assert((pad2 ? SDL_GetGamepadID(pad2) : 0) == p2);
}

int main(void) {
    assert(SDL_Init(SDL_INIT_GAMEPAD));
    state = 1;
    Fixture a = {0}, b = {0};
    attach(&a);
    attach(&b);
    SDL_GUID ga = SDL_GetGamepadGUIDForID(a.id), gb = SDL_GetGamepadGUIDForID(b.id);
    assert(!memcmp(&ga, &gb, sizeof ga) && a.id != b.id);
    scan_pads();
    pad2_scan();
    seats(a.id, b.id);

    unsigned steady = scans;
    for (int i = 0; i < 20; i++) {
        now_ms++;
        pad2_scan();
    }
    assert(scans == steady);

    // A fast replacement arrives within the previous player 2 scan interval.
    detach(&b);
    attach(&b);
    pad2_scan();
    seats(a.id, b.id);
    assert(input_rumble2(5000, 0, 250) && b.rumble == 5000 && !a.rumble);

    // Choose B as player 1 without waiting for a player 2 poll.
    InputPadDevice *devices;
    int n = devices_locked(&devices);
    for (int i = 0; i < n; i++)
        if (devices[i].id == b.id) snprintf(preferred, sizeof preferred, "%s", devices[i].key);
    free(devices);
    assert(strcmp(preferred, "auto"));
    pad_selection.force = 1;
    unsigned b_stops = b.stops;
    scan_pads();
    seats(b.id, 0);
    assert(b.stops == b_stops + 1 && b.rumble == 0);
    assert(!input_rumble2(6000, 0, 250));
    assert(SDL_RumbleGamepad(pad, 7000, 0, 250) && b.rumble == 7000);
    pad2_scan();   // the old P1 is available immediately, without disturbing new P1 rumble
    seats(b.id, a.id);
    assert(b.stops == b_stops + 1 && b.rumble == 7000);
    assert(input_rumble2(8000, 0, 250) && a.rumble == 8000);

    // Physical P1 has just left. Drop the old P2 slot and its active rumble before promoting the survivor.
    detach(&b);
    unsigned a_stops = a.stops;
    assert(!input_rumble2(9000, 0, 250));
    pad2_scan();
    assert(!pad2 && a.stops == a_stops + 1 && !a.rumble);
    scan_pads();
    pad2_scan();
    seats(a.id, 0);

    for (int arrival = 0; arrival < 4; arrival++) {
        detach(&a);
        if (arrival) detach(&b);
        strcpy(preferred, "auto");
        scan_pads();
        pad2_scan();
        seats(0, 0);
        bool reversed = (arrival & 1) != 0;
        bool same_poll = (arrival & 2) != 0;
        Fixture *first = reversed ? &b : &a, *second = reversed ? &a : &b;
        attach(first);
        if (!same_poll) {
            scan_pads();
            pad2_scan();
            seats(first->id, 0);
        }
        attach(second);
        now_ms += RESCAN_MS;
        scan_pads();
        pad2_scan();
        // An established primary stays player 1; otherwise Auto uses the last enumerated arrival.
        if (same_poll) seats(second->id, first->id);
        else seats(first->id, second->id);
    }
    detach(&a);
    detach(&b);
    scan_pads();
    pad2_scan();
    free(pad_selection.ids);
    SDL_Quit();
    puts("controller co-op: role changes, rumble stops, promotion and both reconnect orders passed");
    return 0;
}
