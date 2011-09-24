//*****************************************************************************
// Copyright:   (c) Julian Smart
// Author:      Julian Smart
// Modified by: Wlodzimiez ABX Skiba 2003/2004 Unicode support
//              Ron Lee
// Created:     7/9/1993
// License:     wxWindows license
// Description:
//   Converts Latex to obsolete XLP format.
//*****************************************************************************

#include "tex2any.h"
#include "tex2rtf.h"

#include <ctype.h>

long currentBlockId = -1;
static TexChunk *descriptionItemArg = NULL;
static int indentLevel = 0;
static int noColumns = 0;
static int currentTab = 0;
static bool tableVerticalLineLeft = false;
static bool tableVerticalLineRight = false;
static bool inTable = false;
static int citeCount = 1;
WX_DECLARE_HASH_MAP(int, wxString, wxIntegerHash, wxIntegerEqual, IntHashMap);
IntHashMap hyperLinks;
WX_DECLARE_STRING_HASH_MAP(long, StringLongMap);
StringLongMap hyperLabels;
FILE *Index = NULL;


extern TexReferenceMap TexReferences;


void PadToTab(int tabPos)
{
  int currentCol = GetCurrentColumn();
  for (int i = currentCol; i < tabPos; i++)
    TexOutput(_T(" "), true);
}

static long xlpBlockId = 0;
long NewBlockId(void)
{
  return xlpBlockId ++;
}

// Called on start/end of macro examination
void XLPOnMacro(int macroId, int no_args, bool start)
{
  wxChar buf[100];
  switch (macroId)
  {
  case ltCHAPTER:
  case ltCHAPTERSTAR:
  case ltCHAPTERHEADING:
  {
    if (!start)
    {
      sectionNo = 0;
      figureNo = 0;
      subsectionNo = 0;
      subsubsectionNo = 0;
      if (macroId != ltCHAPTERSTAR)
        chapterNo ++;

      SetCurrentOutputs(Contents, Chapters);
      long id1 = NewBlockId();
      currentBlockId = NewBlockId();

      startedSections = true;
      wxFprintf(Contents, _T("\\hy-%d{%ld}{"), hyBLOCK_SMALL_HEADING, id1);
      wxFprintf(Chapters, _T("\n\\hy-%d{%ld}{"), hyBLOCK_LARGE_VISIBLE_SECTION, currentBlockId);
      wxFprintf(Index, _T("%ld %ld\n"), id1, currentBlockId);

      OutputCurrentSection(); // Repeat section header

      wxFprintf(Contents, _T("}\n\n"));
      wxFprintf(Chapters, _T("}\n\n"));
      SetCurrentOutput(Chapters);
      wxString topicName = FindTopicName(GetNextChunk());
      hyperLabels[topicName] = currentBlockId;
    }
    break;
  }
  case ltSECTION:
  case ltSECTIONSTAR:
  case ltSECTIONHEADING:
  case ltGLOSS:
  {
    if (!start)
    {
      subsectionNo = 0;
      subsubsectionNo = 0;

      if (macroId != ltSECTIONSTAR)
        sectionNo ++;

      SetCurrentOutputs(Chapters, Sections);
      long id1 = NewBlockId();
      currentBlockId = NewBlockId();

      startedSections = true;

      if (DocumentStyle == LATEX_ARTICLE)
        wxFprintf(Contents, _T("\\hy-%d{%ld}{"), hyBLOCK_LARGE_HEADING, id1);
      else
        wxFprintf(Chapters, _T("\\hy-%d{%ld}{"), hyBLOCK_BOLD, id1);
      wxFprintf(Sections, _T("\n\\hy-%d{%ld}{"), hyBLOCK_LARGE_VISIBLE_SECTION, currentBlockId);
      wxFprintf(Index, _T("%ld %ld\n"), id1, currentBlockId);

      OutputCurrentSection(); // Repeat section header

      if (DocumentStyle == LATEX_ARTICLE)
        wxFprintf(Contents, _T("}\n\n"));
      else
        wxFprintf(Chapters, _T("}\n\n"));
      wxFprintf(Sections, _T("}\n\n"));
      SetCurrentOutput(Sections);
      wxString topicName = FindTopicName(GetNextChunk());
      hyperLabels[topicName] = currentBlockId;
    }
    break;
  }
  case ltSUBSECTION:
  case ltSUBSECTIONSTAR:
  case ltMEMBERSECTION:
  case ltFUNCTIONSECTION:
  {
    if (!start)
    {
      subsubsectionNo = 0;

      if (macroId != ltSUBSECTIONSTAR)
        subsectionNo ++;

      SetCurrentOutputs(Sections, Subsections);
      long id1 = NewBlockId();
      currentBlockId = NewBlockId();
      wxFprintf(Sections, _T("\\hy-%d{%ld}{"), hyBLOCK_BOLD, id1);
      wxFprintf(Subsections, _T("\n\\hy-%d{%ld}{"), hyBLOCK_LARGE_VISIBLE_SECTION, currentBlockId);
      wxFprintf(Index, _T("%ld %ld\n"), id1, currentBlockId);

      OutputCurrentSection(); // Repeat section header

      wxFprintf(Sections, _T("}\n\n"));
      wxFprintf(Subsections, _T("}\n\n"));
      SetCurrentOutput(Subsections);
      wxString topicName = FindTopicName(GetNextChunk());
      hyperLabels[topicName] = currentBlockId;
    }
    break;
  }
  case ltSUBSUBSECTION:
  case ltSUBSUBSECTIONSTAR:
  {
    if (!start)
    {
      if (macroId != ltSUBSUBSECTIONSTAR)
        subsubsectionNo ++;

      SetCurrentOutputs(Subsections, Subsubsections);
      long id1 = NewBlockId();
      currentBlockId = NewBlockId();
      wxFprintf(Subsections, _T("\\hy-%d{%ld}{"), hyBLOCK_BOLD, id1);
      wxFprintf(Subsubsections, _T("\n\\hy-%d{%ld}{"), hyBLOCK_LARGE_VISIBLE_SECTION, currentBlockId);
      wxFprintf(Index, _T("%ld %ld\n"), id1, currentBlockId);

      OutputCurrentSection(); // Repeat section header

      wxFprintf(Subsections, _T("}\n\n"));
      wxFprintf(Subsubsections, _T("}\n\n"));
      SetCurrentOutput(Subsubsections);
      wxString topicName = FindTopicName(GetNextChunk());
      hyperLabels[topicName] = currentBlockId;
    }
    break;
  }
  case ltFUNC:
  case ltPFUNC:
  case ltMEMBER:
  {
    SetCurrentOutput(Subsections);
    if (start)
    {
      long id = NewBlockId();
      wxFprintf(Subsections, _T("\\hy-%d{%ld}{"), hyBLOCK_BOLD, id);
    }
    else
      wxFprintf(Subsections, _T("}"));
    break;
  }
  case ltVOID:
//    if (start)
//      TexOutput(_T("void"), true);
    break;
  case ltBACKSLASHCHAR:
    if (start)
      TexOutput(_T("\n"), true);
    break;
  case ltPAR:
  {
    if (start)
    {
      if (ParSkip > 0)
        TexOutput(_T("\n"), true);
      TexOutput(_T("\n"), true);
    }
    break;
  }
  case ltRMFAMILY:
  case ltTEXTRM:
  case ltRM:
  {
    break;
  }
  case ltTEXTBF:
  case ltBFSERIES:
  case ltBF:
  {
    if (start)
    {
      wxChar buf[100];
      long id = NewBlockId();
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_BOLD, id);
      TexOutput(buf);
    }
    else TexOutput(_T("}"));
    break;
  }
  case ltTEXTIT:
  case ltITSHAPE:
  case ltIT:
  {
    if (start)
    {
      wxChar buf[100];
      long id = NewBlockId();
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_ITALIC, id);
      TexOutput(buf);
    }
    else TexOutput(_T("}"));
    break;
  }
  case ltTTFAMILY:
  case ltTEXTTT:
  case ltTT:
  {
    if (start)
    {
      long id = NewBlockId();
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_TELETYPE, id);
      TexOutput(buf);
    }
    else TexOutput(_T("}"));
    break;
  }
  case ltSMALL:
  {
    if (start)
    {
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_SMALL_TEXT, NewBlockId());
      TexOutput(buf);
    }
    else TexOutput(_T("}"));
    break;
  }
  case ltTINY:
  {
    if (start)
    {
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_SMALL_TEXT, NewBlockId());
      TexOutput(buf);
    }
    else TexOutput(_T("}"));
    break;
  }
  case ltNORMALSIZE:
  {
    if (start)
    {
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_NORMAL, NewBlockId());
      TexOutput(buf);
    }
    else TexOutput(_T("}"));
    break;
  }
  case ltlarge:
  {
    if (start)
    {
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_SMALL_HEADING, NewBlockId());
      TexOutput(buf);
    }
    else TexOutput(_T("}\n"));
    break;
  }
  case ltLARGE:
  {
    if (start)
    {
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_LARGE_HEADING, NewBlockId());
      TexOutput(buf);
    }
    else TexOutput(_T("}\n"));
    break;
  }
  case ltITEMIZE:
  case ltENUMERATE:
  case ltDESCRIPTION:
  case ltTWOCOLLIST:
  {
    if (start)
    {
//      tabCount ++;

//      if (indentLevel > 0)
//        TexOutput(_T("\\par\\par\n"));
      indentLevel ++;
      int listType;
      if (macroId == ltENUMERATE)
        listType = LATEX_ENUMERATE;
      else if (macroId == ltITEMIZE)
        listType = LATEX_ITEMIZE;
      else
        listType = LATEX_DESCRIPTION;
      itemizeStack.Insert(new ItemizeStruc(listType));

    }
    else
    {
      indentLevel --;

      wxList::compatibility_iterator iNode = itemizeStack.GetFirst();
      if (iNode)
      {
        ItemizeStruc *struc = (ItemizeStruc *)iNode->GetData();
        delete struc;
        itemizeStack.Erase(iNode);
      }
    }
    break;
  }
  case ltITEM:
  {
    wxList::compatibility_iterator iNode = itemizeStack.GetFirst();
    if (iNode)
    {
      ItemizeStruc *struc = (ItemizeStruc *)iNode->GetData();
      if (!start)
      {
        struc->currentItem += 1;
        wxChar indentBuf[30];

        switch (struc->listType)
        {
          case LATEX_ENUMERATE:
          {
            wxSnprintf(indentBuf, sizeof(indentBuf), _T("\\hy-%d{%ld}{%d.} "),
              hyBLOCK_BOLD, NewBlockId(), struc->currentItem);
            TexOutput(indentBuf);
            break;
          }
          case LATEX_ITEMIZE:
          {
            wxSnprintf(indentBuf, sizeof(indentBuf), _T("\\hy-%d{%ld}{o} "),
              hyBLOCK_BOLD, NewBlockId());
            TexOutput(indentBuf);
            break;
          }
          default:
          case LATEX_DESCRIPTION:
          {
            if (descriptionItemArg)
            {
              wxSnprintf(indentBuf, sizeof(indentBuf), _T("\\hy-%d{%ld}{"),
                 hyBLOCK_BOLD, NewBlockId());
              TexOutput(indentBuf);
              TraverseChildrenFromChunk(descriptionItemArg);
              TexOutput(_T("} "));
              descriptionItemArg = NULL;
            }
            break;
          }
        }
      }
    }
    break;
  }
  case ltMAKETITLE:
  {
    if (start && DocumentTitle && DocumentAuthor)
    {
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_LARGE_HEADING, NewBlockId());
      TexOutput(buf);
      TraverseChildrenFromChunk(DocumentTitle);
      TexOutput(_T("}\n\n"));
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_SMALL_HEADING, NewBlockId());
      TexOutput(buf);
      TraverseChildrenFromChunk(DocumentAuthor);
      TexOutput(_T("}\n\n"));
      if (DocumentDate)
      {
        TraverseChildrenFromChunk(DocumentDate);
        TexOutput(_T("\n"));
      }
    }
    break;
  }
  case ltTABLEOFCONTENTS:
  {
    if (start)
    {
      FILE *fd = wxFopen(ContentsName, _T("r"));
      if (fd)
      {
        int ch = getc(fd);
        while (ch != EOF)
        {
          wxPutc(ch, Chapters);
          ch = getc(fd);
        }
        fclose(fd);
      }
      else
      {
        TexOutput(_T("RUN TEX2RTF AGAIN FOR CONTENTS PAGE\n"));
        OnInform(_T("Run Tex2RTF again to include contents page."));
      }
    }
    break;
  }
  case ltHARDY:
  {
    if (start)
      TexOutput(_T("HARDY"), true);
    break;
  }
  case ltWXCLIPS:
  {
    if (start)
      TexOutput(_T("wxCLIPS"), true);
    break;
  }
  case ltVERBATIM:
  {
    if (start)
    {
      wxChar buf[100];
      long id = NewBlockId();
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_TELETYPE, id);
      TexOutput(buf);
    }
    else TexOutput(_T("}"));
    break;
  }
  case ltHRULE:
  {
    if (start)
    {
      TexOutput(_T("\n------------------------------------------------------------------"), true);
    }
    break;
  }
  case ltHLINE:
  {
    if (start)
    {
      TexOutput(_T("--------------------------------------------------------------------------------"), true);
    }
    break;
  }
  case ltSPECIALAMPERSAND:
  {
    if (start)
    {
      currentTab ++;
      int tabPos = (80/noColumns)*currentTab;
      PadToTab(tabPos);
    }
    break;
  }
  case ltTABULAR:
  case ltSUPERTABULAR:
  {
    if (start)
    {
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_TELETYPE, NewBlockId());
      TexOutput(buf);
    }
    else
      TexOutput(_T("}"));
    break;
  }
  case ltNUMBEREDBIBITEM:
  {
    if (!start)
      TexOutput(_T("\n\n"), true);
    break;
  }
  case ltCAPTION:
  case ltCAPTIONSTAR:
  {
    if (start)
    {
      figureNo ++;

      wxChar figBuf[40];
      if (DocumentStyle != LATEX_ARTICLE)
        wxSnprintf(figBuf, sizeof(figBuf), _T("Figure %d.%d: "), chapterNo, figureNo);
      else
        wxSnprintf(figBuf, sizeof(figBuf), _T("Figure %d: "), figureNo);

      TexOutput(figBuf);
    }
    else
    {
      wxString topicName = FindTopicName(GetNextChunk());

      AddTexRef(
        topicName,
        wxEmptyString,
        wxEmptyString,
        ((DocumentStyle != LATEX_ARTICLE) ? chapterNo : figureNo),
        ((DocumentStyle != LATEX_ARTICLE) ? figureNo : 0));
    }
    break;
  }
  default:
  {
    DefaultOnMacro(macroId, no_args, start);
    break;
  }
  }
}

bool XLPOnArgument(int macroId, int arg_no, bool start)
{
  wxChar buf[300];
  switch (macroId)
  {
  case ltCHAPTER:
  case ltCHAPTERSTAR:
  case ltCHAPTERHEADING:
  case ltSECTION:
  case ltSECTIONSTAR:
  case ltSECTIONHEADING:
  case ltSUBSECTION:
  case ltSUBSECTIONSTAR:
  case ltSUBSUBSECTION:
  case ltSUBSUBSECTIONSTAR:
  case ltGLOSS:
  case ltMEMBERSECTION:
  case ltFUNCTIONSECTION:
  {
    if (!start && (arg_no == 1))
      currentSection = GetArgChunk();
    return false;
  }
  case ltFUNC:
  {
    if (!start && (arg_no == 1))
      TexOutput(_T(" "), true);
    if (start && (arg_no == 3))
      TexOutput(_T("("), true);
    if (!start && (arg_no == 3))
     TexOutput(_T(")"), true);
    break;
  }
  case ltPFUNC:
  {
    if (!start && (arg_no == 1))
      TexOutput(_T(" "), true);

    if (start && (arg_no == 2))
      TexOutput(_T("(*"), true);
    if (!start && (arg_no == 2))
      TexOutput(_T(")"), true);

    if (start && (arg_no == 3))
      TexOutput(_T("("), true);
    if (!start && (arg_no == 3))
      TexOutput(_T(")"), true);
    break;
  }
  case ltCLIPSFUNC:
  {
    if (!start && (arg_no == 1))
      TexOutput(_T(" "), true);
    if (start && (arg_no == 2))
    {
      TexOutput(_T("("), true);
      long id = NewBlockId();
      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_BOLD, id);
      TexOutput(buf);
    }
    if (!start && (arg_no == 2))
    {
      TexOutput(_T("}"));
    }
    if (!start && (arg_no == 3))
     TexOutput(_T(")"), true);
    break;
  }
  case ltPARAM:
  {
    if (start && (arg_no == 2))
    {
      long id = NewBlockId();
      wxSnprintf(buf, sizeof(buf), _T(" \\hy-%d{%ld}{"), hyBLOCK_BOLD, id);
      TexOutput(buf);
    }
    if (!start && (arg_no == 2))
    {
      TexOutput(_T("}"));
    }
    break;
  }
  case ltCPARAM:
  {
    if (start && (arg_no == 2))
    {
      long id = NewBlockId();
      wxSnprintf(buf, sizeof(buf), _T(" \\hy-%d{%ld}{"), hyBLOCK_BOLD, id);
      TexOutput(buf);
    }
    if (!start && (arg_no == 2))
    {
      TexOutput(_T("}"));
    }
    break;
  }
  case ltMEMBER:
  {
    if (!start && (arg_no == 1))
      TexOutput(_T(" "), true);
    break;
  }
  case ltLABEL:
  {
    return false;
  }
  case ltREF:
  {
    if (start)
    {
      wxString sec;

      wxString refName = GetArgData();
      if (!refName.empty())
      {
        TexRef *texRef = FindReference(refName);
        if (texRef)
        {
          sec = texRef->sectionNumber;
        }
      }
      if (!sec.empty())
      {
        TexOutput(sec);
      }
      return false;
    }
    break;
  }
  case ltHELPREF:
  case ltHELPREFN:
  case ltPOPREF:
  {
    if (arg_no == 1)
    {
      if (start)
      {
        currentBlockId = NewBlockId();
        wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{"), hyBLOCK_RED_ITALIC, currentBlockId);
        TexOutput(buf);
      }
      else TexOutput(_T("}"));
    }
    if (arg_no == 2)
    {
      if (start)
      {
        wxString label = GetArgData();
        hyperLinks[currentBlockId] = label;
      }

      return false;
    }
    break;
  }
  case ltURLREF:
  {
    if (arg_no == 1)
    {
      return true;
    }
    else if (arg_no == 2)
    {
      if (start)
        TexOutput(_T(" ("));
      else
        TexOutput(_T(")"));
      return true;
    }
    break;
  }
  case ltITEM:
  {
    if (start && IsArgOptional())
    {
      descriptionItemArg = GetArgChunk();
      return false;
    }
    break;
  }
  case ltTABULAR:
  case ltSUPERTABULAR:
  {
    if (arg_no == 1)
    {
      if (start)
      {
        inTable = true;
        tableVerticalLineLeft = false;
        tableVerticalLineRight = false;

        wxString alignString = GetArgData();

        // Count the number of columns
        noColumns = 0;
        size_t len = alignString.length();
        if (len > 0)
        {
          if (alignString[0] == '|')
            tableVerticalLineLeft = true;
          if (alignString[len-1] == '|')
            tableVerticalLineRight = true;
        }

        for (size_t i = 0; i < len; i++)
          if (isalpha(alignString[i]))
            noColumns ++;

/*
      // Experimental
      TexOutput(_T("\\brdrt\\brdrs"));
      if (tableVerticalLineLeft)
        TexOutput(_T("\\brdrl\\brdrs"));
      if (tableVerticalLineRight)
        TexOutput(_T("\\brdrr\\brdrs"));
*/

        // Calculate a rough size for each column
//        int tabPos = 80/noColumns;
        currentTab = 0;

        return false;
      }
    }
    else if (arg_no == 2 && !start)
    {
      inTable = false;
    }
    else if (arg_no == 2 && start)
      return true;
    break;
  }
  case ltMARGINPAR:
  case ltMARGINPAREVEN:
  case ltMARGINPARODD:
  case ltNORMALBOX:
  case ltNORMALBOXD:
  {
    if (start)
    {
      TexOutput(_T("----------------------------------------------------------------------\n"), true);
      return true;
    }
    else
      TexOutput(_T("\n----------------------------------------------------------------------\n"), true);
    break;
  }
  case ltBIBITEM:
  {
    wxChar buf[100];
    if (arg_no == 1 && start)
    {
      wxString citeKey = GetArgData();
      TexReferenceMap::iterator iTexRef = TexReferences.find(citeKey);
      if (iTexRef != TexReferences.end())
      {
        TexRef *ref = iTexRef->second;
        if (ref)
        {
          if (!ref->sectionNumber.empty())
          {
            ref->sectionNumber = wxEmptyString;
          }
          wxSnprintf(buf, sizeof(buf), _T("[%d]"), citeCount);
          ref->sectionNumber = buf;
        }
      }

      wxSnprintf(buf, sizeof(buf), _T("\\hy-%d{%ld}{[%d]} "), hyBLOCK_BOLD, NewBlockId(), citeCount);
      TexOutput(buf);
      citeCount ++;
      return false;
    }
    return true;
  }
  case ltTHEBIBLIOGRAPHY:
  {
    if (start && (arg_no == 1))
    {
      citeCount = 1;

      SetCurrentOutput(Chapters);

      SetCurrentOutputs(Contents, Chapters);
      long id1 = NewBlockId();
      long id2 = NewBlockId();
      wxFprintf(Contents, _T("\\hy-%d{%ld}{%s}\n"), hyBLOCK_SMALL_HEADING, id1, ReferencesNameString);
      wxFprintf(Chapters, _T("\\hy-%d{%ld}{%s}\n\n\n"), hyBLOCK_LARGE_VISIBLE_SECTION, id2, ReferencesNameString);
      wxFprintf(Index, _T("%ld %ld\n"), id1, id2);

      SetCurrentOutput(Chapters);
      return false;
    }
    if (!start && (arg_no == 2))
    {
    }
    return true;
  }
  case ltTWOCOLITEM:
  case ltTWOCOLITEMRULED:
  {
    if (start && (arg_no == 2))
      TexOutput(_T("\n    "));

    if (!start && (arg_no == 2))
      TexOutput(_T("\n"));
    return true;
  }
  /*
   * Accents
   *
   */
  case ltACCENT_GRAVE:
  {
    if (start)
    {
      wxString val = GetArgData();
      if (!val.empty())
      {
        if (val[0] == _T('a'))
        {
          TexOutput(_T("a"));
        }
        else if (val[0] == _T('e'))
        {
          TexOutput(_T("e"));
        }
        else if (val[0] == _T('i'))
        {
          TexOutput(_T("i"));
        }
        else if (val[0] == _T('o'))
        {
          TexOutput(_T("o"));
        }
        else if (val[0] == _T('u'))
        {
          TexOutput(_T("u"));
        }
        else if (val[0] == _T('A'))
        {
          TexOutput(_T("A"));
        }
        else if (val[0] == _T('E'))
        {
          TexOutput(_T("E"));
        }
        else if (val[0] == _T('I'))
        {
          TexOutput(_T("I"));
        }
        else if (val[0] == _T('O'))
        {
          TexOutput(_T("O"));
        }
        else if (val[0] == _T('U'))
        {
          TexOutput(_T("U"));
        }
      }
    }
    return false;
  }
  case ltACCENT_ACUTE:
  {
    if (start)
    {
      wxString val = GetArgData();
      if (!val.empty())
      {
        if (val[0] == _T('a'))
        {
          TexOutput(_T("a"));
        }
        else if (val[0] == _T('e'))
        {
          TexOutput(_T("e"));
        }
        else if (val[0] == _T('i'))
        {
          TexOutput(_T("i"));
        }
        else if (val[0] == _T('o'))
        {
          TexOutput(_T("o"));
        }
        else if (val[0] == _T('u'))
        {
          TexOutput(_T("u"));
        }
        else if (val[0] == _T('y'))
        {
          TexOutput(_T("y"));
        }
        else if (val[0] == _T('A'))
        {
          TexOutput(_T("A"));
        }
        else if (val[0] == _T('E'))
        {
          TexOutput(_T("E"));
        }
        else if (val[0] == _T('I'))
        {
          TexOutput(_T("I"));
        }
        else if (val[0] == _T('O'))
        {
          TexOutput(_T("O"));
        }
        else if (val[0] == _T('U'))
        {
          TexOutput(_T("U"));
        }
        else if (val[0] == _T('Y'))
        {
          TexOutput(_T("Y"));
        }
      }
    }
    return false;
  }
  case ltACCENT_CARET:
  {
    if (start)
    {
      wxString val = GetArgData();
      if (!val.empty())
      {
        if (val[0] == _T('a'))
        {
          TexOutput(_T("a"));
        }
        else if (val[0] == _T('e'))
        {
          TexOutput(_T("e"));
        }
        else if (val[0] == _T('i'))
        {
          TexOutput(_T("i"));
        }
        else if (val[0] == _T('o'))
        {
          TexOutput(_T("o"));
        }
        else if (val[0] == _T('u'))
        {
          TexOutput(_T("u"));
        }
        else if (val[0] == _T('A'))
        {
          TexOutput(_T("A"));
        }
        else if (val[0] == _T('E'))
        {
          TexOutput(_T("E"));
        }
        else if (val[0] == _T('I'))
        {
          TexOutput(_T("I"));
        }
        else if (val[0] == _T('O'))
        {
          TexOutput(_T("O"));
        }
        else if (val[0] == _T('U'))
        {
          TexOutput(_T("U"));
        }
      }
    }
    return false;
  }
  case ltACCENT_TILDE:
  {
    if (start)
    {
      wxString val = GetArgData();
      if (!val.empty())
      {
        if (val[0] == _T('a'))
        {
          TexOutput(_T("a"));
        }
        else if (val[0] == _T(' '))
        {
          TexOutput(_T("~"));
        }
        else if (val[0] == _T('n'))
        {
          TexOutput(_T("n"));
        }
        else if (val[0] == _T('o'))
        {
          TexOutput(_T("o"));
        }
        else if (val[0] == _T('A'))
        {
          TexOutput(_T("A"));
        }
        else if (val[0] == _T('N'))
        {
          TexOutput(_T("N"));
        }
        else if (val[0] == _T('O'))
        {
          TexOutput(_T("O"));
        }
      }
    }
    return false;
  }
  case ltACCENT_UMLAUT:
  {
    if (start)
    {
      wxString val = GetArgData();
      if (!val.empty())
      {
        if (val[0] == _T('a'))
        {
          TexOutput(_T("a"));
        }
        else if (val[0] == _T('e'))
        {
          TexOutput(_T("e"));
        }
        else if (val[0] == _T('i'))
        {
          TexOutput(_T("i"));
        }
        else if (val[0] == _T('o'))
        {
          TexOutput(_T("o"));
        }
        else if (val[0] == _T('u'))
        {
          TexOutput(_T("u"));
        }
        else if (val[0] == _T('y'))
        {
          TexOutput(_T("y"));
        }
        else if (val[0] == _T('A'))
        {
          TexOutput(_T("A"));
        }
        else if (val[0] == _T('E'))
        {
          TexOutput(_T("E"));
        }
        else if (val[0] == _T('I'))
        {
          TexOutput(_T("I"));
        }
        else if (val[0] == _T('O'))
        {
          TexOutput(_T("O"));
        }
        else if (val[0] == _T('U'))
        {
          TexOutput(_T("U"));
        }
        else if (val[0] == _T('Y'))
        {
          TexOutput(_T("Y"));
        }
      }
    }
    return false;
  }
  case ltACCENT_DOT:
  {
    if (start)
    {
      wxString val = GetArgData();
      if (!val.empty())
      {
        if (val[0] == 'a')
        {
          TexOutput(_T("a"));
        }
        else if (val[0] == 'A')
        {
          TexOutput(_T("A"));
        }
      }
    }
    return false;
  }
  case ltACCENT_CADILLA:
  {
    if (start)
    {
      wxString val = GetArgData();
      if (!val.empty())
      {
        if (val[0] == _T('c'))
        {
          TexOutput(_T("c"));
        }
        else if (val[0] == _T('C'))
        {
          TexOutput(_T("C"));
        }
      }
    }
    return false;
  }
  default:
  {
    return DefaultOnArgument(macroId, arg_no, start);
  }
  }
  return true;
}

bool XLPGo(void)
{
  xlpBlockId = 0;

  if (!InputFile.empty() && !OutputFile.empty())
  {
    Contents = wxFopen(TmpContentsName, _T("w"));
    Chapters = wxFopen(_T("chapters.xlp"), _T("w"));
    Sections = wxFopen(_T("sections.xlp"), _T("w"));
    Subsections = wxFopen(_T("subsections.xlp"), _T("w"));
    Subsubsections = wxFopen(_T("subsubsections.xlp"), _T("w"));
    Index = wxFopen(_T("index.xlp"), _T("w"));

    // Insert invisible section marker at beginning
    wxFprintf(Chapters, _T("\\hy-%d{%ld}{%s}\n"),
                hyBLOCK_INVISIBLE_SECTION, NewBlockId(), _T("\n"));

    wxFprintf(Contents, _T("\\hy-%d{%ld}{%s}\n\n"),
//                hyBLOCK_LARGE_HEADING, NewBlockId(), "\n\n%s\n\n", ContentsNameString);
                hyBLOCK_LARGE_HEADING, NewBlockId(), ContentsNameString);

    SetCurrentOutput(Chapters);

    wxFprintf(Index, _T("\n\\hyindex{\n\"%s\"\n"),
             contentsString ? contentsString : _T("WXHELPCONTENTS"));
    TraverseDocument();

    for (IntHashMap::iterator iHyperLink = hyperLinks.begin(); iHyperLink != hyperLinks.end(); ++iHyperLink)
    {
      long from = iHyperLink->first;
      wxString label = iHyperLink->second;
      StringLongMap::iterator iOther = hyperLabels.find(label);
      if (iOther != hyperLabels.end())
      {
        long to = iOther->second;
        wxFprintf(Index, _T("%ld %ld\n"), from, to);
      }
    }

    wxFprintf(Index, _T("}\n"));

    fclose(Contents); Contents = NULL;
    fclose(Chapters); Chapters = NULL;
    fclose(Sections); Sections = NULL;
    fclose(Subsections); Subsections = NULL;
    fclose(Subsubsections); Subsubsections = NULL;
    fclose(Index); Index = NULL;

    if (wxFileExists(ContentsName)) wxRemoveFile(ContentsName);

    if (!wxRenameFile(TmpContentsName, ContentsName))
    {
      wxCopyFile(TmpContentsName, ContentsName);
      wxRemoveFile(TmpContentsName);
    }

    wxConcatFiles(_T("chapters.xlp"), _T("sections.xlp"), _T("tmp2.xlp"));
    wxConcatFiles(_T("tmp2.xlp"), _T("subsections.xlp"), _T("tmp1.xlp"));
    wxConcatFiles(_T("tmp1.xlp"), _T("subsubsections.xlp"), _T("tmp2.xlp"));
    wxConcatFiles(_T("tmp2.xlp"), _T("index.xlp"), OutputFile);

    wxRemoveFile(_T("tmp1.xlp"));
    wxRemoveFile(_T("tmp2.xlp"));

    wxRemoveFile(_T("chapters.xlp"));
    wxRemoveFile(_T("sections.xlp"));
    wxRemoveFile(_T("subsections.xlp"));
    wxRemoveFile(_T("subsubsections.xlp"));
    wxRemoveFile(_T("index.xlp"));
    return true;
  }
  return false;
}
