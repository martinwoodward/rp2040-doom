//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021-2022 Graham Sanderson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     SDL implementation of system-specific input interface.
//


#include "doomkeys.h"
#include "doomtype.h"
#include "d_event.h"
#include "i_input.h"
#include "m_argv.h"
#include "m_config.h"

// Disallow mouse and joystick movement to cause forward/backward
// motion.  Specified with the '-novert' command line parameter.
// This is an int to allow saving to config file
int novert = 0;

// If true, keyboard mapping is ignored, like in Vanilla Doom.
// The sensible thing to do is to disable this if you have a non-US
// keyboard.

int vanilla_keyboard_mapping = true;

// Mouse acceleration
//
// This emulates some of the behavior of DOS mouse drivers by increasing
// the speed when the mouse is moved fast.
//
// The mouse input values are input directly to the game, but when
// the values exceed the value of mouse_threshold, they are multiplied
// by mouse_acceleration to increase the speed.
float mouse_acceleration = 2.0;
int mouse_threshold = 10;



void I_StartTextInput(int x1, int y1, int x2, int y2)
{
}

void I_StopTextInput(void)
{
}

//
// Read the change in mouse state to generate mouse motion events
//
// This is to combine all mouse movement for a tic into one mouse
// motion event.
void I_ReadMouse(void)
{
}

// Bind all variables controlling input options.
void I_BindInputVariables(void)
{
    M_BindFloatVariable("mouse_acceleration",      &mouse_acceleration);
    M_BindIntVariable("mouse_threshold",           &mouse_threshold);
    M_BindIntVariable("vanilla_keyboard_mapping",  &vanilla_keyboard_mapping);
    M_BindIntVariable("novert",                    &novert);
}
