
#include "32blit.hpp"

extern "C" {
#include "doom/doomstat.h"
#include "doom/s_sound.h"

void D_DoomMain();
void D_Display();

void I_32BlitStartRender();
void I_32BlitEndRender(uint8_t *data, int w, int h);
}

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

bool init_wait = true, done_init = false;

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

    TryRunTics();
	S_UpdateSounds(players[consoleplayer].mo);
}

void render(uint32_t time)
{
    using namespace blit;

    if(!done_init)
        return;

    I_32BlitStartRender();

    D_Display(); // do {} while(wipestate);

    I_32BlitEndRender(screen.data, screen.bounds.w, screen.bounds.h);
}