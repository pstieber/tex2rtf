/////////////////////////////////////////////////////////////////////////////
// Name:        texutils.cpp
// Purpose:     Miscellaneous utilities
// Author:      Julian Smart
// Modified by: Wlodzimiez ABX Skiba 2003/2004 Unicode support
//              Ron Lee
// Created:     7.9.93
// RCS-ID:      $Id: texutils.cpp 47536 2007-07-17 22:47:30Z VZ $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include <wx/app.h>
#include <wx/filename.h>
#include <wx/hash.h>
#include <wx/hashmap.h>
#include <wx/textfile.h>

#include <iostream>
#include <fstream>

using namespace std;

#include <ctype.h>
#include "tex2any.h"

#if !WXWIN_COMPATIBILITY_2_4
static inline wxChar* copystring(const wxChar* s)
    { return wxStrcpy(new wxChar[wxStrlen(s) + 1], s); }
#endif

TexReferenceMap TexReferences;
BibMap BibList;
StringSet CitationList;
ColourTableMap ColourTable;
wxHashTable BibStringTable(wxKEY_STRING);
MacroMap CustomMacroMap;
TexChunk *currentSection = NULL;
wxString fakeCurrentSection;

static long BibLine = 1;

void OutputCurrentSection(void)
{
  if (!fakeCurrentSection.empty())
  {
    TexOutput(fakeCurrentSection);
  }
  else if (currentSection)
  {
    TraverseChildrenFromChunk(currentSection);
  }
}

// Nasty but the way things are done now, necessary,
// in order to output a chunk properly to a string (macros and all).
void OutputCurrentSectionToString(wxChar *buf)
{
  if (!fakeCurrentSection.empty())
  {
    wxStrcpy(buf, fakeCurrentSection);
  }
  else
  {
    OutputChunkToString(currentSection, buf);
  }
}

void OutputChunkToString(TexChunk *chunk, wxChar *buf)
{
  FILE *tempfd = wxFopen(_T("tmp.tmp"), _T("w"));
  if (!tempfd)
    return;

  FILE *old1 = CurrentOutput1;
  FILE *old2 = CurrentOutput2;

  CurrentOutput1 = tempfd;
  CurrentOutput2 = NULL;

  TraverseChildrenFromChunk(chunk);

  CurrentOutput1 = old1;
  CurrentOutput2 = old2;

  fclose(tempfd);

  // Read from file into string
  tempfd = wxFopen(_T("tmp.tmp"), _T("r"));
  if (!tempfd)
    return;

  buf[0] = 0;
  int ch = -2;
  int i = 0;
  while (ch != EOF)
  {
    ch = getc(tempfd);
    if (ch == EOF)
      buf[i] = 0;
    else
    {
      buf[i] = (wxChar)ch;
      i ++;
    }
  }
  fclose(tempfd);
  wxRemoveFile(_T("tmp.tmp"));
}

// Called by Tex2Any to simulate a section
void FakeCurrentSection(const wxString& fakeSection, bool addToContents)
{
  currentSection = NULL;
  fakeCurrentSection = fakeSection;

  if (DocumentStyle == LATEX_ARTICLE)
  {
    int mac = ltSECTIONHEADING;
    if (!addToContents)
      mac = ltSECTIONHEADINGSTAR;
    OnMacro(mac, 0, true);
    OnMacro(mac, 0, false);
  }
  else
  {
    int mac = ltCHAPTERHEADING;
    if (!addToContents)
      mac = ltCHAPTERHEADINGSTAR;
    OnMacro(mac, 0, true);
    OnMacro(mac, 0, false);
  }
}

// Look for \label macro, use this ref name if found or
// make up a topic name otherwise.
static long topicCounter = 0;

void ResetTopicCounter(void)
{
  topicCounter = 0;
}

static wxChar *forceTopicName = NULL;

void ForceTopicName(const wxChar *name)
{
  if (forceTopicName)
    delete[] forceTopicName;
  if (name)
    forceTopicName = copystring(name);
  else
    forceTopicName = NULL;
}

wxChar *FindTopicName(TexChunk *chunk)
{
  if (forceTopicName)
    return forceTopicName;

  wxChar *topicName = NULL;
  static wxChar topicBuf[100];

  if (chunk && (chunk->type == CHUNK_TYPE_MACRO) &&
      (chunk->macroId == ltLABEL))
  {
    list<TexChunk*>::iterator iNode = chunk->mChildren.begin();
    if (iNode != chunk->mChildren.end())
    {
      TexChunk* child = *iNode;
      if (child->type == CHUNK_TYPE_ARG)
      {
        list<TexChunk*>::iterator iNode2 = child->mChildren.begin();
        if (iNode2 != child->mChildren.end())
        {
          TexChunk* schunk = *iNode2;
          if (schunk->type == CHUNK_TYPE_STRING)
          {
            topicName = schunk->value;
          }
        }
      }
    }
  }
  if (topicName)
    return topicName;
  else
  {
    wxSnprintf(topicBuf, sizeof(topicBuf), _T("topic%ld"), topicCounter);
    topicCounter ++;
    return topicBuf;
  }
}

/*
 * Simulate argument data, so we can 'drive' clients which implement
 * certain basic formatting behaviour.
 * Snag is that some save a TexChunk, so don't use yet...
 *
 */

void StartSimulateArgument(wxChar *data)
{
  currentArgData = data;
  haveArgData = true;
}

void EndSimulateArgument(void)
{
  haveArgData = false;
}

/*
 * Parse and convert unit arguments to points
 *
 */

int ParseUnitArgument(wxString& unitArg)
{
  float conversionFactor = 1.0;
  float unitValue = 0.0;
  size_t len = unitArg.length();

  // Get rid of any accidentally embedded commands
  for (size_t i = 0; i < len; i++)
    if (unitArg[i] == '\\')
      unitArg[i] = 0;
  len = unitArg.length();

  if ((len > 0) && (isdigit(unitArg[0]) || unitArg[0] == '-'))
  {
    wxSscanf(unitArg, _T("%f"), &unitValue);
    if (len > 1)
    {
      wxChar units[3];
      units[0] = unitArg[len-2];
      units[1] = unitArg[len-1];
      units[2] = 0;
      if (wxStrcmp(units, _T("in")) == 0)
        conversionFactor = 72.0;
      else if (wxStrcmp(units, _T("cm")) == 0)
        conversionFactor = (float)72.0/(float)2.51;
      else if (wxStrcmp(units, _T("mm")) == 0)
        conversionFactor = (float)72.0/(float)25.1;
      else if (wxStrcmp(units, _T("pt")) == 0)
        conversionFactor = 1;
    }
    return (int)(unitValue*conversionFactor);
  }
  else return 0;
}

// Strip off any extension (dot something) from end of file, if one exists.
// Inserts zero into pFileName.
void StripExtension(wxChar* pFileName)
{
  size_t len = wxStrlen(pFileName);
  size_t i = len-1;
  while (i > 0)
  {
    if (pFileName[i] == '.')
    {
      pFileName[i] = 0;
      break;
    }
    --i;
  }
}

void StripExtension(wxString& String)
{
  wxFileName FileName(String);
  FileName.ClearExt();
  String = FileName.GetFullPath();
}

/*
 * Latex font setting
 *
 */

void SetFontSizes(int pointSize)
{
  switch (pointSize)
  {
    case 12:
    {
      normalFont = 12;
      smallFont = 10;
      tinyFont = 8;
      largeFont1 = 14;
      LargeFont2 = 16;
      LARGEFont3 = 20;
      hugeFont1 = 24;
      HugeFont2 = 28;
      HUGEFont3 = 32;
      break;
    }
    case 11:
    {
      normalFont = 11;
      smallFont = 9;
      tinyFont = 7;
      largeFont1 = 13;
      LargeFont2 = 16;
      LARGEFont3 = 19;
      hugeFont1 = 22;
      HugeFont2 = 26;
      HUGEFont3 = 30;
      break;
    }
    case 10:
    {
      normalFont = 10;
      smallFont = 8;
      tinyFont = 6;
      largeFont1 = 12;
      LargeFont2 = 14;
      LARGEFont3 = 18;
      hugeFont1 = 20;
      HugeFont2 = 24;
      HUGEFont3 = 28;
      break;
    }
  }
}


/*
 * Latex references
 *
 */

void AddTexRef(
  const wxString& name,
  const wxString& file,
  const wxString& sectionName,
  int chapter,
  int section,
  int subsection,
  int subsubsection)
{
  wxChar buf[100];
  buf[0] = 0;
/*
  if (sectionName)
  {
    wxStrcat(buf, sectionName);
    wxStrcat(buf, " ");
  }
*/
  if (chapter)
  {
    wxChar buf2[10];
    wxSnprintf(buf2, sizeof(buf2), _T("%d"), chapter);
    wxStrcat(buf, buf2);
  }
  if (section)
  {
    wxChar buf2[10];
    if (chapter)
      wxStrcat(buf, _T("."));

    wxSnprintf(buf2, sizeof(buf2), _T("%d"), section);
    wxStrcat(buf, buf2);
  }
  if (subsection)
  {
    wxChar buf2[10];
    wxStrcat(buf, _T("."));
    wxSnprintf(buf2, sizeof(buf2), _T("%d"), subsection);
    wxStrcat(buf, buf2);
  }
  if (subsubsection)
  {
    wxChar buf2[10];
    wxStrcat(buf, _T("."));
    wxSnprintf(buf2, sizeof(buf2), _T("%d"), subsubsection);
    wxStrcat(buf, buf2);
  }
  wxChar *tmp = ((wxStrlen(buf) > 0) ? buf : (wxChar *)NULL);
  TexReferences[name] = new TexRef(name, file, tmp, sectionName);
}

void WriteTexReferences(wxChar *filename)
{
    wxString name = filename;
    wxTextFile file;

    if (!(wxFileExists(name)?file.Open(name):file.Create(name)))
        return;

    file.Clear();

    for (TexReferenceMap::iterator iTexRef = TexReferences.begin(); iTexRef != TexReferences.end(); ++iTexRef)
    {
        Tex2RTFYield();
        TexRef *ref = iTexRef->second;
        wxString converter = ref->refLabel;
        converter << wxT(" ");
        converter << (!ref->refFile.empty() ? ref->refFile : _T("??"));
        converter << wxT(" ");
        converter << (!ref->sectionName.empty() ? ref->sectionName : _T("??")) ;
        converter << wxT(" ");
        converter << (!ref->sectionNumber.empty() ? ref->sectionNumber : _T("??")) ;
        file.AddLine(converter);

        if (!ref->sectionNumber || (wxStrcmp(ref->sectionNumber, _T("??")) == 0 && wxStrcmp(ref->sectionName, _T("??")) == 0))
        {
            wxChar buf[200];
            wxSnprintf(buf, sizeof(buf), _T("Warning: reference %s not resolved."), ref->refLabel);
            OnInform(buf);
        }
    }

    file.Write();
    file.Close();
}

void ReadTexReferences(wxChar *filename)
{
    wxString name = filename;

    if (!wxFileExists(name))
        return;

    wxTextFile file;
    if (!file.Open(name))
        return;

    wxString line;
    for ( line = file.GetFirstLine(); !file.Eof(); line = file.GetNextLine() )
    {
        wxString labelStr = line.BeforeFirst(wxT(' '));
        line = line.AfterFirst(wxT(' '));
        wxString fileStr  = line.BeforeFirst(wxT(' '));
        line = line.AfterFirst(wxT(' '));
        wxString sectionNameStr = line.BeforeFirst(wxT(' '));
        wxString sectionStr = line.AfterFirst(wxT(' '));

        // gt - needed to trick the hash table "TexReferences" into deleting the key
        // strings it creates in the Put() function, but not the item that is
        // created here, as that is destroyed elsewhere.  Without doing this, there
        // were massive memory leaks
        TexReferenceMap::iterator iTexRef = TexReferences.find(labelStr.c_str());
        if (iTexRef != TexReferences.end())
        {
            delete iTexRef->second;
        }

        TexReferences[labelStr.c_str()] = new TexRef(
            labelStr.c_str(),
            fileStr.c_str(),
            sectionStr.c_str(),
            sectionNameStr.c_str());
    }
}


/*
 * Bibliography-handling code
 *
 */

void BibEatWhiteSpace(wxString& line)
{
    while(!line.empty() && (line[0] == _T(' ') || line[0] == _T('\t') || line[0] == (wxChar)EOF))
    {
        if (line[0] == 10)
            BibLine ++;
        line = line.substr(1);
    }

    // Ignore end-of-line comments
    if ( !line.empty() && (line[0] == _T('%') || line[0] == _T(';') || line[0] == _T('#')))
    {
        line.clear();
    }
}

void BibEatWhiteSpace(istream& str)
{
  char ch = (char)str.peek();

  while (!str.eof() && (ch == ' ' || ch == '\t' || ch == 13 || ch == 10 || ch == (char)EOF))
  {
    if (ch == 10)
      BibLine ++;
    str.get(ch);
    if ((ch == (char)EOF) || str.eof()) return;
    ch = (char)str.peek();
  }

  // Ignore end-of-line comments
  if (ch == '%' || ch == ';' || ch == '#')
  {
    str.get(ch);
    ch = (char)str.peek();
    while (ch != 10 && ch != 13 && !str.eof())
    {
      str.get(ch);
      ch = (char)str.peek();
    }
    BibEatWhiteSpace(str);
  }
}

// Read word up to { or , or space
wxString BibReadWord(wxString& line)
{
    wxString val;

    while (!line.empty() &&
           line[0] != _T('\t') &&
           line[0] != _T(' ') &&
           line[0] != _T('{') &&
           line[0] != _T('(') &&
           line[0] != _T(',') &&
           line[0] != _T('='))
    {
        val << line[0];
        line = line.substr(1);
    }
    return val;
}

void BibReadWord(istream& istr, wxChar *buffer)
{
  int i = 0;
  buffer[i] = 0;
  char ch = (char)istr.peek();
  while (!istr.eof() && ch != ' ' && ch != '{' && ch != '(' && ch != 13 && ch != 10 && ch != '\t' &&
         ch != ',' && ch != '=')
  {
    istr.get(ch);
    buffer[i] = ch;
    i ++;
    ch = (char)istr.peek();
  }
  buffer[i] = 0;
}

// Read string (double-quoted or not) to end quote or EOL
wxString BibReadToEOL(wxString& line)
{
    if(line.empty())
        return wxEmptyString;

    wxString val;
    bool inQuotes = false;
    if (line[0] == _T('"'))
    {
        line = line.substr(1);
        inQuotes = true;
    }
    // If in quotes, read white space too. If not,
    // stop at white space or comment.
    while (!line.empty() && line[0] != _T('"') &&
           (inQuotes || ((line[0] != _T(' ')) && (line[0] != 9) &&
                          (line[0] != _T(';')) && (line[0] != _T('%')) && (line[0] != _T('#')))))
    {
        val << line[0];
        line = line.substr(1);
    }
    if (!line.empty() && line[0] == '"')
        line = line.substr(1);

    return val;
}

void BibReadToEOL(istream& istr, wxChar *buffer)
{
    int i = 0;
    buffer[i] = 0;
    char ch = (char)istr.peek();
    bool inQuotes = false;
    if (ch == '"')
    {
        istr.get(ch);
        ch = (char)istr.peek();
        inQuotes = true;
    }
    // If in quotes, read white space too. If not,
    // stop at white space or comment.
    while (!istr.eof() && ch != 13 && ch != 10 && ch != _T('"') &&
           (inQuotes || ((ch != _T(' ')) && (ch != 9) &&
                          (ch != _T(';')) && (ch != _T('%')) && (ch != _T('#')))))
    {
        istr.get(ch);
        buffer[i] = ch;
        i ++;
        ch = (char)istr.peek();
    }
    if (ch == '"')
        istr.get(ch);
    buffer[i] = 0;
}

// Read }-terminated value, taking nested braces into account.
wxString BibReadValue(wxString& line,
                      bool ignoreBraces = true,
                      bool quotesMayTerminate = true)
{
    wxString val;
    int braceCount = 1;
    bool stopping = false;

    if (line.length() >= 4000)
    {
        wxChar buf[100];
        wxSnprintf(buf, sizeof(buf), _T("Sorry, value > 4000 chars in bib file at line %ld."), BibLine);
        wxLogError(buf, "Tex2RTF Fatal Error");
        return wxEmptyString;
    }

    while (!line.empty() && !stopping)
    {
        wxChar ch = line[0];
        line = line.substr(1);

        if (ch == _T('{'))
            braceCount ++;

        if (ch == _T('}'))
        {
            braceCount --;
            if (braceCount == 0)
            {
                stopping = true;
                break;
            }
        }
        else if (quotesMayTerminate && ch == _T('"'))
        {
            stopping = true;
            break;
        }

        if (!stopping)
        {
            if (!ignoreBraces || (ch != _T('{') && ch != _T('}')))
            {
                val << ch;
            }
        }
    }

    return val;
}

void BibReadValue(
  istream& istr,
  wxChar *buffer,
  bool ignoreBraces = true,
  bool quotesMayTerminate = true)
{
    int braceCount = 1;
    int i = 0;
    buffer[i] = 0;
    char ch = (char)istr.peek();
    bool stopping = false;
    while (!istr.eof() && !stopping)
    {
//      i ++;
        if (i >= 4000)
        {
            wxChar buf[100];
            wxSnprintf(buf, sizeof(buf), _T("Sorry, value > 4000 chars in bib file at line %ld."), BibLine);
            wxLogError(buf, "Tex2RTF Fatal Error");
            return;
        }
        istr.get(ch);

        if (ch == '{')
            braceCount ++;

        if (ch == '}')
        {
            braceCount --;
            if (braceCount == 0)
            {
                stopping = true;
                break;
            }
        }
        else if (quotesMayTerminate && ch == '"')
        {
            stopping = true;
            break;
        }
        if (!stopping)
        {
            if (!ignoreBraces || (ch != '{' && ch != '}'))
            {
                buffer[i] = ch;
                i ++;
            }
        }
        if (ch == 10)
            BibLine ++;
    }
    buffer[i] = 0;
    wxUnusedVar(stopping);
}

bool ReadBib(const wxString& filename)
{
  if (!wxFileExists(filename))
      return false;

  wxString name = filename;
  wxChar buf[300];
  ifstream istr((char const *)name.fn_str(), ios::in);
  if (istr.bad()) return false;

  BibLine = 1;

  OnInform(_T("Reading .bib file..."));

  char ch;
  wxChar fieldValue[4000];
  wxChar recordType[100];
  wxChar recordKey[100];
  wxChar recordField[100];
  while (!istr.eof())
  {
    Tex2RTFYield();

    BibEatWhiteSpace(istr);
    istr.get(ch);
    if (ch != '@')
    {
      wxString Message;
      Message
        << "Expected @: malformed bib file at line "
        << BibLine << " (" << filename << ')';
      OnError(Message);
      return false;
    }
    BibReadWord(istr, recordType);
    BibEatWhiteSpace(istr);
    istr.get(ch);
    if (ch != '{' && ch != '(')
    {
      wxSnprintf(buf, sizeof(buf), _T("Expected { or ( after record type: malformed .bib file at line %ld (%s)"), BibLine, filename);
      OnError(buf);
      return false;
    }
    BibEatWhiteSpace(istr);
    if (StringMatch(recordType, _T("string"), false, true))
    {
      BibReadWord(istr, recordType);
      BibEatWhiteSpace(istr);
      istr.get(ch);
      if (ch != '=')
      {
        wxSnprintf(buf, sizeof(buf), _T("Expected = after string key: malformed .bib file at line %ld (%s)"), BibLine, filename);
        OnError(buf);
        return false;
      }
      BibEatWhiteSpace(istr);
      istr.get(ch);
      if (ch != '"' && ch != '{')
      {
        wxSnprintf(buf, sizeof(buf), _T("Expected = after string key: malformed .bib file at line %ld (%s)"), BibLine, filename);
        OnError(buf);
        return false;
      }
      BibReadValue(istr, fieldValue);

      // Now put in hash table if necesary
      if (!BibStringTable.Get(recordType))
        BibStringTable.Put(recordType, (wxObject *)copystring(fieldValue));

      // Read closing ) or }
      BibEatWhiteSpace(istr);
      istr.get(ch);
      BibEatWhiteSpace(istr);
    }
    else
    {
      BibReadWord(istr, recordKey);

      BibEntry *bibEntry = new BibEntry;
      bibEntry->key = copystring(recordKey);
      bibEntry->type = copystring(recordType);

      bool moreRecords = true;
      while (moreRecords && !istr.eof())
      {
        BibEatWhiteSpace(istr);
        istr.get(ch);
        if (ch == '}' || ch == ')')
        {
          moreRecords = false;
        }
        else if (ch == ',')
        {
          BibEatWhiteSpace(istr);
          BibReadWord(istr, recordField);
          BibEatWhiteSpace(istr);
          istr.get(ch);
          if (ch != '=')
          {
            wxSnprintf(buf, sizeof(buf), _T("Expected = after field type: malformed .bib file at line %ld (%s)"), BibLine, filename);
            OnError(buf);
            return false;
          }
          BibEatWhiteSpace(istr);
          istr.get(ch);
          if (ch != '{' && ch != '"')
          {
            fieldValue[0] = ch;
            BibReadWord(istr, fieldValue+1);

            // If in the table of strings, replace with string from table.
            wxChar *s = (wxChar *)BibStringTable.Get(fieldValue);
            if (s)
            {
              wxStrcpy(fieldValue, s);
            }
          }
          else
            BibReadValue(istr, fieldValue, true, (ch == _T('"') ? true : false));

          // Now we can add a field
          if (StringMatch(recordField, _T("author"), false, true))
            bibEntry->author = copystring(fieldValue);
          else if (StringMatch(recordField, _T("key"), false, true))
            {}
          else if (StringMatch(recordField, _T("annotate"), false, true))
            {}
          else if (StringMatch(recordField, _T("abstract"), false, true))
            {}
          else if (StringMatch(recordField, _T("edition"), false, true))
            {}
          else if (StringMatch(recordField, _T("howpublished"), false, true))
            {}
          else if (StringMatch(recordField, _T("note"), false, true) || StringMatch(recordField, _T("notes"), false, true))
            {}
          else if (StringMatch(recordField, _T("series"), false, true))
            {}
          else if (StringMatch(recordField, _T("type"), false, true))
            {}
          else if (StringMatch(recordField, _T("keywords"), false, true))
            {}
          else if (StringMatch(recordField, _T("editor"), false, true) || StringMatch(recordField, _T("editors"), false, true))
            bibEntry->editor= copystring(fieldValue);
          else if (StringMatch(recordField, _T("title"), false, true))
            bibEntry->title= copystring(fieldValue);
          else if (StringMatch(recordField, _T("booktitle"), false, true))
            bibEntry->booktitle= copystring(fieldValue);
          else if (StringMatch(recordField, _T("journal"), false, true))
            bibEntry->journal= copystring(fieldValue);
          else if (StringMatch(recordField, _T("volume"), false, true))
            bibEntry->volume= copystring(fieldValue);
          else if (StringMatch(recordField, _T("number"), false, true))
            bibEntry->number= copystring(fieldValue);
          else if (StringMatch(recordField, _T("year"), false, true))
            bibEntry->year= copystring(fieldValue);
          else if (StringMatch(recordField, _T("month"), false, true))
            bibEntry->month= copystring(fieldValue);
          else if (StringMatch(recordField, _T("pages"), false, true))
            bibEntry->pages= copystring(fieldValue);
          else if (StringMatch(recordField, _T("publisher"), false, true))
            bibEntry->publisher= copystring(fieldValue);
          else if (StringMatch(recordField, _T("address"), false, true))
            bibEntry->address= copystring(fieldValue);
          else if (StringMatch(recordField, _T("institution"), false, true) || StringMatch(recordField, _T("school"), false, true))
            bibEntry->institution= copystring(fieldValue);
          else if (StringMatch(recordField, _T("organization"), false, true) || StringMatch(recordField, _T("organisation"), false, true))
            bibEntry->organization= copystring(fieldValue);
          else if (StringMatch(recordField, _T("comment"), false, true) || StringMatch(recordField, _T("comments"), false, true))
            bibEntry->comment= copystring(fieldValue);
          else if (StringMatch(recordField, _T("annote"), false, true))
            bibEntry->comment= copystring(fieldValue);
          else if (StringMatch(recordField, _T("chapter"), false, true))
            bibEntry->chapter= copystring(fieldValue);
          else
          {
            wxSnprintf(buf, sizeof(buf), _T("Unrecognised bib field type %s at line %ld (%s)"), recordField, BibLine, filename);
            OnError(buf);
          }
        }
      }
      BibList[recordKey] = bibEntry;
      BibEatWhiteSpace(istr);
    }
  }
  return true;
}

void OutputBibItem(TexRef *ref, BibEntry *bib)
{
  Tex2RTFYield();

  OnMacro(ltNUMBEREDBIBITEM, 2, true);
  OnArgument(ltNUMBEREDBIBITEM, 1, true);
  TexOutput(ref->sectionNumber);
  OnArgument(ltNUMBEREDBIBITEM, 1, false);
  OnArgument(ltNUMBEREDBIBITEM, 2, true);

  TexOutput(_T(" "));
  OnMacro(ltBF, 1, true);
  OnArgument(ltBF, 1, true);
  if (bib->author)
    TexOutput(bib->author);
  OnArgument(ltBF, 1, false);
  OnMacro(ltBF, 1, false);
  if (bib->author && (wxStrlen(bib->author) > 0) && (bib->author[wxStrlen(bib->author) - 1] != '.'))
    TexOutput(_T(". "));
  else
    TexOutput(_T(" "));

  if (bib->year)
  {
    TexOutput(bib->year);
  }
  if (bib->month)
  {
    TexOutput(_T(" ("));
    TexOutput(bib->month);
    TexOutput(_T(")"));
  }
  if (bib->year || bib->month)
    TexOutput(_T(". "));

  if (StringMatch(bib->type, _T("article"), false, true))
  {
    if (bib->title)
    {
      TexOutput(bib->title);
      TexOutput(_T(". "));
    }
    if (bib->journal)
    {
      OnMacro(ltIT, 1, true);
      OnArgument(ltIT, 1, true);
      TexOutput(bib->journal);
      OnArgument(ltIT, 1, false);
      OnMacro(ltIT, 1, false);
    }
    if (bib->volume)
    {
      TexOutput(_T(", "));
      OnMacro(ltBF, 1, true);
      OnArgument(ltBF, 1, true);
      TexOutput(bib->volume);
      OnArgument(ltBF, 1, false);
      OnMacro(ltBF, 1, false);
    }
    if (bib->number)
    {
      TexOutput(_T("("));
      TexOutput(bib->number);
      TexOutput(_T(")"));
    }
    if (bib->pages)
    {
      TexOutput(_T(", pages "));
      TexOutput(bib->pages);
    }
    TexOutput(_T("."));
  }
  else if (StringMatch(bib->type, _T("book"), false, true) ||
           StringMatch(bib->type, _T("unpublished"), false, true) ||
           StringMatch(bib->type, _T("manual"), false, true) ||
           StringMatch(bib->type, _T("phdthesis"), false, true) ||
           StringMatch(bib->type, _T("mastersthesis"), false, true) ||
           StringMatch(bib->type, _T("misc"), false, true) ||
           StringMatch(bib->type, _T("techreport"), false, true) ||
           StringMatch(bib->type, _T("booklet"), false, true))
  {
    if (bib->title || bib->booktitle)
    {
      OnMacro(ltIT, 1, true);
      OnArgument(ltIT, 1, true);
      TexOutput(bib->title ? bib->title : bib->booktitle);
      TexOutput(_T(". "));
      OnArgument(ltIT, 1, false);
      OnMacro(ltIT, 1, false);
    }
    if (StringMatch(bib->type, _T("phdthesis"), false, true))
      TexOutput(_T("PhD thesis. "));
    if (StringMatch(bib->type, _T("techreport"), false, true))
      TexOutput(_T("Technical report. "));
    if (bib->editor)
    {
      TexOutput(_T("Ed. "));
      TexOutput(bib->editor);
      TexOutput(_T(". "));
    }
    if (bib->institution)
    {
      TexOutput(bib->institution);
      TexOutput(_T(". "));
    }
    if (bib->organization)
    {
      TexOutput(bib->organization);
      TexOutput(_T(". "));
    }
    if (bib->publisher)
    {
      TexOutput(bib->publisher);
      TexOutput(_T(". "));
    }
    if (bib->address)
    {
      TexOutput(bib->address);
      TexOutput(_T(". "));
    }
  }
  else if (StringMatch(bib->type, _T("inbook"), false, true) ||
           StringMatch(bib->type, _T("inproceedings"), false, true) ||
           StringMatch(bib->type, _T("incollection"), false, true) ||
           StringMatch(bib->type, _T("conference"), false, true))
  {
    if (bib->title)
    {
      TexOutput(bib->title);
    }
    if (bib->booktitle)
    {
      TexOutput(_T(", from "));
      OnMacro(ltIT, 1, true);
      OnArgument(ltIT, 1, true);
      TexOutput(bib->booktitle);
      TexOutput(_T("."));
      OnArgument(ltIT, 1, false);
      OnMacro(ltIT, 1, false);
    }
    if (bib->editor)
    {
      TexOutput(_T(", ed. "));
      TexOutput(bib->editor);
    }
    if (bib->publisher)
    {
      TexOutput(_T(" "));
      TexOutput(bib->publisher);
    }
    if (bib->address)
    {
      if (bib->publisher) TexOutput(_T(", "));
      else TexOutput(_T(" "));
      TexOutput(bib->address);
    }
    if (bib->publisher || bib->address)
      TexOutput(_T("."));

    if (bib->volume)
    {
      TexOutput(_T(" "));
      OnMacro(ltBF, 1, true);
      OnArgument(ltBF, 1, true);
      TexOutput(bib->volume);
      OnArgument(ltBF, 1, false);
      OnMacro(ltBF, 1, false);
    }
    if (bib->number)
    {
      if (bib->volume)
      {
        TexOutput(_T("("));
        TexOutput(bib->number);
        TexOutput(_T(")."));
      }
      else
      {
        TexOutput(_T(" Number "));
        TexOutput(bib->number);
        TexOutput(_T("."));
      }
    }
    if (bib->chapter)
    {
      TexOutput(_T(" Chap. "));
      TexOutput(bib->chapter);
    }
    if (bib->pages)
    {
      if (bib->chapter) TexOutput(_T(", pages "));
      else TexOutput(_T(" Pages "));
      TexOutput(bib->pages);
      TexOutput(_T("."));
    }
  }
  OnArgument(ltNUMBEREDBIBITEM, 2, false);
  OnMacro(ltNUMBEREDBIBITEM, 2, false);
}

void OutputBib(void)
{
  // Write the heading
  ForceTopicName(_T("bibliography"));
  FakeCurrentSection(ReferencesNameString);
  ForceTopicName(NULL);

  OnMacro(ltPAR, 0, true);
  OnMacro(ltPAR, 0, false);

  if ((convertMode == TEX_RTF) && !winHelp)
  {
    OnMacro(ltPAR, 0, true);
    OnMacro(ltPAR, 0, false);
  }

  for (StringSet::iterator it = CitationList.begin(); it != CitationList.end(); ++it)
  {
    const wxString& citeKey = *it;
    TexReferenceMap::iterator iTexRef = TexReferences.find(citeKey);
    if (iTexRef != TexReferences.end())
    {
      TexRef *ref = iTexRef->second;
      BibMap::iterator bibNode = BibList.find(citeKey);
      if (bibNode != BibList.end() && ref)
      {
        BibEntry *entry = bibNode->second;
        OutputBibItem(ref, entry);
      }
    }
  }
}

static int citeCount = 1;

void ResolveBibReferences(void)
{
  if (CitationList.size() > 0)
    OnInform(_T("Resolving bibliographic references..."));

  citeCount = 1;
  wxChar buf[200];
  for (StringSet::iterator it = CitationList.begin(); it != CitationList.end(); ++it)
  {
    Tex2RTFYield();
    const wxString& citeKey = *it;
    TexReferenceMap::iterator iTexRef = TexReferences.find(citeKey);
    if (iTexRef != TexReferences.end())
    {
      TexRef *ref = iTexRef->second;
      BibMap::iterator iBibNode = BibList.find(citeKey);
      if (iBibNode != BibList.end() && ref)
      {
        // Unused Variable
        //BibEntry *entry = (BibEntry *)bibNode->GetData();
        if (!ref->sectionNumber.empty())
        {
          ref->sectionNumber = wxEmptyString;
        }
        wxSnprintf(buf, sizeof(buf), _T("[%d]"), citeCount);
        ref->sectionNumber = copystring(buf);
        citeCount ++;
      }
      else
      {
        wxSnprintf(buf, sizeof(buf), _T("Warning: bib ref %s not resolved."), citeKey.c_str());
        OnInform(buf);
      }
    }
  }
}

// Remember we need to resolve this citation
void AddCitation(const wxString& citeKey)
{
  StringSet::iterator iCitation = CitationList.find(citeKey);
  if (iCitation == CitationList.end())
  {
    CitationList.insert(citeKey);
  }

  TexReferenceMap::iterator iTexRef = TexReferences.find(citeKey);
  if (iTexRef == TexReferences.end())
  {
    TexReferences[citeKey] = new TexRef(citeKey, _T("??"), wxEmptyString);
  }
}

TexRef* FindReference(const wxString& key)
{
  TexReferenceMap::iterator iTexRef = TexReferences.find(key);
  if (iTexRef != TexReferences.end())
  {
    return iTexRef->second;
  }
  return 0;
}

/*
 * Custom macro stuff
 *
 */

bool StringTobool(const wxString& val)
{
    wxString up(val);
    up.MakeUpper();

    if (up.IsSameAs(_T("YES")) ||
        up.IsSameAs(_T("TRUE")) ||
        up.IsSameAs(_T("ON")) ||
        up.IsSameAs(_T("OK")) |
        up.IsSameAs(_T("1")))
        return true;

    return false;
}

void RegisterIntSetting (const wxString& s, int *number)
{
    if (number)
    {
        long val;
        s.ToLong(&val);
        *number = (int)val;
    }
}

// Define a variable value from the .ini file
wxChar *RegisterSetting(
  const wxString& settingName,
  const wxString& settingValue,
  bool interactive)
{
  wxString settingValueStr( settingValue );

  static wxChar errorCode[100];
  wxStrcpy(errorCode, _T("OK"));
  if (StringMatch(settingName, _T("chapterName"), false, true))
  {
    ChapterNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("sectionName"), false, true))
  {
    SectionNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("subsectionName"), false, true))
  {
    SubsectionNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("subsubsectionName"), false, true))
  {
    SubsubsectionNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("indexName"), false, true))
  {
    IndexNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("contentsName"), false, true))
  {
    ContentsNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("glossaryName"), false, true))
  {
    GlossaryNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("referencesName"), false, true))
  {
    ReferencesNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("tablesName"), false, true))
  {
    TablesNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("figuresName"), false, true))
  {
    FiguresNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("tableName"), false, true))
  {
    TableNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("figureName"), false, true))
  {
    FigureNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("abstractName"), false, true))
  {
    AbstractNameString = settingValue;
  }
  else if (StringMatch(settingName, _T("chapterFontSize"), false, true))
    RegisterIntSetting(settingValueStr, &chapterFont);
  else if (StringMatch(settingName, _T("sectionFontSize"), false, true))
    RegisterIntSetting(settingValueStr, &sectionFont);
  else if (StringMatch(settingName, _T("subsectionFontSize"), false, true))
    RegisterIntSetting(settingValueStr, &subsectionFont);
  else if (StringMatch(settingName, _T("titleFontSize"), false, true))
    RegisterIntSetting(settingValueStr, &titleFont);
  else if (StringMatch(settingName, _T("authorFontSize"), false, true))
    RegisterIntSetting(settingValueStr, &authorFont);
  else if (StringMatch(settingName, _T("ignoreInput"), false, true))
    IgnorableInputFiles.push_back(wxFileNameFromPath(settingValue));
  else if (StringMatch(settingName, _T("mirrorMargins"), false, true))
    mirrorMargins = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("runTwice"), false, true))
    runTwice = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("isInteractive"), false, true))
    isInteractive = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("headerRule"), false, true))
    headerRule = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("footerRule"), false, true))
    footerRule = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("combineSubSections"), false, true))
    combineSubSections = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("listLabelIndent"), false, true))
    RegisterIntSetting(settingValueStr, &labelIndentTab);
  else if (StringMatch(settingName, _T("listItemIndent"), false, true))
    RegisterIntSetting(settingValueStr, &itemIndentTab);
  else if (StringMatch(settingName, _T("useUpButton"), false, true))
    useUpButton = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("useHeadingStyles"), false, true))
    useHeadingStyles = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("useWord"), false, true))
    useWord = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("contentsDepth"), false, true))
    RegisterIntSetting(settingValueStr, &contentsDepth);
  else if (StringMatch(settingName, _T("generateHPJ"), false, true))
    generateHPJ = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("truncateFilenames"), false, true))
    truncateFilenames = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("winHelpVersion"), false, true))
    RegisterIntSetting(settingValueStr, &winHelpVersion);
  else if (StringMatch(settingName, _T("winHelpContents"), false, true))
    winHelpContents = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("htmlIndex"), false, true))
    htmlIndex = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("htmlWorkshopFiles"), false, true))
    htmlWorkshopFiles = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("htmlFrameContents"), false, true))
    htmlFrameContents = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("htmlStylesheet"), false, true))
  {
    htmlStylesheet = settingValue;
  }
  else if (StringMatch(settingName, _T("upperCaseNames"), false, true))
  {
    upperCaseNames = StringTobool(settingValue);
  }
  else if (StringMatch(settingName, _T("ignoreBadRefs"), false, true))
  {
    ignoreBadRefs = StringTobool(settingValue);
  }
  else if (StringMatch(settingName, _T("htmlFaceName"), false, true))
  {
    htmlFaceName = settingValue;
  }
  else if (StringMatch(settingName, _T("winHelpTitle"), false, true))
  {
    winHelpTitle = settingValue;
  }
  else if (StringMatch(settingName, _T("indexSubsections"), false, true))
    indexSubsections = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("compatibility"), false, true))
    compatibilityMode = StringTobool(settingValue);
  else if (StringMatch(settingName, _T("defaultColumnWidth"), false, true))
  {
    RegisterIntSetting(settingValueStr, &defaultTableColumnWidth);
    defaultTableColumnWidth = 20*defaultTableColumnWidth;
  }
  else if (StringMatch(settingName, _T("bitmapMethod"), false, true))
  {
    if (
      (wxStrcmp(settingValue, _T("includepicture")) != 0) &&
      (wxStrcmp(settingValue, _T("hex")) != 0) &&
      (wxStrcmp(settingValue, _T("import")) != 0))
    {
      if (interactive)
        OnError(_T("Unknown bitmapMethod"));
      wxStrcpy(errorCode, _T("Unknown bitmapMethod"));
    }
    else
    {
      bitmapMethod = settingValue;
    }
  }
  else if (StringMatch(settingName, _T("htmlBrowseButtons"), false, true))
  {
    if (wxStrcmp(settingValue, _T("none")) == 0)
      htmlBrowseButtons = HTML_BUTTONS_NONE;
    else if (wxStrcmp(settingValue, _T("bitmap")) == 0)
      htmlBrowseButtons = HTML_BUTTONS_BITMAP;
    else if (wxStrcmp(settingValue, _T("text")) == 0)
      htmlBrowseButtons = HTML_BUTTONS_TEXT;
    else
    {
      if (interactive)
        OnInform(_T("Initialisation file error: htmlBrowseButtons must be one of none, bitmap, or text."));
      wxStrcpy(errorCode, _T("Initialisation file error: htmlBrowseButtons must be one of none, bitmap, or text."));
    }
  }
  else if (StringMatch(settingName, _T("backgroundImage"), false, true))
  {
    backgroundImageString = settingValue;
  }
  else if (StringMatch(settingName, _T("backgroundColour"), false, true))
  {
    backgroundColourString = settingValue;
  }
  else if (StringMatch(settingName, _T("textColour"), false, true))
  {
    textColourString = settingValue;
  }
  else if (StringMatch(settingName, _T("linkColour"), false, true))
  {
    linkColourString = settingValue;
  }
  else if (StringMatch(settingName, _T("followedLinkColour"), false, true))
  {
    followedLinkColourString = settingValue;
  }
  else if (StringMatch(settingName, _T("conversionMode"), false, true))
  {
    if (StringMatch(settingValue, _T("RTF"), false, true))
    {
      winHelp = false; convertMode = TEX_RTF;
    }
    else if (StringMatch(settingValue, _T("WinHelp"), false, true))
    {
      winHelp = true; convertMode = TEX_RTF;
    }
    else if (
      StringMatch(settingValue, _T("XLP"), false, true) ||
      StringMatch(settingValue, _T("wxHelp"), false, true))
    {
      convertMode = TEX_XLP;
    }
    else if (StringMatch(settingValue, _T("HTML"), false, true))
    {
      convertMode = TEX_HTML;
    }
    else
    {
      if (interactive)
        OnInform(_T("Initialisation file error: conversionMode must be one of\nRTF, WinHelp, XLP (or wxHelp), HTML."));
      wxStrcpy(errorCode, _T("Initialisation file error: conversionMode must be one of\nRTF, WinHelp, XLP (or wxHelp), HTML."));
    }
  }
  else if (StringMatch(settingName, _T("documentFontSize"), false, true))
  {
      int n;
      RegisterIntSetting(settingValueStr, &n);
      if (n == 10 || n == 11 || n == 12)
          SetFontSizes(n);
      else
      {
          wxChar buf[200];
          wxSnprintf(buf, sizeof(buf), _T("Initialisation file error: nonstandard document font size %d."), n);
          if (interactive)
              OnInform(buf);
          wxStrcpy(errorCode, buf);
      }
  }
  else
  {
      wxChar buf[200];
      wxSnprintf(buf, sizeof(buf), _T("Initialisation file error: unrecognised setting %s."), settingName.c_str());
      if (interactive)
          OnInform(buf);
      wxStrcpy(errorCode, buf);
  }
  return errorCode;
}

bool ReadCustomMacros(const wxString& filename)
{
    if (!wxFileExists(filename))
        return false;

    wxFileInputStream input( filename );
    if(!input.Ok()) return false;
    wxTextInputStream ini( input );

    CustomMacroMap.clear();

    while (!input.Eof())
    {
        wxString line = ini.ReadLine();
        BibEatWhiteSpace(line);
        if (line.empty()) continue;

        if (line[0] != _T('\\')) // Not a macro definition, so must be NAME=VALUE
        {
            wxString settingName = BibReadWord(line);
            BibEatWhiteSpace(line);
            if (line.empty() || line[0] != _T('='))
            {
                OnError(_T("Expected = following name: malformed tex2rtf.ini file."));
                return false;
            }
            else
            {
                line = line.substr(1);
                BibEatWhiteSpace(line);
                wxString settingValue = BibReadToEOL(line);
                RegisterSetting(settingName, settingValue);
            }
        }
        else
        {
            line = line.substr(1);
            wxString macroName = BibReadWord(line);
            BibEatWhiteSpace(line);
            if (line[0] != _T('['))
            {
                OnError(_T("Expected [ followed by number of arguments: malformed tex2rtf.ini file."));
                return false;
            }
            line = line.substr(1);
            wxString noAargStr = line.BeforeFirst(_T(']'));
            line = line.AfterFirst(_T(']'));
            long noArgs;
            if (!noAargStr.ToLong(&noArgs) || line.empty())
            {
                OnError(_T("Expected ] following number of arguments: malformed tex2rtf.ini file."));
                return false;
            }
            BibEatWhiteSpace(line);
            if (line[0] != _T('{'))
            {
                OnError(_T("Expected { followed by macro body: malformed tex2rtf.ini file."));
                return false;
            }

            CustomMacro *macro = new CustomMacro(macroName.c_str(), noArgs, NULL);
            wxString macroBody = BibReadValue(line, false, false); // Don't ignore extra braces
            if (!macroBody.empty())
                macro->macroBody = copystring(macroBody.c_str());

            BibEatWhiteSpace(line);
            CustomMacroMap[macroName] = macro;
            AddMacroDef(ltCUSTOM_MACRO, macroName.c_str(), noArgs);
        }

    }
    wxChar mbuf[200];
    wxSnprintf(mbuf, sizeof(mbuf), _T("Read initialization file %s."), filename.c_str());
    OnInform(mbuf);
    return true;
}

CustomMacro *FindCustomMacro(wxChar *name)
{
  MacroMap::iterator it = CustomMacroMap.find(name);
  if (it != CustomMacroMap.end())
  {
    CustomMacro *macro = it->second;

    return macro;
  }
  return NULL;
}

// Display custom macros
void ShowCustomMacros(void)
{
  MacroMap::iterator it = CustomMacroMap.begin();
  if (it == CustomMacroMap.end())

  {
    OnInform(_T("No custom macros loaded.\n"));
    return;
  }

  wxChar buf[400];
  while (it != CustomMacroMap.end())
  {
    CustomMacro *macro = it->second;
    wxSnprintf(buf, sizeof(buf), _T("\\%s[%d]\n    {%s}"), macro->macroName, macro->noArgs,
     macro->macroBody ? macro->macroBody : _T(""));
    OnInform(buf);
    ++it;
  }
}

// Parse a string into several comma-separated fields
wxChar *ParseMultifieldString(const wxString& allFields, size_t *pos)
{
  static wxChar buffer[300];
  int i = 0;
  size_t fieldIndex = *pos;
  size_t len = allFields.length();
  size_t oldPos = *pos;
  bool keepGoing = true;
  while ((fieldIndex <= len) && keepGoing)
  {
    if (allFields[fieldIndex] == _T(' '))
    {
      // Skip
      fieldIndex ++;
    }
    else if (allFields[fieldIndex] == _T(','))
    {
      *pos = fieldIndex + 1;
      keepGoing = false;
    }
    else if (allFields[fieldIndex] == 0)
    {
      *pos = fieldIndex + 1;
      keepGoing = false;
    }
    else
    {
      buffer[i] = allFields[fieldIndex];
      fieldIndex ++;
      i++;
    }
  }
  buffer[i] = 0;
  if (oldPos == *pos)
    *pos = len + 1;

  if (i == 0)
    return NULL;
  else
    return buffer;
}

/*
 * Colour tables
 *
 */

ColourTableEntry::ColourTableEntry(
  const wxString& theName,
  unsigned int r,
  unsigned int g,
  unsigned int b)
{
  mName = theName;
  red = r;
  green = g;
  blue = b;
}

ColourTableEntry::~ColourTableEntry(void)
{
}

void AddColour(
  const wxString& theName,
  unsigned int r,
  unsigned int g,
  unsigned int b)
{
  ColourTableMap::iterator it = ColourTable.find(theName);
  if (it != ColourTable.end())
  {
    ColourTableEntry *entry = it->second;
    if (entry->red == r || entry->green == g || entry->blue == b)
      return;
    else
    {
      delete entry;
      ColourTable.erase(it);
    }
  }
  ColourTableEntry* entry = new ColourTableEntry(theName, r, g, b);
  ColourTable[theName] = entry;
}

int FindColourPosition(const wxString& theName)
{
  int i = 0;
  for (
    ColourTableMap::iterator it = ColourTable.begin();
    it != ColourTable.end();
    ++it)
  {
    ColourTableEntry *entry = it->second;
    if (theName == entry->mName)
    {
      return i;
    }
    ++i;
  }
  return -1;
}

// Converts e.g. "red" -> "#FF0000"
extern void DecToHex(int, wxChar *);

bool FindColourHTMLString(const wxString& theName, wxString& buf)
{
  for (
    ColourTableMap::iterator it = ColourTable.begin();
    it != ColourTable.end();
    ++it)
  {
    ColourTableEntry *entry = it->second;
    if (wxStrcmp(theName, entry->mName) == 0)
    {
      buf = _T("#");
      wxChar buf2[3];
      DecToHex(entry->red, buf2);
      buf.append(buf2);
      DecToHex(entry->green, buf2);
      buf.append(buf2);
      DecToHex(entry->blue, buf2);
      buf.append(buf2);

      return true;
    }
  }
  return false;
}


void InitialiseColourTable(void)
{
  // \\red0\\green0\\blue0;
  AddColour(_T("black"), 0,0,0);

  // \\red0\\green0\\blue255;\\red0\\green255\\blue255;\n");
  AddColour(_T("cyan"), 0,255,255);

  // \\red0\\green255\\blue0;
  AddColour(_T("green"), 0,255,0);

  // \\red255\\green0\\blue255;
  AddColour(_T("magenta"), 255,0,255);

  // \\red255\\green0\\blue0;
  AddColour(_T("red"), 255,0,0);

  // \\red255\\green255\\blue0;
  AddColour(_T("yellow"), 255,255,0);

  // \\red255\\green255\\blue255;}");
  AddColour(_T("white"), 255,255,255);
}

/*
 * The purpose of this is to reduce the number of times wxYield is
 * called, since under Windows this can slow things down.
 */

void Tex2RTFYield(bool force)
{
#ifdef __WINDOWS__
    static int yieldCount = 0;

    if (isSync)
        return;

    if (force)
    yieldCount = 0;
    if (yieldCount == 0)
    {
        if (wxTheApp)
            wxYield();
        yieldCount = 10;
    }
    yieldCount --;
#else
    wxUnusedVar(force);
#endif
}

// In both RTF generation and HTML generation for wxHelp version 2,
// we need to associate \indexed keywords with the current filename/topics.

// Hash table for lists of keywords for topics (WinHelp).
wxHashTable TopicTable(wxKEY_STRING);
void AddKeyWordForTopic(wxChar *topic, wxChar *entry, const wxString& FileName)
{
  TexTopic *texTopic = (TexTopic *)TopicTable.Get(topic);
  if (!texTopic)
  {
    texTopic = new TexTopic(FileName);
    texTopic->keywords = new StringSet;
    TopicTable.Put(topic, texTopic);
  }

  StringSet::iterator iString = texTopic->keywords->find(entry);
  if (iString != texTopic->keywords->end())
  {
    texTopic->keywords->insert(entry);
  }
}

void ClearKeyWordTable(void)
{
  TopicTable.BeginFind();
  wxHashTable::Node *node = TopicTable.Next();
  while (node)
  {
    TexTopic *texTopic = (TexTopic *)node->GetData();
    delete texTopic;
    node = TopicTable.Next();
  }
  TopicTable.Clear();
}


/*
 * TexTopic structure
 */

TexTopic::TexTopic(const wxString& f)
  : filename(f)
{
  hasChildren = false;
  keywords = NULL;
}

TexTopic::~TexTopic(void)
{
  if (keywords)
    delete keywords;
}

// Convert case, according to upperCaseNames setting.
wxString ConvertCase(const wxString& String)
{
  wxString buf;
  if (upperCaseNames)
  {
    for (size_t i = 0; i < String.length(); ++i)
    {
      buf.append(wxToupper(String[i]));
    }
  }
  else
  {
    for (size_t i = 0; i < String.length(); ++i)
    {
      buf.append(wxTolower(String[i]));
    }
  }
  return buf;
}

// if substring is true, search for str1 in str2
bool StringMatch(
  const wxString& str1,
  const wxString& str2,
  bool subString,
  bool exact)
{
  if (subString)
  {
    wxString Sstr1(str1);
    wxString Sstr2(str2);
    if (!exact)
    {
      Sstr1.MakeUpper();
      Sstr2.MakeUpper();
    }
    return Sstr2.find(Sstr1) != wxNOT_FOUND;
  }
  else
  {
    return
      exact ?
        wxString(str2).Cmp(str1) == 0 :
        wxString(str2).CmpNoCase(str1) == 0;
  }
}
