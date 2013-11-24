#include "DxLib.h"

SDL_Joystick *joystick;

bool keysHeld[SDLK_LAST];
bool sound = true;
bool fullscreen = false;

void deinit ();

int
DxLib_Init ()
{
    atexit (deinit);

    if (SDL_Init (SDL_INIT_EVERYTHING) < 0)
    {
        fprintf (stderr, "Unable to init SDL: %s\n", SDL_GetError ());
        return -1;
    }

#ifdef _WIN32
#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION <= 2
    putenv("SDL_VIDEODRIVER=directx");
#else
    putenv("SDL_VIDEODRIVER=win32");
#endif
#endif

    if (!(screen = SDL_SetVideoMode (480 /*(int)fmax/100 */ ,
                                     420 /*(int)fymax/100 */ , 32,
                                     SDL_HWSURFACE | SDL_DOUBLEBUF | (fullscreen ? SDL_FULLSCREEN : 0))))
    {
        if (!(screen = SDL_SetVideoMode (480 /*(int)fmax/100 */ ,
                                         420 /*(int)fymax/100 */ , 32,
                                         SDL_SWSURFACE | SDL_DOUBLEBUF | (fullscreen ? SDL_FULLSCREEN : 0))))
        {
            SDL_Quit ();
            return -1;
        }
    }

    SDL_WM_SetCaption ("Syobon Action", NULL);
    if (fullscreen) SDL_ShowCursor (SDL_DISABLE);

#if SDL_IMAGE_MAJOR_VERSION > 1 || SDL_IMAGE_MINOR_VERSION > 2 || SDL_IMAGE_PATCHLEVEL >= 8
    if (IMG_Init (IMG_INIT_PNG) != IMG_INIT_PNG)
    {
        fprintf (stderr, "Unable to init SDL_img: %s\n", IMG_GetError ());
        return -1;
    }
#endif

    //Audio Rate, Audio Format, Audio Channels, Audio Buffers
#define AUDIO_CHANNELS 2
    if (sound && Mix_OpenAudio (22050, AUDIO_S16SYS, AUDIO_CHANNELS, 1024))
    {
        fprintf (stderr, "Unable to init SDL_mixer: %s\n", Mix_GetError ());
        sound = false;
    }
    //Try to get a joystick
    joystick = SDL_JoystickOpen (0);

    for (int i = 0; i < SDLK_LAST; i++)
        keysHeld[i] = false;

    srand ((unsigned int) time (NULL));

    return 0;
}

//Main screen
SDL_Surface *screen;

byte fontType = DX_FONTTYPE_NORMAL;

void
ChangeFontType (byte type)
{
    fontType = type;
}

#include "font.h"

static void
DrawChar (const unsigned char *ch, int a, int b, Uint32 c)
{
    if (!screen) return;

    int i, j;

    if (*ch <= 0x7f) {
        // ASCII character
        unsigned short cvtchar = ascii2sjis(*ch);

        if (cvtchar) {
            unsigned char buf[2];

            buf[0] = cvtchar >> 8;
            buf[1] = cvtchar & 0xff;

            unsigned short *font = (unsigned short *)kanjiaddr(buf);

            if (font) {
                for (i = 0; i < 15; i++) {
                    for (j = 0; j < 16; j++) {
                        if (*font & (1 << (16 - j))) {
                            if (b + i >= screen->h || b + i < 0) continue;
                            if (a + j >= screen->w || a + j < 0) continue;

                            int offset = (b + i) * screen->pitch + (a + j) * 4;
                            *(Uint32 *)&((Uint8 *)screen->pixels)[offset] = c;
                        }
                    }
                    font++;
                }
            }
        }
    } else {
        // full-width char
        unsigned short *font = (unsigned short *)kanjiaddr(ch);

        if (font) {
            for (i = 0; i < 15; i++) {
                for (j = 0; j < 16; j++) {
                    if (*font & (1 << (16 - j))) {
                        if (b + i >= screen->h || b + i < 0) continue;
                        if (a + j >= screen->w || a + j < 0) continue;

                        int offset = (b + i) * screen->pitch + (a + j) * 4;
                        *(Uint32 *)&((Uint8 *)screen->pixels)[offset] = c;
                    }
                }
                font++;
            }
        }
    }
}

void
DrawString (int a, int b, const char *x, Uint32 c)
{
    int len = (int)strlen(x);
    unsigned char *p = (unsigned char *)x;

    if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);

    while (len > 0) {
        if (*p == 'I' || *p == 'l') a -= 2;

        if (fontType == DX_FONTTYPE_EDGE) {
            DrawChar(p, a - 1, b - 1, 0);
            DrawChar(p, a, b - 1, 0);
            DrawChar(p, a + 1, b - 1, 0);
            DrawChar(p, a - 1, b, 0);
            DrawChar(p, a + 1, b, 0);
            DrawChar(p, a - 1, b + 1, 0);
            DrawChar(p, a, b + 1, 0);
            DrawChar(p, a + 1, b + 1, 0);
        }

        DrawChar(p, a, b, c);

        if (*p <= 0x7f) {
            a += 9;

            if (*p == '-') a += 3;
            else if (*p >= '0' && *p <= '9') a += 3;
            else if (*p == '?') a += 8;
            else if (*p >= 'A' && *p <= 'Z' && *p != 'I') a += 4;

            p++;
            len--;
        } else {
            a += 17;
            p += 2;
            len -= 2;
        }
    }

    if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
}

void
DrawFormatString (int a, int b, Uint32 color, const char *str, ...)
{
    va_list args;
    char *newstr = new char[strlen (str) + 16];
    va_start (args, str);
    vsprintf (newstr, str, args);
    va_end (args);
    DrawString (a, b, newstr, color);
    delete[]newstr;
}

//Key Aliases
#define KEY_INPUT_ESCAPE SDLK_ESCAPE

bool ex = false;

void
UpdateKeys ()
{
    SDL_Event event;
    while (SDL_PollEvent (&event))
    {
        switch (event.type)
        {
        case SDL_KEYDOWN:
            keysHeld[event.key.keysym.sym] = true;
            break;
        case SDL_KEYUP:
            keysHeld[event.key.keysym.sym] = false;
            break;
        case SDL_JOYAXISMOTION:
            if (event.jaxis.which == 0)
            {
                if (event.jaxis.axis == JOYSTICK_XAXIS)
                {
                    if (event.jaxis.value < -16383)
                        keysHeld[SDLK_LEFT] = true;
                    else if (event.jaxis.value > 16383)
                        keysHeld[SDLK_RIGHT] = true;
                    else
                    {
                        keysHeld[SDLK_LEFT] = false;
                        keysHeld[SDLK_RIGHT] = false;
                    }
                }
                else if (event.jaxis.axis == JOYSTICK_YAXIS)
                {
                    if (event.jaxis.value < -16383)
                        keysHeld[SDLK_UP] = true;
                    else if (event.jaxis.value > 16383)
                        keysHeld[SDLK_DOWN] = true;
                    else
                    {
                        keysHeld[SDLK_UP] = false;
                        keysHeld[SDLK_DOWN] = false;
                    }
                }
            }
            break;
        case SDL_QUIT:
            ex = true;
            break;
        }
    }
}

byte
ProcessMessage ()
{
    return ex;
}

byte
CheckHitKey (int key)
{
    if (key == SDLK_z && keysHeld[SDLK_SEMICOLON])
        return true;
    return keysHeld[key];
}

void
WaitKey ()
{
    while (true)
    {
		SDL_Delay (100);
		UpdateKeys ();

		for (int i = 0; i < SDLK_LAST; i++)
			if (keysHeld[i])
				return;

		if (SDL_JoystickGetButton (joystick, JOYSTICK_JUMP))
			return;

		if (ex)
			exit (0);
    }
}

void
DrawGraphZ (int a, int b, SDL_Surface * mx)
{
    if (mx)
    {
        SDL_Rect offset;
        offset.x = a;
        offset.y = b;
        SDL_BlitSurface (mx, NULL, screen, &offset);
    }
}

void
DrawTurnGraphZ (int a, int b, SDL_Surface * mx)
{
    if (mx && mx->format->BitsPerPixel == 32)
    {
        if (SDL_MUSTLOCK (screen)) SDL_LockSurface (screen);
        if (SDL_MUSTLOCK (mx)) SDL_LockSurface (mx);

        Uint32 *src = (Uint32 *) mx->pixels;
        Uint32 *dst = (Uint32 *) screen->pixels;

        int i, j;
        Uint8 rv = 0, gv = 0, bv = 0;
        Uint32 key = SDL_MapRGB (mx->format, 9 * 16 + 9, 255, 255);

        for (i = 0; i < mx->h; i++)
        {
            for (j = 0; j < mx->w; j++)
            {
                int x = a + j, y = b + i;
                if (x < 0 || y < 0 || x >= screen->w || y >= screen->h)
                    continue;

                Uint32 pixel = src[(i + 1) * mx->pitch / 4 - j - 1];
                if (pixel == key)
                    continue;

                SDL_GetRGB (pixel, mx->format, &rv, &gv, &bv);
                dst[y * screen->pitch / 4 + x] = SDL_MapRGB (screen->format, rv, gv, bv);
            }
        }

        if (SDL_MUSTLOCK (screen)) SDL_UnlockSurface (screen);
        if (SDL_MUSTLOCK (mx)) SDL_UnlockSurface (mx);
    }
}

void
DrawVertTurnGraph (int a, int b, SDL_Surface * mx)
{
    if (mx && mx->format->BitsPerPixel == 32)
    {
        if (SDL_MUSTLOCK (screen)) SDL_LockSurface (screen);
        if (SDL_MUSTLOCK (mx)) SDL_LockSurface (mx);

        Uint32 *src = (Uint32 *) mx->pixels;
        Uint32 *dst = (Uint32 *) screen->pixels;

        int i, j;
        Uint8 rv = 0, gv = 0, bv = 0;
        Uint32 key = SDL_MapRGB (mx->format, 9 * 16 + 9, 255, 255);

        for (i = 0; i < mx->h; i++)
        {
            for (j = 0; j < mx->w; j++)
            {
                int x = a + j, y = b + i;
                if (x < 0 || y < 0 || x >= screen->w || y >= screen->h)
                    continue;

                Uint32 pixel = src[(mx->h - i - 1) * mx->pitch / 4 + j];
                if (pixel == key)
                    continue;

                SDL_GetRGB (pixel, mx->format, &rv, &gv, &bv);
                dst[y * screen->pitch / 4 + x] = SDL_MapRGB (screen->format, rv, gv, bv);
            }
        }

        if (SDL_MUSTLOCK (screen)) SDL_UnlockSurface (screen);
        if (SDL_MUSTLOCK (mx)) SDL_UnlockSurface (mx);
    }
}

SDL_Surface *
DerivationGraph (int srcx, int srcy, int width, int height, SDL_Surface * src)
{
    SDL_Surface *img = SDL_CreateRGBSurface (SDL_SWSURFACE, width, height,
                       screen->format->BitsPerPixel,
                       src->format->Rmask,
                       src->format->Bmask,
                       src->format->Gmask,
                       src->format->Amask);

    SDL_Rect offset;
    offset.x = srcx;
    offset.y = srcy;
    offset.w = width;
    offset.h = height;

    SDL_BlitSurface (src, &offset, img, NULL);
    SDL_SetColorKey (img, SDL_SRCCOLORKEY,
                     SDL_MapRGB (img->format, 9 * 16 + 9, 255, 255));
    return img;
}

//Noticably different than the original
SDL_Surface *
LoadGraph (const char *filename)
{
    SDL_Surface *image = SDL_LoadBMP (filename);

    if (image)
        return image;
    fprintf (stderr, "Error: Unable to load %s: %s\n", filename,
             SDL_GetError ());
    exit (1);
}

void
PlaySoundMem (Mix_Chunk * s, int l)
{
    if (sound)
        Mix_PlayChannel (-1, s, l);
}

Mix_Chunk *
LoadSoundMem (const char *f)
{
    if (!sound)
        return NULL;

    Mix_Chunk *s = Mix_LoadWAV (f);
    if (s)
        return s;
    fprintf (stderr, "Error: Unable to load sound %s: %s\n", f,
             Mix_GetError ());
    return NULL;
}

Mix_Music *
LoadMusicMem (const char *f)
{
    if (!sound)
        return NULL;

    Mix_Music *m = Mix_LoadMUS (f);
    if (m)
        return m;
    fprintf (stderr, "Error: Unable to load music %s: %s\n", f,
             Mix_GetError ());
    return NULL;
}
