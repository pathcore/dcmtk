/*
 *
 *  Copyright (C) 2018, OFFIS e.V.
 *  All rights reserved.  See COPYRIGHT file for details.
 *
 *  This software and supporting documentation were developed by
 *
 *    OFFIS e.V.
 *    R&D Division Health
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *
 *  Module:  dcmdata
 *
 *  Author:  Pedro Arizpe
 *
 *  Purpose: Implementation of Document encapsulation
 *
 */

//make sure OS specific configuration is included first
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/dcmdata/dccodec.h"
//for tag constants
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcencdoc.h"
//for dcmtk version name
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/ofstd/ofdatime.h"
#include "dcmtk/ofstd/ofstd.h"
#include "dcmtk/ofstd/ofstdinc.h"

#define SHORTCOL 3
#define LONGCOL 21

//exit codes for this command line tool
//(common codes are defined in "ofexit.h" * included from "ofconapp.h")

// general errors
#define EXITCODE_MEMORY_EXHAUSTED                4

DcmEncapsulatedDocument::DcmEncapsulatedDocument() :
  opt_patientBirthdate(),
  opt_patientID(),
  opt_patientName(),
  opt_patientSex(),

  opt_conceptCM(),
  opt_conceptCSD(),
  opt_conceptCV(),

  opt_documentTitle(),
  opt_seriesFile(),
  opt_seriesUID(),
  opt_studyUID(),

  opt_oenctype(EET_ExplicitLength),
  opt_writeMode(EWM_fileformat),
  opt_oglenc(EGL_withoutGL),
  opt_opadenc(EPD_withoutPadding),
  opt_oxfer(EXS_LittleEndianExplicit),
  opt_filepad(0),
  opt_itempad(0),

  opt_readSeriesInfo(OFFalse),
  opt_annotation(OFTrue),
  opt_increment(OFFalse),

  opt_instance(1),
  opt_overrideKeys(),

  cda_mediaTypes(),
  opt_override(OFFalse)
{
}

OFBool DcmEncapsulatedDocument::XMLsearchAttribute(
  XMLNode currnode,
  OFList<OFString> *results,
  OFString attr)
{
  OFBool found = OFFalse;
  if (currnode.nChildNode() == 0)
  {
    //"currnode has no children (leaf)";
    if (currnode.isAttributeSet(attr.c_str()))
    {//attribute found on leaf
      results->push_back(OFSTRING_GUARD(currnode.getAttribute(attr.c_str())));
      found = OFTrue;
    }
  }
  else {
    //"currnode has children (branch)";
    if (currnode.isAttributeSet(attr.c_str()))
    {//attribute found on branch
      results->push_back(OFSTRING_GUARD(currnode.getAttribute(attr.c_str())));
      found = OFTrue;
    }
    for (int i = 0; i < currnode.nChildNode(); i++)
    {//search all children recursively
      OFBool childfound = XMLsearchAttribute(currnode.getChildNode(i), results, attr);
      found |= childfound;
    }
  }
  return found;
}

OFString DcmEncapsulatedDocument::XMLgetAllAttributeValues(
  XMLNode fileNode,
  OFString attr)
{
  OFList<OFString> attributeValueslist;
  OFString attributeValues;
  if (XMLsearchAttribute(fileNode, &attributeValueslist, attr))
  {
    //If the Attribute is mediaType, initialize with text/xml to exclude
    //the primary MIME Type of the encapsulated document
    if (attr == "mediaType")attributeValues.append("text/xml");
    while (!attributeValueslist.empty())
    {
      if (attributeValues.find(attributeValueslist.front()) == OFString_npos) {
        if (!attributeValues.empty())attributeValues.append("\\");
        attributeValues.append(attributeValueslist.front());
      }
      attributeValueslist.pop_front();
    }
    //remove the primary MIME Type of the
    //encapsulated document
    if (attr == "mediaType")
    {
      if (attributeValues.size() > 9)
        attributeValues.erase(0, 9);
      else
        attributeValues = "";
    }
  }
  return attributeValues;
}

OFString DcmEncapsulatedDocument::XMLgetAttribute(XMLNode fileNode, DcmTagKey attr)
{
  OFString result = "";
  if (attr == DCM_DocumentTitle) {
    if (fileNode.getChildNode("title").getText() != NULL) {
      result = OFString(OFSTRING_GUARD(fileNode.getChildNode("title").getText()));
    }
  }
  /*if (attr == DCM_PatientNameExtension) {
  result = OFString(OFSTRING_GUARD(fileNode.getChildNodeByPath("recordTarget/patientRole/patient/name").getAttribute("use")));
  }*/
  if (attr == DCM_PatientName) {
    result = OFString(OFSTRING_GUARD(
      fileNode.getChildNodeByPath("recordTarget/patientRole/patient/name/family").getText())) + "^" + OFString(OFSTRING_GUARD(
        fileNode.getChildNodeByPath("recordTarget/patientRole/patient/name").getChildNode("given", 0).getText())) + "^" + OFString(OFSTRING_GUARD(
          fileNode.getChildNodeByPath("recordTarget/patientRole/patient/name").getChildNode("given", 1).getText())) + "^" + OFString(OFSTRING_GUARD(
            fileNode.getChildNodeByPath("recordTarget/patientRole/patient/name/prefix").getText())) + "^" + OFString(OFSTRING_GUARD(
              fileNode.getChildNodeByPath("recordTarget/patientRole/patient/name/suffix").getText()));
  }
  if (attr == DCM_PatientSex) {
    result = OFString(OFSTRING_GUARD(fileNode.getChildNodeByPath("recordTarget/patientRole/patient/administrativeGenderCode").getAttribute("code")));
  }
  if (attr == DCM_PatientBirthDate) {
    result = OFString(OFSTRING_GUARD(fileNode.getChildNodeByPath("recordTarget/patientRole/patient/birthTime").getAttribute("value")));
  }
  //Table A.8-1. Basic Code Attributes Mapping to HL7 V3 Code Data Types (CV, CS, CE and CD)
  if (attr == DCM_PatientID)
  {
    result = OFString(OFSTRING_GUARD(fileNode.getChildNodeByPath("recordTarget/patientRole/id").getAttribute("extension")));
  }
  if (attr == DCM_CodeValue)//Code Value
  {
    result = OFString(OFSTRING_GUARD(fileNode.getChildNode("code").getAttribute("code")));
  }
  if (attr == DCM_CodingSchemeUID)//Coding Scheme UID (PS3.16)
  {
    result = OFString(OFSTRING_GUARD(fileNode.getChildNode("code").getAttribute("codeSystem")));
  }
  if (attr == DCM_CodingSchemeDesignator)//Coding Scheme Designator (0008,0102)
  {
    result = OFString(OFSTRING_GUARD(fileNode.getChildNode("code").getAttribute("codeSystemName")));
  }
  if (attr == DCM_CodingSchemeVersion)//Coding Scheme Version (0008,0103)
  {
    result = OFString(OFSTRING_GUARD(fileNode.getChildNode("code").getAttribute("codeSystemVersion")));
  }
  if (attr == DCM_CodeMeaning)//Code Meaning (0008,0104)
  {
    result = OFString(OFSTRING_GUARD(fileNode.getChildNode("code").getAttribute("displayName")));
  }
  return result;
}

int DcmEncapsulatedDocument::getCDAData(const char *filename, OFLogger &appLogger)
{
  XMLResults err;
  XMLNode fileNode = XMLNode::parseFile(filename, "ClinicalDocument", &err);
  //checks if the XML file is correctly formatted
  if (0 != err.error)
  {
    OFLOG_ERROR(appLogger, fileNode.getError(err.error));
    return EXITCODE_INVALID_INPUT_FILE;
  }

  //get all media types from CDA file
  cda_mediaTypes = XMLgetAllAttributeValues(fileNode, "mediaType");
  //read and compare patient information from CDA File
  OFString pID = XMLgetAttribute(fileNode, DCM_PatientID);
  if ((pID != "") && (opt_patientID != pID))
  {
    if (opt_patientID != "") {
      //if override option is inactive, return an error
      if (!opt_override) {
        OFLOG_ERROR(appLogger, "Patient ID mismatch:" << OFendl
          << "Found in the CDA file : " << pID << OFendl
          << "Entered (or found in DCM file): " << opt_patientID << OFendl
          << "If you wish to override, run again with +ov");
        return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
      }
      else {
        OFLOG_WARN(appLogger, "Patient ID mismatch:" << OFendl
          << "Found in the CDA file : " << pID << OFendl
          << "Provided (in DCM file): " << opt_patientID);
      }
    }
    else opt_patientID = pID;
  }
  OFString pBirthDate = XMLgetAttribute(fileNode, DCM_PatientBirthDate);
  if ((pBirthDate != "") && (opt_patientBirthdate != pBirthDate))
  {
    if (opt_patientBirthdate != "") {
      if (!opt_override) {
        OFLOG_ERROR(appLogger, "Patient Birth Date mismatch:" << OFendl
          << "Found in the CDA file : " << pBirthDate << OFendl
          << "Provided (in DCM file): " << opt_patientBirthdate << OFendl
          << "If you wish to override, run again with +ov");
        return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
      }
      else {
        OFLOG_WARN(appLogger, "Patient Birth Date mismatch:" << OFendl
          << "Found in the CDA file : " << pBirthDate << OFendl
          << "Provided (in DCM file): " << opt_patientBirthdate);
      }
    }
    else opt_patientBirthdate = pBirthDate;
  }
  OFString pSex = XMLgetAttribute(fileNode, DCM_PatientSex);
  if ((pSex != "") && (opt_patientSex != pSex))
  {
    if (opt_patientSex != "") {
      if (!opt_override) {
        OFLOG_ERROR(appLogger, "Patient Sex mismatch:" << OFendl
          << "Found in the CDA file : " << pSex << OFendl
          << "Provided (in DCM file): " << opt_patientSex << OFendl
          << "If you wish to override, run again with +ov");
        return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
      }
      else {
        OFLOG_WARN(appLogger, "Patient Sex mismatch:" << OFendl
          << "Found in the CDA file : " << pSex << OFendl
          << "Provided (in DCM file): " << opt_patientSex);
      }
    }
    else opt_patientSex = pSex;
  }
  OFString pName = XMLgetAttribute(fileNode, DCM_PatientName);
  if ((pName != "^^^^") && (opt_patientName != pName))
  {
    if (opt_patientName != "") {
      if (!opt_override) {
        OFLOG_ERROR(appLogger, "Patient Name mismatch:" << OFendl
          << "Found in the CDA file : " << pName << OFendl
          << "Provided (in DCM file): " << opt_patientName << OFendl
          << "If you wish to override, run again with +ov");
        return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
      }
      else {
        OFLOG_WARN(appLogger, "Patient Name mismatch:" << OFendl
          << "Found in the CDA file : " << pName << OFendl
          << "Provided (in DCM file): " << opt_patientName);
      }
    }
    else opt_patientName = pName;
  }
  //get document title from CDA
  OFString dTitle = XMLgetAttribute(fileNode, DCM_DocumentTitle);
  if (opt_documentTitle == "") {
    if (opt_conceptCSD != "") opt_documentTitle = opt_conceptCSD;
    if (opt_conceptCV != "") opt_documentTitle = opt_conceptCV;
    if (opt_conceptCM != "") opt_documentTitle = opt_conceptCM;
  }

  if ((dTitle != "") && (opt_documentTitle != dTitle))
  {
    if (opt_documentTitle != "") {
      if (!opt_override) {
        OFLOG_ERROR(appLogger, "Document Title mismatch:" << OFendl
          << "Found in the CDA file : " << dTitle << OFendl
          << "Provided (in DCM file): " << opt_documentTitle << OFendl
          << "If you wish to override, run again with +ov");
        return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
      }
      else {
        OFLOG_WARN(appLogger, "Document Title mismatch:" << OFendl
          << "Found in the CDA file : " << dTitle << OFendl
          << "Provided (in DCM file): " << opt_documentTitle);
      }
    }
    else opt_documentTitle = dTitle;
  }
  //get Concept information from CDA
  OFString cCSD = XMLgetAttribute(fileNode, DCM_CodingSchemeDesignator);
  if ((cCSD != "") && (opt_conceptCSD != cCSD))
  {
    if (opt_conceptCSD != "") {
      if (!opt_override) {
        OFLOG_ERROR(appLogger, "concept CSD mismatch:" << OFendl
          << "Found in the CDA file : " << cCSD << OFendl
          << "Provided (in DCM file): " << opt_conceptCSD << OFendl
          << "If you wish to override, run again with +ov");
        return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
      }
      else {
        OFLOG_WARN(appLogger, "concept CSD mismatch:" << OFendl
          << "Found in the CDA file : " << cCSD << OFendl
          << "Provided (in DCM file): " << opt_conceptCSD);
      }
    }
    else opt_conceptCSD = cCSD;
  }
  OFString cCV = XMLgetAttribute(fileNode, DCM_CodeValue);
  if ((cCV != "") && (opt_conceptCV != cCV))
  {
    if (opt_conceptCV != "") {
      if (!opt_override) {
        OFLOG_ERROR(appLogger, "concept CV mismatch:" << OFendl
          << "Found in the CDA file : " << cCV << OFendl
          << "Provided (in DCM file): " << opt_conceptCV << OFendl
          << "If you wish to override, run again with +ov");
        return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
      }
      else {
        OFLOG_WARN(appLogger, "concept CV mismatch:" << OFendl
          << "Found in the CDA file : " << cCV << OFendl
          << "Provided (in DCM file): " << opt_conceptCV);
      }
    }
    else opt_conceptCV = cCV;
  }
  OFString cCM = XMLgetAttribute(fileNode, DCM_CodeMeaning);
  if ((cCM != "") && (opt_conceptCM != cCM))
  {
    if (opt_conceptCM != "") {
      if (!opt_override) {
        OFLOG_ERROR(appLogger, "concept CM mismatch:" << OFendl
          << "Found in the CDA file : " << cCM << OFendl
          << "Provided (in DCM file): " << opt_conceptCM << OFendl
          << "If you wish to override, run again with +ov");
        return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
      }
      else {
        OFLOG_WARN(appLogger, "concept CM mismatch:" << OFendl
          << "Found in the CDA file : " << cCM << OFendl
          << "Provided (in DCM file): " << opt_conceptCM);
      }
    }
    else opt_conceptCM = cCM;
  }
  return EXITCODE_NO_ERROR;
}

void DcmEncapsulatedDocument::addCDACommandlineOptions(OFCommandLine &cmd)
{
  cmd.setOptionColumns(LONGCOL, SHORTCOL);
  cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
  cmd.addParam(     "cdafile-in",                         "CDA input filename to be converted");
  cmd.addParam(     "dcmfile-out",                        "DICOM output filename");
  addGeneralOptions(cmd);
  addDocumentOptions(cmd);
  cmd.addSubGroup("burned-in annotation:");
      cmd.addOption("--annotation-yes",       "+an",      "CDA file (or an encapsulated document within)\ncontains patient identifying data (default)");
      cmd.addOption("--annotation-no",        "-an",      "CDA file (or an encapsulated document within)\ndoes not contain patient identifying data");
    cmd.addSubGroup("override CDA data:");
      cmd.addOption("--no-override",          "-ov",      "CDA patient and document data must match study,\nseries or manually entered information (default)");
      cmd.addOption("--override",             "+ov",      "CDA's data will be overwritten by study, series\nor manually entered information");
  addOutputOptions(cmd);
}

void DcmEncapsulatedDocument::addPDFCommandlineOptions(OFCommandLine &cmd)
{
  cmd.setOptionColumns(LONGCOL, SHORTCOL);
  cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
  cmd.addParam(     "pdffile-in",                         "PDF input filename to be converted");
  cmd.addParam(     "dcmfile-out",                        "DICOM output filename");
  addGeneralOptions(cmd);
  addDocumentOptions(cmd);
  cmd.addSubGroup("burned-in annotation:");
      cmd.addOption("--annotation-yes",       "+an",      "PDF file (or an image within)\ncontains patient identifying data (default)");
      cmd.addOption("--annotation-no",        "-an",      "PDF file (or an image within)\ndoes not contain patient identifying data");
  addOutputOptions(cmd);
}
void DcmEncapsulatedDocument::addSTLCommandlineOptions(OFCommandLine &cmd)
{
  cmd.setOptionColumns(LONGCOL, SHORTCOL);
  cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
  cmd.addParam(     "stlfile-in"                          "STL input filename to be converted");
  cmd.addParam(     "dcmfile-out",                        "DICOM output filename");
  addGeneralOptions(cmd);
  addDocumentOptions(cmd);
  cmd.addSubGroup("burned-in annotation:");
      cmd.addOption("--annotation-yes",       "+an",      "STL file contains patient identifying data (default)");
      cmd.addOption("--annotation-no",        "-an",      "STL file does not contain patient identifying data");
  addOutputOptions(cmd);
}

void DcmEncapsulatedDocument::addGeneralOptions(OFCommandLine &cmd)
{
  cmd.addGroup(     "general options:", LONGCOL, SHORTCOL + 2);
    cmd.addOption(  "--help",                 "-h",       "print this help text and exit",          OFCommandLine::AF_Exclusive);
    cmd.addOption(  "--version",                          "print version information and exit",     OFCommandLine::AF_Exclusive);
  OFLog::addOptions(cmd);
}

void DcmEncapsulatedDocument::addDocumentOptions(OFCommandLine &cmd)
{
  cmd.addGroup("DICOM document options:");
    cmd.addSubGroup("document title:");
      cmd.addOption("--title",                "+t",   1,  "[t]itle: string (default: empty)",
          "document title");
      cmd.addOption("--concept-name",         "+cn",  3,  "[CSD] [CV] [CM]: string (default: empty)",
                                                          "document title as concept name code sequence\nwith coding scheme designator CSD, code value CV\nand code meaning CM");
    cmd.addSubGroup("patient data:");
      cmd.addOption("--patient-name",         "+pn",  1,  "[n]ame: string",
                                                          "patient's name in DICOM PN syntax");
      cmd.addOption("--patient-id",           "+pi",  1,  "[i]d: string",
                                                          "patient identifier");
      cmd.addOption("--patient-birthdate",    "+pb",  1,  "[d]ate: string (YYYYMMDD)",
                                                          "patient's birth date");
      cmd.addOption("--patient-sex",          "+ps",  1,  "[s]ex: string (M, F or O)",
                                                          "patient's sex");
    cmd.addSubGroup("other processing options:");
      cmd.addOption("--key",                  "-k",   1,  "[k]ey: gggg,eeee=\"str\", path or dict. name=\"str\"",
                                                          "add further attribute");
    cmd.addSubGroup("study and series:");
      cmd.addOption("--generate",             "+sg",      "generate new study and series UIDs (default)");
      cmd.addOption("--study-from",           "+st",  1,  "[f]ilename: string",
                                                          "read patient/study data from DICOM file");
      cmd.addOption("--series-from",          "+se",  1,  "[f]ilename: string",
                                                          "read patient/study/series data from DICOM file");
    cmd.addSubGroup("instance number:");
      cmd.addOption("--instance-one",         "+i1",      "use instance number 1 (default, not with +se)");
      cmd.addOption("--instance-inc",         "+ii",      "increment instance number (only with +se)");
      cmd.addOption("--instance-set",         "+is",  1,  "[i]nstance number: integer", "use instance number i");
}

void DcmEncapsulatedDocument::addOutputOptions(OFCommandLine &cmd)
{
  cmd.addGroup("output options:");
    cmd.addSubGroup("output transfer syntax:");
      cmd.addOption("--write-xfer-little",    "+te",      "write with explicit VR little endian (default)");
      cmd.addOption("--write-xfer-big",       "+tb",      "write with explicit VR big endian TS");
      cmd.addOption("--write-xfer-implicit",  "+ti",      "write with implicit VR little endian TS");
    cmd.addSubGroup("group length encoding:");
      cmd.addOption("--group-length-recalc",  "+g=",      "recalculate group lengths if present (default)");
      cmd.addOption("--group-length-create",  "+g",       "always write with group length elements");
      cmd.addOption("--group-length-remove",  "-g",       "always write without group length elements");
    cmd.addSubGroup("length encoding in sequences and items:");
      cmd.addOption("--length-explicit",      "+e",       "write with explicit lengths (default)");
      cmd.addOption("--length-undefined",     "-e",       "write with undefined lengths");
    cmd.addSubGroup("data set trailing padding (not with --write-dataset):");
      cmd.addOption("--padding-retain",       "-p=",      "do not change padding (default)");
      cmd.addOption("--padding-off",          "-p",       "no padding (implicit if --write-dataset)");
      cmd.addOption("--padding-create",       "+p",  2,   "[f]ile-pad [i]tem-pad: integer",
                                                          "align file on multiple of f bytes\nand items on multiple of i bytes");
}

void DcmEncapsulatedDocument::parseArguments(OFConsoleApplication& app, OFCommandLine& cmd)
{
    /* command line parameters and options */
    cmd.getParam(1, opt_ifname);
    cmd.getParam(2, opt_ofname);

    OFLog::configureFromCommandLine(cmd, app);

    dcmEnableGenerationOfNewVRs();

    cmd.beginOptionBlock();
    if (cmd.findOption("--generate"))
    {
      opt_seriesFile = "";
      opt_readSeriesInfo = OFFalse;
    }

    if (cmd.findOption("--series-from"))
    {
      app.checkValue(cmd.getValue(opt_seriesFile));
      opt_readSeriesInfo = OFTrue;
    }

    if (cmd.findOption("--study-from"))
    {
      app.checkValue(cmd.getValue(opt_seriesFile));
      opt_readSeriesInfo = OFFalse;
    }
    cmd.endOptionBlock();
    cmd.beginOptionBlock();
    if (cmd.findOption("--title"))
    {
      app.checkValue(cmd.getValue(opt_documentTitle));
    }
    if (cmd.findOption("--concept-name"))
    {
      app.checkValue(cmd.getValue(opt_conceptCSD));
      app.checkValue(cmd.getValue(opt_conceptCV));
      app.checkValue(cmd.getValue(opt_conceptCM));
    }
    cmd.endOptionBlock();
    cmd.beginOptionBlock();
    if (cmd.findOption("--patient-name"))
    {
      app.checkValue(cmd.getValue(opt_patientName));
      app.checkConflict("--patient-name", "--study-from or --series-from", opt_seriesFile != "");
    }
    if (cmd.findOption("--patient-id"))
    {
      app.checkValue(cmd.getValue(opt_patientID));
      app.checkConflict("--patient-id", "--study-from or --series-from", opt_seriesFile != "");
    }
    if (cmd.findOption("--patient-birthdate"))
    {
      app.checkValue(cmd.getValue(opt_patientBirthdate));
      app.checkConflict("--patient-birthdate", "--study-from or --series-from", opt_seriesFile != "");
    }
    if (cmd.findOption("--patient-sex"))
    {
      app.checkValue(cmd.getValue(opt_patientSex));
      app.checkConflict("--patient-sex", "--study-from or --series-from", opt_seriesFile != "");
    }
    cmd.endOptionBlock();
    cmd.beginOptionBlock();
    if (cmd.findOption("--annotation-yes"))
    {
      opt_annotation = OFTrue;
    }
    if (cmd.findOption("--annotation-no"))
    {
      opt_annotation = OFTrue;
    }
    cmd.endOptionBlock();

    cmd.beginOptionBlock();
    if (cmd.findOption("--write-xfer-little")) opt_oxfer = EXS_LittleEndianExplicit;
    if (cmd.findOption("--write-xfer-big")) opt_oxfer = EXS_BigEndianExplicit;
    if (cmd.findOption("--write-xfer-implicit")) opt_oxfer = EXS_LittleEndianImplicit;
    cmd.endOptionBlock();

    cmd.beginOptionBlock();
    if (cmd.findOption("--group-length-recalc")) opt_oglenc = EGL_recalcGL;
    if (cmd.findOption("--group-length-create")) opt_oglenc = EGL_withGL;
    if (cmd.findOption("--group-length-remove")) opt_oglenc = EGL_withoutGL;
    cmd.endOptionBlock();

    cmd.beginOptionBlock();
    if (cmd.findOption("--length-explicit")) opt_oenctype = EET_ExplicitLength;
    if (cmd.findOption("--length-undefined")) opt_oenctype = EET_UndefinedLength;
    cmd.endOptionBlock();

    cmd.beginOptionBlock();
    if (cmd.findOption("--padding-retain"))
    {
      app.checkConflict("--padding-retain", "--write-dataset", opt_writeMode == EWM_dataset);
      opt_opadenc = EPD_noChange;
    }
    if (cmd.findOption("--padding-off")) opt_opadenc = EPD_withoutPadding;
    if (cmd.findOption("--padding-create"))
    {
      app.checkConflict("--padding-create", "--write-dataset", opt_writeMode == EWM_dataset);
      app.checkValue(cmd.getValueAndCheckMin(opt_filepad, 0));
      app.checkValue(cmd.getValueAndCheckMin(opt_itempad, 0));
      opt_opadenc = EPD_withPadding;
    }
    cmd.endOptionBlock();

    // create override attribute dataset
    if (cmd.findOption("--key", 0, OFCommandLine::FOM_FirstFromLeft))
    {
      const char *ovKey = NULL;
      do {
        app.checkValue(cmd.getValue(ovKey));
        opt_overrideKeys.push_back(ovKey);
      } while (cmd.findOption("--key", 0, OFCommandLine::FOM_NextFromLeft));
      }
      setOverrideKeys(opt_overrideKeys);
      // initialize default for --series-from
      if (opt_seriesFile!="" && opt_readSeriesInfo) opt_increment = OFTrue;

      cmd.beginOptionBlock();
      if (cmd.findOption("--instance-one"))
      {
        app.checkConflict("--instance-one", "--series-from", (opt_seriesFile != "") && opt_readSeriesInfo);
        opt_increment = OFFalse;
        opt_instance = 1;
      }
      if (cmd.findOption("--instance-inc"))
      {
        app.checkDependence("--instance-inc", "--series-from", (opt_seriesFile != "") && opt_readSeriesInfo);
        opt_increment = OFTrue;
      }
      if (cmd.findOption("--instance-set"))
      {
        opt_increment = OFFalse;
        app.checkValue(cmd.getValueAndCheckMin(opt_instance, 1));
      }
      cmd.endOptionBlock();
    }

OFCondition DcmEncapsulatedDocument::createIdentifiers(OFLogger& appLogger)
{
  char buf[100];
  OFCondition cond = EC_Normal;
  Sint32 incrementedInstance = 0;
  if (opt_seriesFile != "")
  {
    DcmFileFormat dfile;
    cond = dfile.loadFile(opt_seriesFile, EXS_Unknown, EGL_noChange);
    if (cond.bad())
    {
      OFLOG_WARN(appLogger, cond.text() << ": reading file: " << opt_seriesFile);
    }
    else
    {
      const char *c = NULL;
      DcmDataset *dset = dfile.getDataset();
      if (dset)
      {
        // read patient attributes
        c = NULL;
        if (dset->findAndGetString(DCM_PatientName, c).good() && c) opt_patientName = c;
        c = NULL;
        if (dset->findAndGetString(DCM_PatientID, c).good() && c)opt_patientID = c;
        c = NULL;
        if (dset->findAndGetString(DCM_PatientBirthDate, c).good() && c)opt_patientBirthdate = c;
        c = NULL;
        if (dset->findAndGetString(DCM_PatientSex, c).good() && c)opt_patientSex = c;

        // read study attributes
        c = NULL;
        if (dset->findAndGetString(DCM_StudyInstanceUID, c).good() && c) opt_studyUID = c;

        // read series attributes
        if (opt_readSeriesInfo)
        {
          c = NULL;
          if (dset->findAndGetString(DCM_SeriesInstanceUID, c).good() && c) opt_seriesUID = c;
          if (dset->findAndGetSint32(DCM_InstanceNumber, incrementedInstance).good()) ++incrementedInstance; else incrementedInstance = 0;
          if (opt_increment) opt_instance = incrementedInstance;
        }
      }
    }
  }
  if (opt_studyUID.empty())
  {
    dcmGenerateUniqueIdentifier(buf, SITE_STUDY_UID_ROOT);
    opt_studyUID = buf;
  }
  if (opt_seriesUID.empty())
  {
    dcmGenerateUniqueIdentifier(buf, SITE_SERIES_UID_ROOT);
    opt_seriesUID = buf;
  }
  return cond;

}

int DcmEncapsulatedDocument::insertEncapsulatedDocument(
  DcmItem *dataset,
  OFLogger& appLogger)
{
  size_t fileSize = 0;
  struct stat fileStat;
  char buf[100];

  if (0 == stat(opt_ifname.c_str(), &fileStat)) fileSize = OFstatic_cast(size_t, fileStat.st_size);
  else
  {
    OFLOG_ERROR(appLogger, "file " << opt_ifname << " not found");
    return EXITCODE_NO_INPUT_FILES;
  }

  if (fileSize == 0)
  {
    OFLOG_ERROR(appLogger, "file " << opt_ifname << " is empty");
    return EXITCODE_INVALID_INPUT_FILE;
  }

  FILE *encapfile = fopen(opt_ifname.c_str(), "rb");
  if (encapfile == NULL)
  {
    OFLOG_ERROR(appLogger, "unable to read file " << opt_ifname);
    return EXITCODE_CANNOT_READ_INPUT_FILE;
  }

  size_t buflen = 100;
  if (fileSize < buflen) buflen = fileSize;
  if (buflen != fread(buf, 1, buflen, encapfile))
  {
    OFLOG_ERROR(appLogger, "read error in file " << opt_ifname);
    fclose(encapfile);
    return EXITCODE_INVALID_INPUT_FILE;
  }
  if (appLogger.getName() == "dcmtk.apps.pdf2dcm")
  {
    // check magic word for PDF file
    if (0 != strncmp("%PDF-", buf, 5))
    {
      OFLOG_ERROR(appLogger, "file " << opt_ifname << " is not a PDF file");
      fclose(encapfile);
      return EXITCODE_INVALID_INPUT_FILE;
    }

    // check PDF version number
    char *version = buf + 5;
    OFBool found = OFFalse;
    for (int i = 0; i < 5; ++i)
    {
      if (version[i] == 10 || version[i] == 13)
      {
        version[i] = 0; // insert end of string
        found = OFTrue;
        break;
      }
    }

    if (!found)
    {
      OFLOG_ERROR(appLogger, "file " << opt_ifname << ": unable to decode PDF version number");
      fclose(encapfile);
      return EXITCODE_INVALID_INPUT_FILE;
    }

    OFLOG_INFO(appLogger, "file " << opt_ifname << ": PDF " << version << ", " << (fileSize + 1023) / 1024 << "kB");
  }

  if (0 != fseek(encapfile, 0, SEEK_SET))
  {
    OFLOG_ERROR(appLogger, "file " << opt_ifname << ": seek error");
    fclose(encapfile);
    return EXITCODE_CANNOT_READ_INPUT_FILE;
  }

  OFCondition result = EC_Normal;
  DcmPolymorphOBOW *elem = new DcmPolymorphOBOW(DCM_EncapsulatedDocument);
  if (elem)
  {
    size_t numBytes = fileSize;
    if (numBytes & 1) ++numBytes;
    Uint8 *bytes = NULL;
    result = elem->createUint8Array(OFstatic_cast(Uint32, numBytes), bytes);
    if (result.good())
    {
      // blank pad byte
      bytes[numBytes - 1] = 0;

      // read file content
      if (fileSize != fread(bytes, 1, fileSize, encapfile))
      {
        OFLOG_ERROR(appLogger, "read error in file " << opt_ifname);
        return EXITCODE_CANNOT_READ_INPUT_FILE;
      }
    }
  }
  else {
    fclose(encapfile);
    return EXITCODE_MEMORY_EXHAUSTED;
  }

  // if successful, insert element into dataset
  if (result.good()) result = dataset->insert(elem); else delete elem;

  // close file
  fclose(encapfile);
  if (result.good())return EXITCODE_NO_ERROR;
  else return EXITCODE_CANNOT_WRITE_OUTPUT_FILE;
}


OFCondition DcmEncapsulatedDocument::createHeader(
  DcmItem *dataset,
  OFLogger& logger,
  const char *opt_mediaTypes)
{
  OFCondition result = EC_Normal;
  char buf[80];

  // insert empty type 2 attributes
  if (result.good()) result = dataset->insertEmptyElement(DCM_StudyDate);
  if (result.good()) result = dataset->insertEmptyElement(DCM_StudyTime);
  if (result.good()) result = dataset->insertEmptyElement(DCM_AccessionNumber);
  if (result.good()) result = dataset->insertEmptyElement(DCM_Manufacturer);
  if (result.good()) result = dataset->insertEmptyElement(DCM_ReferringPhysicianName);
  if (result.good()) result = dataset->insertEmptyElement(DCM_StudyID);
  if (result.good()) result = dataset->insertEmptyElement(DCM_ContentDate);
  if (result.good()) result = dataset->insertEmptyElement(DCM_ContentTime);
  if (result.good()) result = dataset->insertEmptyElement(DCM_AcquisitionDateTime);
  if (result.good()) {
    if (opt_conceptCSD!="" && opt_conceptCV != "" && opt_conceptCM != "")
    {
      result = DcmCodec::insertCodeSequence(dataset, DCM_ConceptNameCodeSequence,
                                            opt_conceptCSD.c_str(), opt_conceptCV.c_str(),
                                            opt_conceptCM.c_str());
    }
    else
    {
      result = dataset->insertEmptyElement(DCM_ConceptNameCodeSequence);
    }
  }
  // insert const value attributes
  if (result.good()) result = dataset->putAndInsertString(DCM_SpecificCharacterSet, "ISO_IR 100");
  //insert encapsulated file storage UID (CDA/PDF/STL)
  if (result.good()) {
    if (logger.getName() == "dcmtk.apps.pdf2dcm"){
      result = dataset->putAndInsertString(DCM_SOPClassUID, UID_EncapsulatedPDFStorage);
    }
    if (logger.getName() == "dcmtk.apps.cda2dcm"){
      result = dataset->putAndInsertString(DCM_SOPClassUID, UID_EncapsulatedCDAStorage);
    }
    if (logger.getName() == "dcmtk.apps.stl2dcm") {
      result = dataset->putAndInsertString(DCM_SOPClassUID, UID_EncapsulatedSTLStorage);
    }
  }
  // we are now using "DOC" for the modality, which seems to be more appropriate than "OT" (see CP-749)
  if (result.good()) result = dataset->putAndInsertString(DCM_Modality, "DOC");
  if (result.good()) result = dataset->putAndInsertString(DCM_ConversionType, "WSD");
  // according to C.24.2.1 on part 3, (0042,0012) is text/XML for CDA.
  if (result.good()) result = dataset->putAndInsertString(DCM_MIMETypeOfEncapsulatedDocument, "text/XML");
  // there is no way we could determine a meaningful series number, so we just use a constant.
  if (result.good()) result = dataset->putAndInsertString(DCM_SeriesNumber, "1");

  // insert variable value attributes
  if (result.good()) result = dataset->putAndInsertString(DCM_DocumentTitle, opt_documentTitle.c_str());
  if (result.good()) result = dataset->putAndInsertString(DCM_PatientName, opt_patientName.c_str());
  if (result.good()) result = dataset->putAndInsertString(DCM_PatientID, opt_patientID.c_str());
  if (result.good()) result = dataset->putAndInsertString(DCM_PatientBirthDate, opt_patientBirthdate.c_str());
  if (result.good()) result = dataset->putAndInsertString(DCM_PatientSex, opt_patientSex.c_str());
  if (result.good()) result = dataset->putAndInsertString(DCM_BurnedInAnnotation, opt_annotation ? "YES" : "NO");

  if (strlen(opt_mediaTypes) >0) {
    if (result.good()) result = dataset->putAndInsertString(DCM_ListOfMIMETypes, opt_mediaTypes);
  }
  sprintf(buf, "%ld", OFstatic_cast(long, opt_instance));
  if (result.good()) result = dataset->putAndInsertString(DCM_InstanceNumber, buf);

  dcmGenerateUniqueIdentifier(buf, SITE_INSTANCE_UID_ROOT);
  if (result.good()) result = dataset->putAndInsertString(DCM_StudyInstanceUID, opt_studyUID.c_str());
  if (result.good()) result = dataset->putAndInsertString(DCM_SeriesInstanceUID, opt_seriesUID.c_str());
  if (result.good()) result = dataset->putAndInsertString(DCM_SOPInstanceUID, buf);

  // set instance creation date and time
  OFString s;
  if (result.good()) result = DcmDate::getCurrentDate(s);
  if (result.good()) result = dataset->putAndInsertOFStringArray(DCM_InstanceCreationDate, s);
  if (result.good()) result = DcmTime::getCurrentTime(s);
  if (result.good()) result = dataset->putAndInsertOFStringArray(DCM_InstanceCreationTime, s);
  return result;
}

OFCondition DcmEncapsulatedDocument::applyOverrideKeys(DcmDataset *outputDset)
{
  /* replace specific keys by those in overrideKeys, copied from findscu */
  OFListConstIterator(OFString) path = opt_overrideKeys.begin();
  OFListConstIterator(OFString) endOfList = opt_overrideKeys.end();
  OFCondition cond;
  DcmPathProcessor proc;
  while (path != endOfList)
  {
    cond = proc.applyPathWithValue(outputDset, *path);
    if (cond.bad()) {
      OFString err;
      err += "Bad override key/path: "; err += *path; err += ": "; err += cond.text();
      return makeOFCondition(OFM_dcmdata, 18, OF_error, err.c_str());
    }
    path++;
  }
  return cond;
}

void DcmEncapsulatedDocument::setOverrideKeys(const OFList<OFString>& ovkeys)
{
  OFListConstIterator(OFString) it = ovkeys.begin();
  OFListConstIterator(OFString) end = ovkeys.end();
  while (it != end)
  {
    opt_overrideKeys.push_back(*it);
    it++;
  }
}

OFCondition DcmEncapsulatedDocument::saveFile(DcmFileFormat fileformat)
{
  return fileformat.saveFile(opt_ofname, opt_oxfer, opt_oenctype, opt_oglenc, opt_opadenc,
    OFstatic_cast(Uint32, opt_filepad), OFstatic_cast(Uint32, opt_itempad));
}

OFString DcmEncapsulatedDocument::getInputFileName()
{
  return opt_ifname;
}

void DcmEncapsulatedDocument::setInputFileName(OFString fName)
{
  opt_ifname= fName;
}

OFString DcmEncapsulatedDocument::getOutputFileName()
{
  return opt_ofname;
}

void DcmEncapsulatedDocument::setOutputFileName(OFString fName)
{
  opt_ofname = fName;
}

E_TransferSyntax DcmEncapsulatedDocument::getTransferSyntax()
{
  return opt_oxfer;
}

DcmEncapsulatedDocument::~DcmEncapsulatedDocument()
{
}