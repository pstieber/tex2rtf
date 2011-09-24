//*****************************************************************************
// Copyright:   (c) Petr Smilauer
// Author:      Petr Smilauer
// Created:     1/1/1999
// License:     wxWindows license
// Description:
//   Petr Smilauer's .SHG (Segmented Hypergraphics file) reading code.
// Note: .SHG is undocumented so this is reverse-engineering and guesswork.
//*****************************************************************************

#ifndef readshgh
#define readshgh

#include <wx/string.h>

typedef enum
{
  TypePopup = 0xE2,
  TypeJump = 0xE3,
  TypeMacro = 0xC8
} HotspotType;

#define NOT_VISIBLE  0x04

typedef struct
{
  unsigned char   hotspotType;// combines HotspotType /w NOT_VISIBLE if appropriate
  unsigned char   flag;       // NOT_VISIBLE or 0 ??
  unsigned char   skip;       // 0, always??
  unsigned short  left,
                  top,
                  width,      // left+width/top+height give right/bottom,
                  height;     // =>right and bottom edge are not 'included'
  unsigned char   magic[4];   // wonderful numbers: for macros, this seems
    // (at least first 2 bytes) to represent offset into macro-strings block.
} ShgInfoBlock;   // whole block is just 15 bytes long. How weird!

#define OFF_OFFSET    0x20    // this is offset, where WORD (?) lies
#define OFFSET_DELTA  9       // we must add this to get real offset from file beginning

struct HotSpot
{
  HotspotType type;
  unsigned int left, top, right, bottom;
  wxChar szHlpTopic_Macro[65];
  bool IsVisible;
};

// Converts Windows .SHG file to HTML map file
extern bool SHGToMap(const wxString& FileName, const wxString& DefaultFile);

#endif
