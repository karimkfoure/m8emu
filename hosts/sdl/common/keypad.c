#include "keypad.h"

#include <math.h>
#include <string.h>

bool m8_keypad_button_rect(const SDL_FRect *viewport,uint8_t key,SDL_FRect *rectangle) {
    if(!viewport || !rectangle) return false;
    M8KeypadRect view={viewport->x,viewport->y,viewport->w,viewport->h},r;
    if(!m8_keypad_cpu_button_rect(&view,key,&r)) return false;
    *rectangle=(SDL_FRect){r.x,r.y,r.w,r.h};return true;
}
uint8_t m8_keypad_hit_test(const SDL_FRect *viewport,float x,float y) {
    if(!viewport) return 0;
    const M8KeypadRect view={viewport->x,viewport->y,viewport->w,viewport->h};
    return m8_keypad_cpu_hit_test(&view,x,y);
}

void m8_keypad_init(M8Keypad *keypad) {
    memset(keypad, 0, sizeof(*keypad));
}

static M8KeypadContact *find_contact(M8Keypad *keypad, uint64_t device,
                                    uint64_t pointer) {
    for (unsigned i = 0; i < M8_KEYPAD_CONTACTS; ++i) {
        M8KeypadContact *c = &keypad->contacts[i];
        if (c->occupied && c->device_id == device && c->pointer_id == pointer)
            return c;
    }
    return NULL;
}

bool m8_keypad_pointer_down(M8Keypad *keypad, const SDL_FRect *viewport,
                           uint64_t device_id, uint64_t pointer_id,
                           float x, float y) {
    if (find_contact(keypad, device_id, pointer_id))
        return m8_keypad_pointer_move(keypad, viewport, device_id, pointer_id, x, y);
    uint8_t key = m8_keypad_hit_test(viewport, x, y);
    if (!key)
        return false;
    for (unsigned i = 0; i < M8_KEYPAD_CONTACTS; ++i) {
        M8KeypadContact *c = &keypad->contacts[i];
        if (!c->occupied) {
            *c = (M8KeypadContact){device_id, pointer_id, key, true, true};
            return true;
        }
    }
    return false;
}

bool m8_keypad_pointer_move(M8Keypad *keypad, const SDL_FRect *viewport,
                           uint64_t device_id, uint64_t pointer_id,
                           float x, float y) {
    M8KeypadContact *c = find_contact(keypad, device_id, pointer_id);
    if (!c)
        return false;
    c->inside = m8_keypad_hit_test(viewport, x, y) == c->key;
    return true;
}

bool m8_keypad_pointer_up(M8Keypad *keypad, uint64_t device_id,
                         uint64_t pointer_id) {
    M8KeypadContact *c = find_contact(keypad, device_id, pointer_id);
    if (!c)
        return false;
    memset(c, 0, sizeof(*c));
    return true;
}

bool m8_keypad_pointer_cancel(M8Keypad *keypad, uint64_t device_id,
                             uint64_t pointer_id) {
    return m8_keypad_pointer_up(keypad, device_id, pointer_id);
}

void m8_keypad_release_all(M8Keypad *keypad) {
    m8_keypad_init(keypad);
}

uint8_t m8_keypad_held(const M8Keypad *keypad) {
    uint8_t mask = 0;
    for (unsigned i = 0; i < M8_KEYPAD_CONTACTS; ++i) {
        const M8KeypadContact *c = &keypad->contacts[i];
        if (c->occupied && c->inside)
            mask |= c->key;
    }
    return mask;
}
