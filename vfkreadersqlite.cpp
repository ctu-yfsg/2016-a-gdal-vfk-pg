/******************************************************************************
 * $Id: vfkreadersqlite.cpp 33713 2016-03-12 17:41:57Z goatbar $
 *
 * Project:  VFK Reader (SQLITE)
 * Purpose:  Implements VFKReaderSQLite class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Martin Landa <landa.martin gmail.com>
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_vsi.h"

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

#include <cstring>

#define SUPPORT_GEOMETRY

#ifdef SUPPORT_GEOMETRY
#include "ogr_geometry.h"
#endif

/*!
  \brief VFKReaderSQLite constructor
*/
VFKReaderSQLite::VFKReaderSQLite(const char *pszFileName) : VFKReaderDB(pszFileName)
{
    size_t          nLen;
    VSIStatBufL     sStatBufDb;
    GDALOpenInfo   *poOpenInfo;

    poOpenInfo = new GDALOpenInfo(pszFileName, GA_ReadOnly);
    m_bDbSource = poOpenInfo->nHeaderBytes >= 16 &&
      STARTS_WITH((const char*)poOpenInfo->pabyHeader, "SQLite format 3");
    delete poOpenInfo;

    if (!m_bDbSource) {
        m_bNewDb = TRUE;

        /* open tmp SQLite DB (re-use DB file if already exists) */
        pszDbNameConf = CPLGetConfigOption("OGR_VFK_DB_NAME", NULL);
        if (pszDbNameConf) {
            osDbName = pszDbNameConf;
        }
        else {
            osDbName = CPLResetExtension(m_pszFilename, "db");
        }
        nLen = osDbName.length();
        if( nLen > 2048 )
        {
            nLen = 2048;
            osDbName.resize(nLen);
        }
    }
    else {
        m_bNewDb = FALSE;

        nLen = strlen(pszFileName);
        osDbName = pszFileName;
    }

    m_pszDBname = new char [nLen+1];
    std::strncpy(m_pszDBname, osDbName.c_str(), nLen);
    m_pszDBname[nLen] = 0;

    CPLDebug("OGR-VFK", "Using internal DB: %s",
             m_pszDBname);

    if (!m_bDbSource && VSIStatL(osDbName, &sStatBufDb) == 0) {
        /* Internal DB exists */
        if (CPLTestBool(CPLGetConfigOption("OGR_VFK_DB_OVERWRITE", "NO"))) {
            m_bNewDb = TRUE;     /* overwrite existing DB */
            CPLDebug("OGR-VFK", "Internal DB (%s) already exists and will be overwritten",
                     m_pszDBname);
            VSIUnlink(osDbName);
        }
        else {
            if (pszDbNameConf == NULL &&
                m_poFStat->st_mtime > sStatBufDb.st_mtime) {
                CPLDebug("OGR-VFK",
                         "Found %s but ignoring because it appears\n"
                         "be older than the associated VFK file.",
                         osDbName.c_str());
                m_bNewDb = TRUE;
                VSIUnlink(osDbName);
            }
            else {
                m_bNewDb = FALSE;    /* re-use existing DB */
            }
        }
    }

    CPLDebug("OGR-VFK", "New DB: %s Spatial: %s",
             m_bNewDb ? "yes" : "no", m_bSpatial ? "yes" : "no");

    char* pszErrMsg;
    if (SQLITE_OK != sqlite3_open(osDbName, &m_poDB)) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Creating SQLite DB failed: %s",
                 sqlite3_errmsg(m_poDB));
    }

    char** papszResult;
    int nRowCount, nColCount;
    if (m_bDbSource) {
        /* check if it's really VFK DB datasource */
        pszErrMsg = NULL;
        papszResult = NULL;
        nRowCount = nColCount = 0;

        osCommand.Printf("SELECT * FROM sqlite_master WHERE type='table' AND name='%s'",
                         VFK_DB_TABLE);
        sqlite3_get_table(m_poDB,
                          osCommand.c_str(),
                          &papszResult,
                          &nRowCount, &nColCount, &pszErrMsg);
        sqlite3_free_table(papszResult);
        sqlite3_free(pszErrMsg);

        if (nRowCount != 1) {
            /* DB is not valid VFK datasource */
            sqlite3_close(m_poDB);
            m_poDB = NULL;
            return;
        }
    }

    if (!m_bNewDb) {
        /* check if DB is up-to-date datasource */
        pszErrMsg = NULL;
        papszResult = NULL;
        nRowCount = nColCount = 0;
        osCommand.Printf("SELECT * FROM %s LIMIT 1", VFK_DB_TABLE);
        sqlite3_get_table(m_poDB,
                          osCommand.c_str(),
                          &papszResult,
                          &nRowCount, &nColCount, &pszErrMsg);
        sqlite3_free_table(papszResult);
        sqlite3_free(pszErrMsg);

        if (nColCount != 7) {
            /* it seems that DB is outdated, let's create new DB from
             * scratch */
            if (m_bDbSource) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid VFK DB datasource");
            }

            if (SQLITE_OK != sqlite3_close(m_poDB)) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Closing SQLite DB failed: %s",
                         sqlite3_errmsg(m_poDB));
            }
            VSIUnlink(osDbName);
            if (SQLITE_OK != sqlite3_open(osDbName, &m_poDB)) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Creating SQLite DB failed: %s",
                         sqlite3_errmsg(m_poDB));
            }
            CPLDebug("OGR-VFK", "Internal DB (%s) is invalid - will be re-created",
                     m_pszDBname);

            m_bNewDb = TRUE;
        }
    }

    pszErrMsg = NULL;
    CPL_IGNORE_RET_VAL(sqlite3_exec(m_poDB, "PRAGMA synchronous = OFF",
                                    NULL, NULL, &pszErrMsg));
    sqlite3_free(pszErrMsg);

    if (m_bNewDb) {
        /* new DB, create support metadata tables */
        osCommand.Printf("CREATE TABLE %s (file_name text, file_size integer, table_name text, num_records integer, "
                         "num_features integer, num_geometries integer, table_defn text)",
                         VFK_DB_TABLE);
        ExecuteSQL(osCommand.c_str());

        /* header table */
        osCommand.Printf("CREATE TABLE %s (key text, value text)", VFK_DB_HEADER);
        ExecuteSQL(osCommand.c_str());
    }

}

VFKReaderSQLite::~VFKReaderSQLite()
{
    /* close tmp SQLite DB */
    if (SQLITE_OK != sqlite3_close(m_poDB)) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Closing SQLite DB failed: %s",
                 sqlite3_errmsg(m_poDB));
    }
    CPLDebug("OGR-VFK", "Internal DB (%s) closed",
             m_pszDBname);

    /* delete tmp SQLite DB if requested */
    if (CPLTestBool(CPLGetConfigOption("OGR_VFK_DB_DELETE", "NO"))) {
        CPLDebug("OGR-VFK", "Internal DB (%s) deleted",
                 m_pszDBname);
        VSIUnlink(m_pszDBname);
    }
}
