// akaux.cc: the screens around the game: the greeting, the instructions (from AK.DAT), and the
// scrolling quotes and credits. Compiled -O0 (docs/MATCHING.md).

#include <stdio.h>
#include <string.h>
#include <allegro.h>
#include "ak.h"
#include "akaux.h"

// Only checks that AK.DAT can be loaded (the datafile is not kept).
akaux::akaux()                                          // 003D08
{
    DATAFILE *dat;
    dat = load_datafile("ak.dat");
    if (dat == NULL)
    {
        allegro_exit();
        printf("Error Loading AK.DAT!\r\n");
        printf("Please Reinstall Akalabeth.\r\n");
        exit(1);
    }
}

akaux::~akaux()                                         // 003D5C
{
}

// "Ready?": N quits.
void akaux::hello()                                     // 003DB4
{
    int key;
    textout(screen, font, "Ultima 0 - Akalabeth!", 48, 88, 11);
    textout(screen, font, "Ready? ", 24, 160, 11);
    key = readkey() & 0xff;
    if (key == 'n' | key == 'N')
    {
        textout(screen, font, "Then Leave and Begone!", 24, 168, 11);
        readkey();
        text_mode(0);
        exit(1);
    }
}

// The instructions: AK.DAT's text, one line per ';', a new page at each line holding '~'.
void akaux::instruct()                                  // 003E88
{
    int row = 0;
    DATAFILE *dat;
    char *text;
    char **rest;
    char *line;
    clear(screen);
    dat = load_datafile("ak.dat");
    if (dat == NULL)
    {
        allegro_exit();
        printf("Error Loading AK.DAT!\r\n");
        printf("Please Reinstall Akalabeth.\r\n");
        exit(1);
    }
    text = (char *)dat[AKINS].dat;
    rest = &text;
    while ((line = strsep(rest, ";")) != NULL)
    {
        if (strchr(line, '~') == NULL)
            textout(screen, font, line, 0, row++ * 12, 11);
        else
        {
            textout_centre(screen, font, "Press any Key to Continue", 160, (row + 1) * 12, 11);
            readkey();
            clear(screen);
            row = 0;
        }
    }
    unload_datafile(dat);
}

// The quotes: they scroll up from the bottom of the screen, then on until the last line is gone
// or a key is pressed.
void akaux::quotes()                                    // 004844
{
    char *quote[] = {
        " A SMATTERiNG OF QUOTES FROM ",
        "     THE AKALABETH TEAM      ",
        "                             ",
        "                             ",
        "I have been searching around ",
        "and correct me if I am wrong,",
        "but there is not a PC version",
        "        of Akalabeth.        ",
        "  -JUiCEY Bird in an E-Mail  ",
        "       to Lord British       ",
        "                             ",
        "                             ",
        "   And remember if you say   ",
        "  anything stupid it WILL be ",
        " used in the quotes section. ",
        "           -Anon             ",
        "                             ",
        "                             ",
        "  S for stats? What is this? ",
        "          Wizardry?          ",
        "          -Tortuga           ",
        "                             ",
        "                             ",
        "            Great!           ",
        "   -LB reponding to a three  ",
        "     paragraph E-Mail from   ",
        "         JUiCEY Bird         ",
        "                             ",
        "                             ",
        "Just what exactly do you plan",
        "     on doing with this?     ",
        "   -Brian Martin inquiring   ",
        "       about Akalabeth       ",
        "                             ",
        "                             ",
        "F*** Computer Science!  We've",
        "   gotta finish Akalabeth!   ",
        "   -Tortuga & JUiCEY Bird    ",
        "                             ",
        "                             ",
        " I want your version Marcus! ",
        "    These guys sound too     ",
        "       overconfident.        ",
        "    -A foolish dragon who    ",
        "   underestimated the power  ",
        "      of A JUiCEY Bird       ",
        "                             ",
        "                             ",
        "I could have written that in ",
        "         five seconds.       ",
        "            -Jake            ",
        "                             ",
        "                             ",
        "  It came in a ziplock bag.  ",
        "  With an eight page zeroxed ",
        "      2 x 3 manual.      ",
        "-Richard Garriott commenting ",
        "  on the original package of ",
        "          Akalabeth          ",
        "                             ",
        "                             ",
        "    This compiler is crap!   ",
        "  -Jake Commenting on DjGPP  ",
        "                             ",
        "                             ",
        "  I'm gonna kick your ass!   ",
        "  -Tortuga fighting JUiCEY   ",
        "   Bird for an Ultima Book   ",
        "                             ",
        "                             ",
        "Where did they have dungeons ",
        "        back in 1979?        ",
        "     -JUiCEY Bird's Mom      ",
        "                             ",
        "                             ",
        "If LB saw this dungeon right ",
        "   now he'd kick our a$$es!  ",
        "  -Tortuga commenting on the ",
        "  first attempt at a dungeon ",
        "                             ",
        "                             ",
        "        Another GPF?         ",
        "            -Jake            ",
        "                             ",
        "                             ",
        "  It's supposed to do that,  ",
        "       it's a feature.       ",
        "           -Corey            ",
        "                             ",
        "                             ",
        " Whose idea was it to write  ",
        "     this in C++ anyway?     ",
        "           -Corey            ",
        "                             ",
    };
    int first = 0, last = 0, height = 200;
    int i, step, shift;
    clear(screen);
    for (step = 200; step >= 0; step--)
    {
        for (i = 0; i <= last; i++)
            textout_centre(screen, font, quote[i], 160, i * 8 + step, 11);
        if (step % 8 == 0)
            last++;
        rest(100);
        if (keypressed()) break;
    }
    first = 0;
    last = 26;
    step = 1;
    shift = 0;
    while (!keypressed())
    {
        for (i = first; i <= last; i++)
            if (i <= 93)
                textout_centre(screen, font, quote[i], 160, (i - first) * 8 + shift, 11);
        shift--;
        if (step % 8 == 0)
        {
            first++;
            last++;
            shift = 0;
        }
        rest(100);
        step++;
        if (last > 119) break;
    }
    clear_keybuf();
}

// The credits, scrolled like the quotes.
void akaux::shoutouts()                                 // 004D30
{
    char *credit[] = {
        "  Akalabeth PC-Conversion   ",
        "                            ",
        "                            ",
        "Written by Richard Garriott ",
        "                            ",
        "                            ",
        "                            ",
        "      PC-Conversion by      ",
        "         Corey Roth         ",
        "  with special assitance by ",
        "       Nathan Tucker        ",
        "                            ",
        "                            ",
        "                            ",
        "                            ",
        "                            ",
        " Greets and Thanx go out to ",
        "        Jake Groshong       ",
        "        Ronny McCrory       ",
        "        Adrian Dilley       ",
        "        Russell Kabir       ",
        "        James Ashley        ",
        "         Chris Harjo        ",
        "and everyone else out there ",
        "     who helped me out.     ",
        "                            ",
        "                            ",
        "                            ",
        "                            ",
        "                            ",
        "This game made possible by: ",
        "                            ",
        "           Origin           ",
        "            and             ",
        "  JUiCEY Bird Productions   ",
        "                            ",
        0,
    };
    int first = 0, last = 0, height = 200;
    int i, step, shift;
    clear(screen);
    for (step = 200; step >= 0; step--)
    {
        for (i = 0; i <= last; i++)
            textout_centre(screen, font, credit[i], 160, i * 8 + step, 11);
        if (step % 8 == 0)
            last++;
        rest(100);
        if (keypressed()) break;
    }
    first = 0;
    last = 26;
    step = 1;
    shift = 0;
    while (!keypressed())
    {
        for (i = first; i <= last; i++)
            if (i <= 36)
                textout_centre(screen, font, credit[i], 160, (i - first) * 8 + shift, 11);
        shift--;
        if (step % 8 == 0)
        {
            first++;
            last++;
            shift = 0;
        }
        rest(100);
        step++;
        if (last > 62) break;
    }
    clear_keybuf();
}
