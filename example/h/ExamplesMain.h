/*
 * This file is part of the Airplay SDK Code Samples.
 *
 * Copyright (C) 2001-2010 Ideaworks3D Ltd.
 * All Rights Reserved.
 *
 * This source code is intended only as a supplement to Ideaworks Labs
 * Development Tools and/or on-line documentation.
 *
 * THIS CODE AND INFORMATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
 * KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
 * PARTICULAR PURPOSE.
 */
// Examples main header
//-----------------------------------------------------------------------------

#ifndef EXAMPLES_MAIN_H
#define EXAMPLES_MAIN_H

#include "IwGeom.h"
#include "s3ePointer.h"
#include "s3eKeyboard.h"

typedef enum CursorKeyCodes
{
    EXCURSOR_NONE = 0,
    EXCURSOR_UP,
    EXCURSOR_DOWN,
    EXCURSOR_LEFT,
    EXCURSOR_RIGHT
} CursorKeyCodes;

typedef void (*exbutton_handler)();

typedef struct ExButtons
{
    char             name[64];
    int              x;
    int              y;
    int              w;
    int              h;
    s3eKey           key;
    int32            key_state;
    exbutton_handler handler;
    ExButtons*       next;
    ExButtons()
    {
        name[0] = '\0';
        x = 0;
        y = 0;
        w = 0;
        h = 0;
        key = s3eKeyFirst;
        key_state = 0;
        handler = NULL;
        next = NULL;
    }
} ExButtons;

extern "C" int AddButton(const char* text, int x, int y, int w, int h, s3eKey key, exbutton_handler handler = NULL);
extern "C" void DeleteButtons();
extern "C" void RenderButtons();
extern "C" void RemoveButton(const char* text);
extern "C" int32 CheckButton(const char* text);
extern "C" CursorKeyCodes CheckCursorState();
extern "C" void RenderSoftkeys();
extern "C" void RenderCursor();

// Allocate (and configure) a vertex stream for rendering a 'fullscreen' backdrop that
// does not obscure the Ideaworks logo & softkeys
CIwSVec2* AllocClientScreenRectangle();
void DisplayMessage(const char* strmessage);
extern "C" int RenderActionkey(const char* text, int x, int y, void (*handler)() = NULL);
extern "C" void RenderCursorskeys();

#endif /* !EXAMPLES_MAIN_H */
