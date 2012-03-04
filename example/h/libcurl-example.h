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

#include "s3eDebug.h"
#include "s3eDevice.h"
#include "s3eFile.h"
#include "s3eKeyboard.h"
#include "s3eTimer.h"
#include "s3eSurface.h"

// Attempt to lock to 25 frames per second
#define MS_PER_FRAME (1000 / 25)

// Externs for functions which examples must implement
void ExampleInit();
void ExampleShutDown();
void ExampleRender();
bool ExampleUpdate();

