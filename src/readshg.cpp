//*****************************************************************************
// Copyright:   (c) Petr Smilauer
// Author:      Petr Smilauer
// Modified by: Wlodzimiez ABX Skiba 2003/2004 Unicode support
//              Ron Lee
// Created:     1/1/1999
// License:     wxWindows license
// Description:
//   Petr Smilauer's .SHG (Segmented Hypergraphics file) reading code.
// Note: .SHG is undocumented so this is reverse-engineering and guesswork.
//*****************************************************************************

#include "readshg.h"
#include "tex2any.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace std;

//*****************************************************************************
// Returns a vector of hotspots.
// E.g.
// vector<HotSpot> HotSpots;
// HotSpots = ParseSHG("thing.shg");
//*****************************************************************************
vector<HotSpot> ParseSHG(const wxString& FileName)
{
  vector<HotSpot> HotSpots;

  FILE* fSHG = wxFopen(FileName, _T("rb"));
  if (fSHG == 0)
  {
    return HotSpots;
  }

  long offset;

  // Look at offset OFF_OFFSET to get another offset :-)
  fseek(fSHG, OFF_OFFSET, SEEK_SET);

  // Initialize the whole 4-byte variable
  offset = 0L;

  // Get the offset in first two bytes.
  fread(&offset, 2, 1, fSHG);

  // If the offset is zero, used next DWORD field.
  if (offset == 0)
  {
    // This is our offset for very large DIB.
    fread(&offset, 4, 1, fSHG);
  }

  // Don't know how this delta comes-about.
  offset += 9;

  if (fseek(fSHG, offset, SEEK_SET) != 0)
  {
    // This condition is probably due to an incorrect offset calculation.
    fclose(fSHG);
    return HotSpots;
  }

  int HotSpotCount = 0;
  fread(&HotSpotCount, 2, 1, fSHG);

  HotSpots.resize(HotSpotCount);

  int nMacroStrings = 0;

  // We can ignore the macros, as this is repeated later, but we need to know
  // how much to skip.
  fread(&nMacroStrings, 2, 1, fSHG);

  // Skip another 2 bytes I do not understand.
  fseek(fSHG, 2, SEEK_CUR);

  ShgInfoBlock  sib;
  for (int i = 0 ; i < HotSpotCount ; ++i)
  {
    // Read one hotspot's information.
    fread(&sib, sizeof(ShgInfoBlock), 1, fSHG);

    // Analyse the data.
    HotSpots[i].type    = (HotspotType)(sib.hotspotType & 0xFB);
    HotSpots[i].left    = sib.left;
    HotSpots[i].top     = sib.top;
    HotSpots[i].right   = sib.left + sib.width;
    HotSpots[i].bottom  = sib.top  + sib.height;
    HotSpots[i].IsVisible = ((sib.hotspotType & 4) == 0);
    HotSpots[i].szHlpTopic_Macro[0] = '\0';
  }

  // we have it...now read-off the macro-string block.
  if(nMacroStrings > 0)
  {
    // nMacroStrings is the byte offset.
    fseek(fSHG, nMacroStrings, SEEK_CUR);
  }

  // Read through the strings: hotspot-id[ignored], then topic/macro.
  int c;
  for (int i = 0 ; i < HotSpotCount ; ++i)
  {
    while ((c = fgetc(fSHG)) != 0)
      ;

    // Now read it.
    int j = 0;
    while ((c = fgetc(fSHG)) != 0)
    {
      HotSpots[i].szHlpTopic_Macro[j] = (wxChar)c;
      ++j;
    }
    HotSpots[i].szHlpTopic_Macro[j] = 0;
  }

  fclose(fSHG);

  return HotSpots;
}


//*****************************************************************************
// Convert Windows .SHG file to HTML map file
//*****************************************************************************
bool SHGToMap(const wxString& FileName, const wxString& DefaultFile)
{
  // Test the SHG parser
  vector<HotSpot> HotSpots = ParseSHG(FileName);
  if (HotSpots.empty())
  {
    return false;
  }

  wxString Message;
  Message
    << "Converting .SHG file to HTML map file: there are " << HotSpots.size()
    << " hotspots in " << FileName;
  OnInform(Message);

  wxChar outBuf[256];
  wxStrcpy(outBuf, FileName);
  StripExtension(outBuf);
  wxStrcat(outBuf, _T(".map"));

  FILE* fd = wxFopen(outBuf, _T("w"));
  if (!fd)
  {
    OnError("Could not open .map file for writing.");
    return false;
  }

  wxFprintf(fd, _T("default %s\n"), DefaultFile);
  for (unsigned i = 0; i < HotSpots.size(); ++i)
  {
    wxString refFilename = "??";

    TexRef *texRef = FindReference(HotSpots[i].szHlpTopic_Macro);
    if (texRef)
    {
      refFilename = texRef->refFile;
    }
    else
    {
      Message.clear();
      Message
        << "Warning: could not find hotspot reference "
        << HotSpots[i].szHlpTopic_Macro;
      OnInform(Message);
    }
    wxFprintf(
      fd,
      _T("rect %s %d %d %d %d\n"),
      refFilename,
      (int)HotSpots[i].left,
      (int)HotSpots[i].top,
      (int)HotSpots[i].right,
      (int)HotSpots[i].bottom);
  }
  wxFprintf(fd, "\n");

  fclose(fd);

  return true;
}
