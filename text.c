#include "text.h"
#define CHARS_IN_FONT 37
#define ALPHABET_LEN 26
#include <stdbool.h>


Font *loadFont(char *fileName, SDL_Surface *screen, Color background, Color textColor, int size) {
    Font *font = (Font *) malloc(sizeof(Font));
    font->screen = screen;
    font->size = size;
    font->textColor = textColor;
    font->backgroundColor = background;
    font->font = loadPPM(fileName);
    if(!font->font) {
        free(font);
        return NULL;
    }
    return font;
}
void freeFont(Font *font) {
    free(font->font);
    free(font);
}

void writeText(Font *font, char *text, int x, int y) {
    int letterHeight = font->font->height / CHARS_IN_FONT;
    int letterWidth = font->font->width;
    int letterIdx;
    for(int i = 0; text[i]; i++) {
        if('a' <= text[i] && text[i] <= 'z') {
            letterIdx = text[i] - 'a';
        }
        else if('A' <= text[i] && text[i] <= 'Z') {
            letterIdx = text[i] - 'A';
        }
        else if('0' <= text[i] && text[i] <= '9') {
            letterIdx = ALPHABET_LEN + text[i] - '0';
        } else if(text[i] == '.') {
            letterIdx = CHARS_IN_FONT - 1;
        } else {
            letterIdx = CHARS_IN_FONT - 1;
        }
        
        drawSubImage(font->font, font->screen,
        x + i * letterWidth, y, 0, letterIdx * letterHeight,
        letterWidth, letterHeight, &font->backgroundColor,
        false, 0);
    }
}
void clearText(Font *font, char *text, int x, int y) {
    
}

