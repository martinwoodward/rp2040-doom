
#include "32blit.hpp"
#include "engine/api_private.hpp"

extern "C" {
#include "d_event.h"
#include "doomkeys.h"
#include "doom/doomstat.h"
#include "doom/s_sound.h"
#include "doom/f_wipe.h"

void D_DoomMain();
void D_Display();

void I_32BlitStartRender();
void I_32BlitEndRender(uint8_t *data, int w, int h);

void I_32BlitInput();
const char *I_32BlitSaveDir();
}

static uint32_t last_button_state = 0;

// Home button: short press = use, long press = menu (escape)
static const uint32_t home_long_press_ms = 600;
static const uint32_t home_use_pulse_ms = 150;
static uint32_t home_press_time = 0;
static bool home_long_press = false;
static uint32_t home_use_until = 0;

static void add_appended_files()
{
#ifdef TARGET_32BLIT_HW
    extern char _flash_end;

    if(memcmp(&_flash_end, "APPFILES", 8) != 0)
        return;

    uint32_t num_files = *reinterpret_cast<uint32_t *>(&_flash_end + 8);

    const int header_size = 12, file_header_size = 8;

    auto dataPtr = &_flash_end + header_size + file_header_size * num_files;

    for(auto i = 0u; i < num_files; i++)
    {
        auto filename_length = *reinterpret_cast<uint16_t *>(&_flash_end + header_size + i * file_header_size);
        auto file_length = *reinterpret_cast<uint32_t *>(&_flash_end + header_size + i * file_header_size + 4);

        blit::File::add_buffer_file(std::string(dataPtr, filename_length), reinterpret_cast<uint8_t *>(dataPtr + filename_length), file_length);

        dataPtr += filename_length + file_length;
    }
#endif
}

// input event generation
static void CheckButton(uint32_t changed_buttons, blit::Button button, int key)
{
	event_t event;
	if(changed_buttons & button)
	{
		event.type = (blit::buttons & button) ? ev_keydown : ev_keyup;
		event.data1 = key;
		D_PostEvent(&event);
	}
}

void I_32BlitInput()
{
	event_t event = {};

	uint32_t changed_buttons = blit::buttons ^ last_button_state;

	CheckButton(changed_buttons, blit::Button::DPAD_LEFT, KEY_LEFTARROW);
	CheckButton(changed_buttons, blit::Button::DPAD_RIGHT, KEY_RIGHTARROW);
	CheckButton(changed_buttons, blit::Button::DPAD_UP, KEY_UPARROW);
	CheckButton(changed_buttons, blit::Button::DPAD_DOWN, KEY_DOWNARROW);

	CheckButton(changed_buttons, blit::Button::X, KEY_RSHIFT);
	CheckButton(changed_buttons, blit::Button::Y, KEY_TAB); // map toggle

	CheckButton(changed_buttons, blit::Button::MENU, KEY_ESCAPE);

	uint32_t now = blit::now();
	if(changed_buttons & blit::Button::HOME)
	{
		if(blit::buttons & blit::Button::HOME)
		{
			home_press_time = now;
			home_long_press = false;
		}
		else if(home_long_press)
		{
			event.type = ev_keyup;
			event.data1 = KEY_ESCAPE;
			D_PostEvent(&event);
		}
		else
			home_use_until = now + home_use_pulse_ms;
	}
	else if((blit::buttons & blit::Button::HOME) && !home_long_press && now - home_press_time >= home_long_press_ms)
	{
		home_long_press = true;
		event.type = ev_keydown;
		event.data1 = KEY_ESCAPE;
		D_PostEvent(&event);
	}

	last_button_state = blit::buttons;

    if (I_GetTime() <= joywait)
        return;

    event.type = ev_joystick;
    // joystick buttons
    event.data1 = 0;
    if(blit::buttons & blit::Button::A)
        event.data1 |= 1 << 0; // "fire"
    if((blit::buttons & blit::Button::B) || int32_t(home_use_until - now) > 0)
        event.data1 |= 1 << 3; // "use"
    
    event.data2 = blit::joystick.x * (INT16_MAX - 1);
    event.data3 = blit::joystick.y * (INT16_MAX - 1);
    D_PostEvent(&event);
}

const char *I_32BlitSaveDir()
{
    return blit::api.get_save_path();
}

static bool done_init = false;

void init()
{
    // palette would be best, if more widely supported
    blit::set_screen_mode(blit::ScreenMode::hires, blit::PixelFormat::RGB565);

    add_appended_files();
}

void update(uint32_t time)
{
    if(!done_init) {
        D_DoomMain();
        done_init = true;
        return;
    }

    if(blit::now() - time > 20)
        return;

    if(wipestate)
        return;

    TryRunTics();
	S_UpdateSounds(players[consoleplayer].mo);
}

void render(uint32_t time)
{
    using namespace blit;

    if(!done_init)
        return;

    I_32BlitStartRender();

    D_Display();

    I_32BlitEndRender(screen.data, screen.bounds.w, screen.bounds.h);
}