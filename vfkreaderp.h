/******************************************************************************
 * $Id: vfkreaderp.h 33713 2016-03-12 17:41:57Z goatbar $
 *
 * Project:  VFK Reader
 * Purpose:  Private Declarations for OGR free VFK Reader code.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, 2012-2013, Martin Landa <landa.martin gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#ifndef GDAL_OGR_VFK_VFKREADERP_H_INCLUDED
#define GDAL_OGR_VFK_VFKREADERP_H_INCLUDED

#include <map>
#include <string>
#include <vector>

#include "vfkreader.h"
#include "ogr_api.h"

class VFKReader;

/************************************************************************/
/*                              VFKReader                               */
/************************************************************************/
class VFKReader : public IVFKReader
{
private:
    bool           m_bLatin2;

    FILE          *m_poFD;
    char          *ReadLine(bool = FALSE);

    void          AddInfo(const char *);

protected:
    char           *m_pszFilename;
    VSIStatBuf     *m_poFStat;
    bool            m_bAmendment;
    int             m_nDataBlockCount;
    IVFKDataBlock **m_papoDataBlock;

    IVFKDataBlock  *CreateDataBlock(const char *);
    void            AddDataBlock(IVFKDataBlock *, const char *);
    OGRErr          AddFeature(IVFKDataBlock *, VFKFeature *);

    /* metadata */
    std::map<CPLString, CPLString> poInfo;

public:
    VFKReader(const char *);
    virtual ~VFKReader();

    bool           IsLatin2() const { return m_bLatin2; }
    bool           IsSpatial() const { return FALSE; }
    bool           IsPreProcessed() const { return FALSE; }
    bool           IsValid() const { return TRUE; }
    int            ReadDataBlocks();
    int            ReadDataRecords(IVFKDataBlock * = NULL);
    int            LoadGeometry();

    int            GetDataBlockCount() const { return m_nDataBlockCount; }
    IVFKDataBlock *GetDataBlock(int) const;
    IVFKDataBlock *GetDataBlock(const char *) const;

    const char    *GetInfo(const char *);
};

/************************************************************************/
/*                              VFKReaderDB                             */
/************************************************************************/

class VFKReaderDB : public VFKReader
{
private:

protected:
    char          *m_pszDBname;
    bool           m_bSpatial;
    bool           m_bNewDb;
    bool           m_bDbSource;

    CPLString      osCommand;
    CPLString      osDbName;
    const char    *pszDbNameConf;

    IVFKDataBlock *CreateDataBlock(const char *);
    void           AddDataBlock(IVFKDataBlock *, const char *);
    OGRErr         AddFeature(IVFKDataBlock *, VFKFeature *);

    void           StoreInfo2DB();

    void           CreateIndex(const char *, const char *, const char *, bool = TRUE);

    friend class   VFKFeatureDB;
public:
    VFKReaderDB(const char *);
    virtual ~VFKReaderDB();

    bool          IsSpatial() const { return m_bSpatial; }
    bool          IsPreProcessed() const { return !m_bNewDb; }
    bool          IsValid() const { return TRUE; }
    int           ReadDataBlocks();
    int           ReadDataRecords(IVFKDataBlock * = NULL);

    virtual void    PrepareStatement(const char *) = 0;
    virtual OGRErr  ExecuteSQL(const char *, bool = FALSE) = 0;
    virtual OGRErr  ExecuteSQL(const char *, int&) = 0;
    virtual OGRErr  ExecuteSQL(std::vector<int>&) = 0;
};

#endif // GDAL_OGR_VFK_VFKREADERP_H_INCLUDED
