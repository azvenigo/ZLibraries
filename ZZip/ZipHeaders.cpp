// MIT License
// Copyright 2019 Alex Zvenigorodsky
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "ZipHeaders.h"
#include <time.h>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include "helpers/FNMatch.h"
#include "helpers/StringHelpers.h"
#include "helpers/LoggingHelpers.h"

using namespace std;
using namespace ZFile;


string cExtensibleFieldEntry::HeaderToString()
{
    switch (mnHeader)
    {
    case 0x0001:    return "0x01-Zip64 extended information extra field";
    case 0x0007:    return "0x07-AV Info";
    case 0x0008:    return "0x08-Reserved for extended language encoding data(PFS)";
    case 0x0009:    return "0x09-OS/2";
    case 0x000a:    return "0x0a-NTFS";
    case 0x000c:    return "0x0c-OpenVMS";
    case 0x000d:    return "0x0d-UNIX";
    case 0x000e:    return "0x0e-Reserved for file stream and fork descriptors";
    case 0x000f:    return "0x0f-Patch Descriptor";
    case 0x0014:    return "0x14-PKCS#7 Store for X.509 Certificates";
    case 0x0015:    return "0x15-X.509 Certificate ID and Signature for individual file";
    case 0x0016:    return "0x16-X.509 Certificate ID for Central Directory";
    case 0x0017:    return "0x17-Strong Encryption Header";
    case 0x0018:    return "0x18-Record Management Controls";
    case 0x0019:    return "0x19-PKCS#7 Encryption Recipient Certificate List";
    case 0x0065:    return "0x65-IBM S / 390 (Z390), AS / 400 (I400)attributes-uncompressed";
    case 0x0066:    return "0x66-Reserved for IBM S / 390 (Z390), AS / 400 (I400)attributes - compressed";
    case 0x4690:    return "0x4690-POSZIP 4690 (reserved)";
    }

    return "";
}

cExtensibleFieldEntry::cExtensibleFieldEntry(const cExtensibleFieldEntry& from)
{
    mnHeader = from.mnHeader;
    mnSize = from.mnSize;
    if (mnSize > 0)
    {
        mpData = from.mpData;
    }
}


string cExtensibleFieldEntry::ToString()
{
    return "Header:" + HeaderToString() + " Size:" + to_string(mnSize) + " Data: [" + SH::FromBin(mpData.get(), mnSize) + "]";
}

bool ParseExtendedFields(uint8_t* pSearch, int32_t nSize, tExtensibleFieldList& list)
{
    uint8_t* pEnd = pSearch + nSize;
    while (pSearch < pEnd)
    {
        cExtensibleFieldEntry entry;

        entry.mnHeader = *((uint16_t*)(pSearch));
        entry.mnSize = *((uint16_t*)(pSearch + 2));

        if (entry.mnSize > 0)
        {
            entry.mpData.reset(new uint8_t[entry.mnSize]);
            memcpy(entry.mpData.get(), pSearch + 4, entry.mnSize);
        }

        list.push_back(entry);
        pSearch += sizeof(uint16_t) + sizeof(uint16_t) + entry.mnSize;
    }

    return true;
}

bool cLocalFileHeader::ParseRaw(uint8_t* pBuffer, uint32_t& nNumBytesProcessed)
{
    mLocalFileTag = *((uint32_t*)pBuffer);
    if (mLocalFileTag != kZipLocalFileHeaderTag)
    {
        zout << "Couldn't read LocalFileHeader signature.\n";
        return false;
    }

    mMinVersionToExtract = *((uint16_t*)(pBuffer + 4));
    mGeneralPurposeBitFlag = *((uint16_t*)(pBuffer + 6));
    mCompressionMethod = *((uint16_t*)(pBuffer + 8));
    mLastModificationTime = *((uint16_t*)(pBuffer + 10));
    mLastModificationDate = *((uint16_t*)(pBuffer + 12));
    mCRC32 = *((uint32_t*)(pBuffer + 14));
    mCompressedSize = *((uint32_t*)(pBuffer + 18));
    mUncompressedSize = *((uint32_t*)(pBuffer + 22));
    mFilenameLength = *((uint16_t*)(pBuffer + 26));
    mExtraFieldLength = *((uint16_t*)(pBuffer + 28));
    mFilename.assign((char*)(pBuffer + kStaticDataSize), mFilenameLength);

    uint8_t* pSearch = pBuffer + kStaticDataSize + mFilenameLength;
    ParseExtendedFields(pSearch, mExtraFieldLength, mExtensibleFieldList);

    uint8_t* pExtraFieldEnd = pSearch + mExtraFieldLength;
    while (pSearch < pExtraFieldEnd)
    {
        uint16_t nTag = *((uint16_t*)pSearch);
        if (nTag == kZipExtraFieldZip64ExtendedInfoTag)
        {
            pSearch += sizeof(uint16_t);        // skip the 16 bit tag
                                                //            uint16_t nSizeOfExtendedField = *((uint16_t*)pSearch);
            pSearch += sizeof(uint16_t);        // skip the size of this extended field

            if (mUncompressedSize == 0xffffffff)
            {
                mUncompressedSize = *((uint64_t*)(pSearch));
                pSearch += sizeof(uint64_t);
            }

            if (mCompressedSize == 0xffffffff)
            {
                mCompressedSize = *((uint64_t*)(pSearch));
                pSearch += sizeof(uint64_t);
            }

            break;
        }

        uint16_t nFieldBlock = *((uint16_t*)(pSearch + sizeof(uint16_t)));
        pSearch += (nFieldBlock + sizeof(uint32_t));        // past the tag
    }

    nNumBytesProcessed = kStaticDataSize + mFilenameLength + mExtraFieldLength;
    return true;
}

string cLocalFileHeader::FieldNames(eToStringFormat format)
{
    return FormatStrings(format, "MinVersionToExtract", "GeneralPurposeBitFlag", "CompressionMethod", "LastModificationTime", "LastModificationDate", "CRC32", "CompressedSize", "UncompressedSize", "FilenameLength", "ExtraFieldLength", "Filename", "ExtraField");
}

string cLocalFileHeader::ToString(eToStringFormat format)
{
    string sExtendedFields;
    for (tExtensibleFieldList::iterator it = mExtensibleFieldList.begin(); it != mExtensibleFieldList.end(); it++)
        sExtendedFields += (*it).ToString() + " ";

    return FormatStrings(format,
        to_string(mMinVersionToExtract),
        to_string(mGeneralPurposeBitFlag),
        to_string(mCompressionMethod),
        to_string(mLastModificationTime),
        to_string(mLastModificationDate),
        SH::ToHexString(mCRC32),
        to_string(mCompressedSize),
        to_string(mUncompressedSize),
        to_string(mFilenameLength),
        to_string(mExtraFieldLength),
        mFilename,
        sExtendedFields);
}

bool cLocalFileHeader::Read(tZFilePtr file, uint64_t nOffsetToLocalFileHeader, uint32_t& nNumBytesProcessed)
{
    // First we calculate how much data we need to read the LocalFileHeader
    // The LocalFileHeader is a static size of 30 bytes + FileNameLength + ExtraFieldLength which are stored at offset 26 and 28 respectively.
    const uint64_t kOffsetToFilenameLength = 26;

    uint16_t nFilenameLength;
    uint16_t nExtraFieldLength;

    int64_t nNumRead;

    file->Read((int64_t)nOffsetToLocalFileHeader + kOffsetToFilenameLength, sizeof(uint16_t), (uint8_t*)&nFilenameLength, nNumRead);
    file->Read((int64_t)nOffsetToLocalFileHeader + kOffsetToFilenameLength + sizeof(uint16_t), sizeof(uint16_t), (uint8_t*)&nExtraFieldLength, nNumRead);

    uint32_t nLocalFileHeaderRawSize = kStaticDataSize + nFilenameLength + nExtraFieldLength;

    uint8_t* pBuffer = new uint8_t[nLocalFileHeaderRawSize];

    if (!file->Read(nOffsetToLocalFileHeader, nLocalFileHeaderRawSize, pBuffer, nNumRead))
    {
        delete[] pBuffer;
        zout << "Couldn't read LocalFileHeader\n";
        return false;
    }

    ParseRaw(pBuffer, nNumBytesProcessed);
    delete[] pBuffer;
    return true;
}



bool cLocalFileHeader::Write(tZFilePtr file, uint64_t nOffsetToLocalFileHeader)
{
    bool bSuccess = true;
    int64_t nWritten = 0;
    bSuccess &= file->Write(nOffsetToLocalFileHeader, sizeof(uint32_t), (uint8_t*)&mLocalFileTag, nWritten);
    bSuccess &= file->Write((uint8_t*)&mMinVersionToExtract, sizeof(uint16_t)) == sizeof(uint16_t);
    bSuccess &= file->Write((uint8_t*)&mGeneralPurposeBitFlag, sizeof(uint16_t)) == sizeof(uint16_t);
    bSuccess &= file->Write((uint8_t*)&mCompressionMethod, sizeof(uint16_t)) == sizeof(uint16_t);
    bSuccess &= file->Write((uint8_t*)&mLastModificationTime, sizeof(uint16_t)) == sizeof(uint16_t);
    bSuccess &= file->Write((uint8_t*)&mLastModificationDate, sizeof(uint16_t)) == sizeof(uint16_t);
    bSuccess &= file->Write((uint8_t*)&mCRC32, sizeof(uint32_t)) == sizeof(uint32_t);

    uint16_t nExtraFieldLengthToWrite = kExtendedFieldLength;
    uint16_t nExtendedFieldLengthToWrite = kExtendedFieldLength - sizeof(uint16_t) - sizeof(uint16_t);

    uint32_t nNegOne = (uint32_t)-1;
    bSuccess &= file->Write((uint8_t*)&nNegOne, sizeof(uint32_t)) == sizeof(uint32_t);                              // compressed size
    bSuccess &= file->Write((uint8_t*)&nNegOne, sizeof(uint32_t)) == sizeof(uint32_t);                              // uncompressed size
    bSuccess &= file->Write((uint8_t*)&mFilenameLength, sizeof(uint16_t)) == sizeof(uint16_t);
    bSuccess &= file->Write((uint8_t*)&nExtraFieldLengthToWrite, sizeof(uint16_t)) == sizeof(uint16_t);

    // write the filename
    bSuccess &= file->Write((uint8_t*)mFilename.c_str(), mFilenameLength) == mFilenameLength;

    // now write the extra field
    bSuccess &= file->Write((uint8_t*)&kZipExtraFieldZip64ExtendedInfoTag, sizeof(uint16_t)) == sizeof(uint16_t);
    bSuccess &= file->Write((uint8_t*)&nExtendedFieldLengthToWrite, sizeof(uint16_t)) == sizeof(uint16_t);         // extra field just includes this extended field minus tag and size of data
    bSuccess &= file->Write((uint8_t*)&mUncompressedSize, sizeof(uint64_t)) == sizeof(uint64_t);
    bSuccess &= file->Write((uint8_t*)&mCompressedSize, sizeof(uint64_t)) == sizeof(uint64_t);

    if (!bSuccess)
    {
        zout << "cLocalFileHeader::Write - Failure to write LocalFileHeader!\n";
        return false;
    }

    return true;
}

uint64_t cLocalFileHeader::Size()
{
    return kStaticDataSize + mFilenameLength + kExtendedFieldLength;
}

bool cEndOfCDRecord::ParseRaw(uint8_t* pBuffer, uint32_t& nNumBytesProcessed)
{
    mEndOfCDRecTag = *((uint32_t*)pBuffer);
    if (mEndOfCDRecTag != kZipEndofCDTag)
    {
        zout << "Couldn't parse EndOfCDRecord\n";
        return false;
    }

    mDiskNum = *((uint16_t*)(pBuffer + 4));
    mDiskNumOfCD = *((uint16_t*)(pBuffer + 6));
    mNumCDRecordsThisDisk = *((uint16_t*)(pBuffer + 8));
    mNumTotalRecords = *((uint16_t*)(pBuffer + 10));
    mNumBytesOfCD = *((uint32_t*)(pBuffer + 12));
    mCDStartOffset = *((uint32_t*)(pBuffer + 16));
    mNumBytesOfComment = *((uint16_t*)(pBuffer + 20));
    mComment.assign((char*)(pBuffer + 22), mNumBytesOfComment);

    nNumBytesProcessed = 22 + mNumBytesOfComment;   // end of comment
    return true;
}

bool cEndOfCDRecord::Write(tZFilePtr file)
{
    int64_t nWritten = 0;
    file->SeekWrite(file->GetFileSize());
    file->Write((uint8_t*)&mEndOfCDRecTag, sizeof(uint32_t));
    file->Write((uint8_t*)&mDiskNum, sizeof(uint16_t));
    file->Write((uint8_t*)&mDiskNumOfCD, sizeof(uint16_t));
    file->Write((uint8_t*)&mNumCDRecordsThisDisk, sizeof(uint16_t));
    file->Write((uint8_t*)&mNumTotalRecords, sizeof(uint16_t));
    file->Write((uint8_t*)&mNumBytesOfCD, sizeof(uint32_t));
    file->Write((uint8_t*)&mCDStartOffset, sizeof(uint32_t));
    file->Write((uint8_t*)&mNumBytesOfComment, sizeof(uint16_t));
    file->Write((uint8_t*)mComment.c_str(), (uint32_t)mComment.length());

    return true;
}


string cEndOfCDRecord::FieldNames(eToStringFormat format)
{
    return FormatStrings(format, "DiskNum", "DiskNumOfCD", "NumCDRecordsThisDisk", "NumTotalRecords", "NumBytesOfCD", "CDStartOffset", "NumBytesOfComment", "Comment");
}

string cEndOfCDRecord::ToString(eToStringFormat format)
{
    return FormatStrings(format,
        to_string(mDiskNum),
        to_string(mDiskNumOfCD),
        to_string(mNumCDRecordsThisDisk),
        to_string(mNumTotalRecords),
        to_string(mNumBytesOfCD),
        to_string(mCDStartOffset),
        to_string(mNumBytesOfComment),
        mComment);
}



cZip64EndOfCDRecord::~cZip64EndOfCDRecord()
{
    delete[] mpZip64ExtensibleDataSector;       // delete any allocated buffer if it exists.  Safe to do if null.
}

bool cZip64EndOfCDRecord::ParseRaw(uint8_t* pBuffer, uint32_t& nNumBytesProcessed)
{
    mZip64EndOfCDRecTag = *((uint32_t*)pBuffer);

    if (mZip64EndOfCDRecTag != kZip64EndofCDTag)
    {
        zout << "Couldn't parse Zip64EndOfCDRecord\n";
        return false;
    }

    mSizeOfZiP64EndOfCDRecord = *((uint64_t*)(pBuffer + 4));
    mVersionMadeBy = *((uint16_t*)(pBuffer + 12));
    mMinVersionToExtract = *((uint16_t*)(pBuffer + 14));
    mDiskNum = *((uint32_t*)(pBuffer + 16));
    mDiskNumOfCD = *((uint32_t*)(pBuffer + 20));
    mNumCDRecordsThisDisk = *((uint64_t*)(pBuffer + 24));
    mNumTotalRecords = *((uint64_t*)(pBuffer + 32));
    mNumBytesOfCD = *((uint64_t*)(pBuffer + 40));
    mCDStartOffset = *((uint64_t*)(pBuffer + 48));

    mnDerivedSizeOfExtensibleDataSector = (uint32_t)(mSizeOfZiP64EndOfCDRecord + 12 - 56);
    if (mnDerivedSizeOfExtensibleDataSector > 0)
    {
        mpZip64ExtensibleDataSector = new uint8_t[mnDerivedSizeOfExtensibleDataSector];
        memcpy(mpZip64ExtensibleDataSector, pBuffer + 56, mnDerivedSizeOfExtensibleDataSector);
    }
    else
    {
        mpZip64ExtensibleDataSector = NULL;
    }

    nNumBytesProcessed = sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint32_t) + mnDerivedSizeOfExtensibleDataSector;

    return true;
}

bool cZip64EndOfCDRecord::Write(tZFilePtr file)
{
    int64_t nWritten = 0;
    file->SeekWrite(file->GetFileSize());
    file->Write((uint8_t*)&mZip64EndOfCDRecTag, sizeof(uint32_t));
    file->Write((uint8_t*)&mSizeOfZiP64EndOfCDRecord, sizeof(uint64_t));
    file->Write((uint8_t*)&mVersionMadeBy, sizeof(uint16_t));
    file->Write((uint8_t*)&mMinVersionToExtract, sizeof(uint16_t));
    file->Write((uint8_t*)&mDiskNum, sizeof(uint32_t));
    file->Write((uint8_t*)&mDiskNumOfCD, sizeof(uint32_t));
    file->Write((uint8_t*)&mNumCDRecordsThisDisk, sizeof(uint64_t));
    file->Write((uint8_t*)&mNumTotalRecords, sizeof(uint64_t));
    file->Write((uint8_t*)&mNumBytesOfCD, sizeof(uint64_t));
    file->Write((uint8_t*)&mCDStartOffset, sizeof(uint64_t));

    if (mpZip64ExtensibleDataSector)
    {
        file->Write((uint8_t*)mpZip64ExtensibleDataSector, mnDerivedSizeOfExtensibleDataSector);
    }

    return true;
}

string cZip64EndOfCDRecord::FieldNames(eToStringFormat format)
{
    return FormatStrings(format, "SizeOfZiP64EndOfCDRecord", "VersionMadeBy", "MinVersionToExtract", "DiskNum", "DiskNumOfCD", "NumCDRecordsThisDisk", "NumTotalRecords", "NumBytesOfCD", "CDStartOffset", "nDerivedSizeOfExtensibleDataSector");
}

string cZip64EndOfCDRecord::ToString(eToStringFormat format)
{
    return FormatStrings(format,
        to_string(mSizeOfZiP64EndOfCDRecord),
        to_string(mVersionMadeBy),
        to_string(mMinVersionToExtract),
        to_string(mDiskNum),
        to_string(mDiskNumOfCD),
        to_string(mNumCDRecordsThisDisk),
        to_string(mNumTotalRecords),
        to_string(mNumBytesOfCD),
        to_string(mCDStartOffset),
        to_string(mnDerivedSizeOfExtensibleDataSector));
}



bool cZip64EndOfCDLocator::ParseRaw(uint8_t* pBuffer, uint32_t& nNumBytesProcessed)
{
    mZip64EndOfCDLocatorTag = *((uint32_t*)pBuffer);
    if (mZip64EndOfCDLocatorTag != kZip64EndofCDLocatorTag)
    {
        zout << "Couldn't parse CD Header Tag\n";
        return false;
    }

    mDiskNumOfCD = *((uint32_t*)(pBuffer + 4));
    mZip64EndofCDOffset = *((uint64_t*)(pBuffer + 8));
    mNumTotalDisks = *((uint32_t*)(pBuffer + 16));

    nNumBytesProcessed = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t);

    return true;
}

bool cZip64EndOfCDLocator::Write(tZFilePtr file)
{
    file->Write((uint8_t*)&mZip64EndOfCDLocatorTag, sizeof(uint32_t));
    file->Write((uint8_t*)&mDiskNumOfCD, sizeof(uint32_t));
    file->Write((uint8_t*)&mZip64EndofCDOffset, sizeof(uint64_t));
    file->Write((uint8_t*)&mNumTotalDisks, sizeof(uint32_t));

    return true;
}

string cZip64EndOfCDLocator::FieldNames(eToStringFormat format)
{
    return FormatStrings(format, "DiskNumOfCD", "Zip64EndofCDOffset", "NumTotalDisks");
}

string cZip64EndOfCDLocator::ToString(eToStringFormat format)
{
    return FormatStrings(format, to_string(mDiskNumOfCD), to_string(mZip64EndofCDOffset), to_string(mNumTotalDisks));
}

bool cCDFileHeader::ParseRaw(uint8_t* pBuffer, uint32_t& nNumBytesProcessed)
{
    mCDTag = *((uint32_t*)pBuffer);
    if (mCDTag != kZipCDTag)
    {
        zout << "Couldn't parse CD Header Tag\n";
        return false;
    }

    mVersionMadeBy = *((uint16_t*)(pBuffer + 4));
    mMinVersionToExtract = *((uint16_t*)(pBuffer + 6));
    mGeneralPurposeBitFlag = *((uint16_t*)(pBuffer + 8));
    mCompressionMethod = *((uint16_t*)(pBuffer + 10));
    mLastModificationTime = *((uint16_t*)(pBuffer + 12));
    mLastModificationDate = *((uint16_t*)(pBuffer + 14));
    mCRC32 = *((uint32_t*)(pBuffer + 16));
    mCompressedSize = *((uint32_t*)(pBuffer + 20));
    mUncompressedSize = *((uint32_t*)(pBuffer + 24));
    mFilenameLength = *((uint16_t*)(pBuffer + 28));
    mExtraFieldLength = *((uint16_t*)(pBuffer + 30));
    mFileCommentLength = *((uint16_t*)(pBuffer + 32));
    mDiskNumFileStart = *((uint16_t*)(pBuffer + 34));
    mInternalFileAttributes = *((uint16_t*)(pBuffer + 36));
    mExternalFileAttributes = *((uint32_t*)(pBuffer + 38));
    mLocalFileHeaderOffset = *((uint32_t*)(pBuffer + 42));
    mFileName.assign((char*)pBuffer + kStaticDataSize, mFilenameLength);
    mFileComment.assign((char*)pBuffer + kStaticDataSize + mFilenameLength + mExtraFieldLength, mFileCommentLength);

    uint8_t* pSearch = pBuffer + kStaticDataSize + mFilenameLength;

    ParseExtendedFields(pSearch, mExtraFieldLength, mExtensibleFieldList);

    uint8_t* pExtraFieldEnd = pSearch + mExtraFieldLength;
    while (pSearch < pExtraFieldEnd)
    {
        uint16_t nTag = *((uint16_t*)pSearch);
        if (nTag == kZipExtraFieldZip64ExtendedInfoTag)
        {
            pSearch += sizeof(uint16_t);        // skip the 16 bit tag
                                                //            uint16_t nSizeOfExtendedField = *((uint16_t*)pSearch);
            pSearch += sizeof(uint16_t);        // skip the size of this extended field

            if (mUncompressedSize == 0xffffffff)
            {
                mUncompressedSize = *((uint64_t*)(pSearch));
                pSearch += sizeof(uint64_t);
            }

            if (mCompressedSize == 0xffffffff)
            {
                mCompressedSize = *((uint64_t*)(pSearch));
                pSearch += sizeof(uint64_t);
            }

            if (mLocalFileHeaderOffset == 0xffffffff)
            {
                mLocalFileHeaderOffset = *((uint64_t*)(pSearch));
                pSearch += sizeof(uint64_t);
            }

            break;
        }
        else if (nTag == kZipExtraFieldUnicodePathTag)
        {
            zout << "found unicode path tag";
            // TBD. Add unicode path parsing/handling
        }
        else if (nTag == kZipExtraFieldUnicodeCommentTag)
        {
            zout << "found unicode comment tag";
            // TBD. Add unicode comment parsing/handling
        }

        uint16_t nFieldBlock = *((uint16_t*)(pSearch + sizeof(uint16_t)));
        pSearch += (nFieldBlock + sizeof(uint32_t));        // past the tag
    }

    nNumBytesProcessed = 46 + mFilenameLength + mExtraFieldLength + mFileCommentLength;
    return true;
}

bool cCDFileHeader::Write(tZFilePtr file)
{
    int64_t nWritten = 0;
    file->SeekWrite(file->GetFileSize());
    file->Write((uint8_t*)&mCDTag, sizeof(uint32_t));
    file->Write((uint8_t*)&mVersionMadeBy, sizeof(uint16_t));
    file->Write((uint8_t*)&mMinVersionToExtract, sizeof(uint16_t));
    file->Write((uint8_t*)&mGeneralPurposeBitFlag, sizeof(uint16_t));
    file->Write((uint8_t*)&mCompressionMethod, sizeof(uint16_t));
    file->Write((uint8_t*)&mLastModificationTime, sizeof(uint16_t));
    file->Write((uint8_t*)&mLastModificationDate, sizeof(uint16_t));
    file->Write((uint8_t*)&mCRC32, sizeof(uint32_t));

    uint32_t nNegOne = (uint32_t)-1;
    file->Write((uint8_t*)&nNegOne, sizeof(uint32_t));                             // compressed size
    file->Write((uint8_t*)&nNegOne, sizeof(uint32_t));                             // uncompressed size
    file->Write((uint8_t*)&mFilenameLength, sizeof(uint16_t));
    file->Write((uint8_t*)&kExtraFieldLength, sizeof(uint16_t));
    file->Write((uint8_t*)&mFileCommentLength, sizeof(uint16_t));

    uint16_t n16NegOne = 0xffff;
    file->Write((uint8_t*)&n16NegOne, sizeof(uint16_t));
    file->Write((uint8_t*)&mInternalFileAttributes, sizeof(uint16_t));
    file->Write((uint8_t*)&mExternalFileAttributes, sizeof(uint32_t));
    file->Write((uint8_t*)&nNegOne, sizeof(uint32_t));                             // local file header offset
    file->Write((uint8_t*)mFileName.c_str(), mFilenameLength);

    // now write the extra field
    file->Write((uint8_t*)&kZipExtraFieldZip64ExtendedInfoTag, sizeof(uint16_t));
    file->Write((uint8_t*)&kExtendedFieldLength, sizeof(uint16_t));
    file->Write((uint8_t*)&mUncompressedSize, sizeof(uint64_t));
    file->Write((uint8_t*)&mCompressedSize, sizeof(uint64_t));
    file->Write((uint8_t*)&mLocalFileHeaderOffset, sizeof(uint64_t));
    uint32_t nDiskNum = mDiskNumFileStart;
    file->Write((uint8_t*)&nDiskNum, sizeof(uint32_t));

    file->Write((uint8_t*)mFileComment.c_str(), mFileCommentLength);

    return true;
}

uint64_t cCDFileHeader::Size()
{
    return kStaticDataSize + mFilenameLength + mFileCommentLength + kExtraFieldLength;
}


string cCDFileHeader::FieldNames(eToStringFormat format)
{
    return FormatStrings(format, "FileName", "CompressedSize", "UncompressedSize", "LocalFileHeaderOffset", "CRC32", "VersionMadeBy", "MinVersionToExtract", "GeneralPurposeBitFlag", "CompressionMethod", "LastModificationTime", "LastModificationDate", "FilenameLength", "ExtraFieldLength", "FileCommentLength", "DiskNumFileStart", "InternalFileAttributes", "ExternalFileAttributes", "ExtraField", "FileComment");
}

string cCDFileHeader::ToString(eToStringFormat format)
{
    string sExtendedFields;
    for (tExtensibleFieldList::iterator it = mExtensibleFieldList.begin(); it != mExtensibleFieldList.end(); it++)
        sExtendedFields += (*it).ToString() + " ";

    return FormatStrings(format, string("\"" + mFileName + "\""),
        to_string(mCompressedSize),
        to_string(mUncompressedSize),
        to_string(mLocalFileHeaderOffset),
        SH::ToHexString(mCRC32),
        to_string(mVersionMadeBy),
        to_string(mMinVersionToExtract),
        to_string(mGeneralPurposeBitFlag),
        to_string(mCompressionMethod),
        to_string(mLastModificationTime),
        to_string(mLastModificationDate),
        to_string(mFilenameLength),
        to_string(mExtraFieldLength),
        to_string(mFileCommentLength),
        to_string(mDiskNumFileStart),
        to_string(mInternalFileAttributes),
        to_string(mExternalFileAttributes),
        sExtendedFields,
        string("\"" + mFileComment + "\""));
}



cZipCD::cZipCD()
{
    mbInitted = false;
    mbIsZip64 = false;
}

cZipCD::~cZipCD()
{
}

bool cZipCD::Init(tZFilePtr file)
{
    // Find the end of CD Record
    const int32_t kMaxSizeOfCDRec = 1024;

    std::streampos nZipFileSize = file->GetFileSize();

    int32_t nReadSizeofCDRec = kMaxSizeOfCDRec;
    if (nReadSizeofCDRec > nZipFileSize)
        nReadSizeofCDRec = (int32_t)nZipFileSize;

    int64_t nSeekPosition = nZipFileSize - std::streampos(nReadSizeofCDRec);

    uint8_t* pBuf = new uint8_t[nReadSizeofCDRec];     // Should be more than enough space for this record

                                                       // fill the buffer with the end of the zip file
    int64_t nBytesRead = 0;
    if (!file->Read(nSeekPosition, nReadSizeofCDRec, pBuf, nBytesRead))
    {
        delete[] pBuf;
        zout << "Failed to read " << nReadSizeofCDRec << " bytes for End of CD Record.\n";
        return false;
    }

    bool bFoundEndOfCDRecord = false;
    for (int32_t nSeek = nReadSizeofCDRec - sizeof(kZipEndofCDTag); nSeek >= 0; nSeek--)
    {
        uint32_t* pTag = (uint32_t*)(pBuf + nSeek);
        if (*pTag == kZipEndofCDTag)
        {
            // Found a 32 bit tag
            uint32_t nNumBytesProcessed = 0;
            mEndOfCDRecord.ParseRaw(pBuf + nSeek, nNumBytesProcessed);
            bFoundEndOfCDRecord = true;
            break;
        }
    }

    if (!bFoundEndOfCDRecord)
    {
        delete[] pBuf;
        zout << "Couldn't find End of CD Tag\n";
        return false;
    }


    // Now look for ZiP64 End of CD Directory Locator
    bool bFoundZip64EndOfCDLocator = false;
    for (int32_t nSeek = nReadSizeofCDRec - sizeof(kZip64EndofCDLocatorTag); nSeek >= 0; nSeek--)
    {
        uint32_t* pTag = (uint32_t*)(pBuf + nSeek);
        if (*pTag == kZip64EndofCDLocatorTag)
        {
            // Found 
            //            zout << "Found kZip64EndofCDLocatorTag.\n";
            uint32_t nNumBytesProcessed = 0;
            mZip64EndOfCDLocator.ParseRaw(pBuf + nSeek, nNumBytesProcessed);
            bFoundZip64EndOfCDLocator = true;
            break;
        }
    }

    // Found a Zip64 locator, now read zip64 end of CD record
    if (bFoundZip64EndOfCDLocator)
    {
        for (int32_t nSeek = nReadSizeofCDRec - sizeof(kZip64EndofCDTag); nSeek >= 0; nSeek--)
        {
            uint32_t* pTag = (uint32_t*)(pBuf + nSeek);
            if (*pTag == kZip64EndofCDTag)
            {
                //                zout << "Found kZip64EndofCDTag.\n";
                uint32_t nNumBytesProcessed = 0;
                mZip64EndOfCDRecord.ParseRaw(pBuf + nSeek, nNumBytesProcessed);

                mbIsZip64 = true;       // Treat archive as zip64
                break;
            }
        }
    }

    delete[] pBuf;

    uint64_t nOffsetOfCD = mEndOfCDRecord.mCDStartOffset;
    uint64_t nCDBytes = mEndOfCDRecord.mNumBytesOfCD;       // central directory
    uint64_t nCDRecords = mEndOfCDRecord.mNumTotalRecords;
    if (mbIsZip64)
    {
        nOffsetOfCD = mZip64EndOfCDRecord.mCDStartOffset;
        nCDBytes = (uint32_t)mZip64EndOfCDRecord.mNumBytesOfCD;
        nCDRecords = mZip64EndOfCDRecord.mNumTotalRecords;
    }


    const uint32_t kMaxReasonableCDBytes = 64 * 1024 * 1024;    // 64 megs ought to be far far beyond anything we'd ever encounter
    if ((uint32_t)nCDBytes > kMaxReasonableCDBytes)
    {
        zout << "CD Size read as " << nCDBytes << " which exceeds the maximum accepted of " << kMaxReasonableCDBytes << ".  If this assumpiton was unreasonable then the author of this code humbly apologizes.\n";
        return false;
    }

    ///////////////////////

    pBuf = new uint8_t[(uint32_t)nCDBytes];
    // fill the buffer with the raw CD data
    if (!file->Read(nOffsetOfCD, (uint32_t)nCDBytes, pBuf, nBytesRead))
    {
        delete[] pBuf;
        zout << "Failed to read " << nCDBytes << " bytes for the CD.\n";
        return false;
    }



    int32_t nBufOffset = 0;
    for (size_t i = 0; i < nCDRecords; i++)
    {
        cCDFileHeader fileHeader;
        uint32_t nNumBytesProcessed = 0;

        fileHeader.ParseRaw(pBuf + nBufOffset, nNumBytesProcessed);
        //        zout << "read header for file \"" << fileHeader.mFileName << "\" at offset " << (uint32_t) (nBufOffset + nOffsetOfCD) << fileHeader.ToString() << "\n";
        nBufOffset += nNumBytesProcessed;

        mCDFileHeaderList.push_back(fileHeader);
    }

    delete[] pBuf;

    mbInitted = true;
    return true;
}


uint64_t cZipCD::GetNumTotalFiles()
{
    uint64_t nTotal = 0;
    for (tCDFileHeaderList::iterator it = mCDFileHeaderList.begin(); it != mCDFileHeaderList.end(); it++)
    {
        cCDFileHeader& file = *it;
        if (file.mUncompressedSize > 0)
            nTotal++;
    }

    return nTotal;
}

uint64_t cZipCD::GetNumTotalFolders()
{
    uint64_t nTotal = 0;
    for (tCDFileHeaderList::iterator it = mCDFileHeaderList.begin(); it != mCDFileHeaderList.end(); it++)
    {
        cCDFileHeader& file = *it;
        if (file.mUncompressedSize == 0 && file.mCompressedSize == 0)
            nTotal++;
    }

    return nTotal;
}

uint64_t cZipCD::GetTotalCompressedBytes()
{
    uint64_t nTotal = 0;
    for (tCDFileHeaderList::iterator it = mCDFileHeaderList.begin(); it != mCDFileHeaderList.end(); it++)
    {
        cCDFileHeader& file = *it;
        nTotal += file.mCompressedSize;
    }

    return nTotal;
}

uint64_t cZipCD::GetTotalUncompressedBytes()
{
    uint64_t nTotal = 0;
    for (tCDFileHeaderList::iterator it = mCDFileHeaderList.begin(); it != mCDFileHeaderList.end(); it++)
    {
        cCDFileHeader& file = *it;
        nTotal += file.mUncompressedSize;
    }

    return nTotal;
}



bool cZipCD::ComputeCDRecords(uint64_t nStartOfCDOffset)
{
    // Only supporting single "disk" (for now?)
    mEndOfCDRecord.mComment = "Test comment!";
    mEndOfCDRecord.mNumBytesOfComment = 13;
    mEndOfCDRecord.mNumBytesOfCD = (uint32_t)Size();
    mEndOfCDRecord.mNumTotalRecords = (uint16_t)mCDFileHeaderList.size();
    mEndOfCDRecord.mNumCDRecordsThisDisk = mEndOfCDRecord.mNumTotalRecords;
    mEndOfCDRecord.mCDStartOffset = 0xffffffff;

    mZip64EndOfCDRecord.mSizeOfZiP64EndOfCDRecord = mZip64EndOfCDRecord.Size();
    mZip64EndOfCDRecord.mNumCDRecordsThisDisk = mCDFileHeaderList.size();
    mZip64EndOfCDRecord.mNumTotalRecords = mCDFileHeaderList.size();
    mZip64EndOfCDRecord.mNumBytesOfCD = Size();
    mZip64EndOfCDRecord.mCDStartOffset = nStartOfCDOffset;

    return true;
}


uint64_t cZipCD::Size()
{
    uint64_t nSize = 0;
    for (tCDFileHeaderList::iterator it = mCDFileHeaderList.begin(); it != mCDFileHeaderList.end(); it++)
    {
        cCDFileHeader& cdFileHeader = *it;
        nSize += cdFileHeader.Size();
    }

    nSize += mEndOfCDRecord.Size();
    nSize += mZip64EndOfCDLocator.Size();
    nSize += mZip64EndOfCDRecord.Size();

    return nSize;
}

void cZipCD::DumpCD(std::ostream& out, const string& sPattern, bool bVerbose, eToStringFormat format)
{
    int32_t nIndex = 0;
    if (bVerbose)
    {
        out << NextLine(format);
        out << "Package Central Directory";
        out << NextLine(format);
    }

    out << NextLine(format);

    uint32_t nPatternMatchingFiles = 0;

    // CD Entries
    if (bVerbose)
    {
        out << NextLine(format);
        out << "List of Files";
    }

    if (format == kHTML)
        out << "<table border='1'>\n";
    if (bVerbose)
    {
        out << cCDFileHeader::FieldNames(format).c_str();
        out << NextLine(format);
    }

    for (tCDFileHeaderList::iterator it = mCDFileHeaderList.begin(); it != mCDFileHeaderList.end(); it++)
    {
        cCDFileHeader& cdFileHeader = *it;

        if (FNMatch(sPattern, cdFileHeader.mFileName.c_str()))
        {
            nPatternMatchingFiles++;
            if (bVerbose)
                out << cdFileHeader.ToString(format).c_str();
            else
                out << cdFileHeader.mFileName.c_str() << NextLine(format);
        }

        nIndex++;
    }

    if (format == kHTML)
        out << "</table>\n";

    out << NextLine(format);

    if (bVerbose)
    {
        // CD Stats
        if (format == kHTML)
            out << "<table border='1'>\n";

        out << FormatStrings(format, "Total Files", "Total Folders", "Total Compressed Size", "Total Uncompressed Size", "Compression Ratio");
        out << FormatStrings(format, to_string(GetNumTotalFiles()), to_string(GetNumTotalFolders()), to_string(GetTotalCompressedBytes()), to_string(GetTotalUncompressedBytes()), to_string((double)GetTotalCompressedBytes() / (double)GetTotalUncompressedBytes()));

        if (format == kHTML)
            out << "</table>\n";
    }

    // If a pattern was specified then provide stats
    if (!sPattern.empty())
    {
        out << "Found " << nPatternMatchingFiles << " out of " << GetNumTotalFiles() + GetNumTotalFolders() << " matching pattern: \"" << sPattern.c_str() << "\"\n";
    }


    out << NextLine(format);
}

bool cZipCD::GetFileHeader(const string& sFilename, cCDFileHeader& fileHeader)
{
    if (!mbInitted)
        return false;

    for (tCDFileHeaderList::iterator it = mCDFileHeaderList.begin(); it != mCDFileHeaderList.end(); it++)
    {
        cCDFileHeader& cdFileHeader = *it;
        if (cdFileHeader.mFileName == sFilename)
        {
            fileHeader = *it;
            return true;
        }
    }

    return false;
}

bool cZipCD::Write(tZFilePtr file)
{
    bool bSuccess = true;

    for (tCDFileHeaderList::iterator it = mCDFileHeaderList.begin(); it != mCDFileHeaderList.end(); it++)
    {
        cCDFileHeader& cdFileHeader = *it;
        //zout << "writing header for file \"" << cdFileHeader.mFileName << "\" at offset " << file.tellg() << cdFileHeader.ToString() << "\n";
        bSuccess &= cdFileHeader.Write(file);
    }

    // Write Zip64 End of CD Record
    // but first record the offset to it
    //mZip64EndOfCDLocator.mZip64EndofCDOffset = file.tellg();
    mZip64EndOfCDLocator.mZip64EndofCDOffset = file->GetFileSize();

    bSuccess &= mZip64EndOfCDRecord.Write(file);

    // Write Zip64 End of CD Locator
    bSuccess &= mZip64EndOfCDLocator.Write(file);

    // Write End of CD Record
    bSuccess &= mEndOfCDRecord.Write(file);

    return bSuccess;
}
