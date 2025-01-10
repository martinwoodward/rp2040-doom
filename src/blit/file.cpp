#include "engine/file.hpp"

struct wrap_FILE
{
    blit::File file;
    uint32_t offset;
};

extern "C" wrap_FILE *__wrap_fopen(const char *filename, const char *mode)
{
    int blit_mode;

    // no append mode
    if(mode[0] != 'r' && mode[0] != 'w')
        return nullptr;

    // ignore b flag
    int i = 1;
    if(mode[i] == 'b')
        i++;

    if(mode[0] == 'r')
    {
        if(mode[i] != 0) return nullptr; // no r+

        blit_mode = blit::OpenMode::read;
    }
    else if(mode[0] == 'w')
    {
        blit_mode = blit::OpenMode::write;
        if(mode[i] == '+')
            blit_mode |= blit::OpenMode::read;
    }

    auto ret = new wrap_FILE;
    ret->file.open(filename, blit_mode);
    ret->offset = 0;

    if(!ret->file.is_open())
    {
        delete ret;
        return nullptr;
    }
    
    return ret;
}

extern "C" int __wrap_fclose(wrap_FILE *file)
{
    file->file.close();
    delete file;
    return 0;
}


extern "C" size_t __wrap_fread(void *buffer, size_t size, size_t count, wrap_FILE *file)
{
    if(!file) return 0;

    auto ret = file->file.read(file->offset, size * count, (char *)buffer);
    file->offset += ret;

    return ret < 0 ? 0 : ret / size;
}

extern "C" size_t __wrap_fwrite(const void *buffer, size_t size, size_t count, wrap_FILE *file)
{
    if(!file) return 0;

    auto ret = file->file.write(file->offset, size * count, (char *)buffer);
    file->offset += ret;

    return ret < 0 ? 0 : ret / size;
}

extern "C" int __wrap_fseek(wrap_FILE *file, long offset, int origin)
{
    if(!file) return 0;

    if(origin == SEEK_SET)
        file->offset = offset;
    else if(origin == SEEK_CUR)
        file->offset += offset;
    else
        file->offset = file->file.get_length() - offset;

    return 0;
}

extern "C" long __wrap_ftell(wrap_FILE *file)
{
    if(!file) return 0;

    return file->offset;
}

extern "C" int __wrap_remove(const char *filename)
{
    return blit::remove_file(filename) ? 0 : 1;
}

extern "C" int __wrap_rename(const char *old_name, const char *new_name)
{
    return blit::rename_file(old_name, new_name) ? 0 : 1;
}