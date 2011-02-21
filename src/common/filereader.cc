//=========================================================================
//  FILEREADER.CC - part of
//                  OMNeT++/OMNEST
//           Discrete System Simulation in C++
//
//  Author: Levente Meszaros
//
//=========================================================================

/*--------------------------------------------------------------*
  Copyright (C) 2006-2008 OpenSim Ltd.

  This file is distributed WITHOUT ANY WARRANTY. See the file
  `license' for details on this and other legal matters.
*--------------------------------------------------------------*/


#include <sstream>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include "platmisc.h"
#include "commonutil.h"
#include "filereader.h"
#include "exception.h"

USING_NAMESPACE

#define LL  INT64_PRINTF_FORMAT

//#define TRACE_FILEREADER

FileChangedError::FileChangedError(FileReader::FileChangedState changed, const char *messagefmt, ...) : opp_runtime_error(""), change(change)
{
    char buf[1024];
    VSNPRINTF(buf, 1024, messagefmt);
    errormsg = buf;
}

FileReader::FileReader(const char *fileName, size_t bufferSize)
   : fileName(fileName), bufferSize(bufferSize),
     bufferBegin(new char[bufferSize]),
     bufferEnd(bufferBegin + bufferSize),
     maxLineSize(bufferSize / 2),
     lastSavedBufferBegin(new char[bufferSize]),
     lastSavedBufferEnd(lastSavedBufferBegin + bufferSize)
{
#ifdef TRACE_FILEREADER
	TRACE("FileReader::FileReader(%s, %d)", fileName, bufferSize);
#endif
	lastSavedSize = -1;
    file = NULL;
    fileSize = -1;
    bufferFileOffset = -1;
    enableCheckFileForChanges = true;
    enableIgnoreAppendChanges = true;
    numReadLines = 0;
    numReadBytes = 0;
    dataBegin = NULL;
    dataEnd = NULL;
    currentDataPointer = NULL;
    currentLineStartOffset = -1;
    currentLineEndOffset = -1;
}

FileReader::~FileReader()
{
#ifdef TRACE_FILEREADER
    TRACE("FileReader::~FileReader(%s)", fileName.c_str());
#endif
    delete [] bufferBegin;
    delete [] lastSavedBufferBegin;
    ensureFileClosed();
}

void FileReader::ensureFileOpenInternal()
{
    if (!file) {
        // Note: 'b' mode turns off CR/LF translation and might be faster
        file = fopen(fileName.c_str(), "rb");

        if (!file)
            throw opp_runtime_error("Cannot open file `%s'", fileName.c_str());

        if (bufferFileOffset == -1)
            seekTo(0);
    }
}

int FileReader::readFileEnd(void *dataPointer)
{
    opp_fseek(file, std::max((file_offset_t)0, (file_offset_t)(fileSize - bufferSize)), SEEK_SET);
    if (ferror(file))
        throw opp_runtime_error("Cannot seek in file `%s'", fileName.c_str());
    int bytesRead = fread(dataPointer, 1, std::min((int64)bufferSize, fileSize), file);
    if (ferror(file))
        throw opp_runtime_error("Read error in file `%s'", fileName.c_str());
    return bytesRead;
}

void FileReader::ensureFileOpen()
{
    if (!file) {
        ensureFileOpenInternal();
        fileSize = getFileSizeInternal();
        lastSavedSize = readFileEnd(lastSavedBufferBegin);
    }
}

void FileReader::ensureFileClosed()
{
    if (file) {
        fclose(file);
        file = NULL;
    }
}

file_offset_t FileReader::pointerToFileOffset(char *pointer) const
{
	Assert(bufferBegin <= pointer && pointer <= bufferEnd);
    file_offset_t fileOffset = pointer - bufferBegin + bufferFileOffset;
    Assert(fileOffset >= 0 && fileOffset <= fileSize);
    return fileOffset;
}

void FileReader::checkConsistency(bool checkDataPointer) const
{
    bool ok = (size_t)(bufferEnd - bufferBegin) == bufferSize &&
      ((!dataBegin && !dataEnd) ||
       (dataBegin <= dataEnd && bufferBegin <= dataBegin && dataEnd <= bufferEnd &&
        (!checkDataPointer || (dataBegin <= currentDataPointer && currentDataPointer <= dataEnd))));

    if (!ok)
        throw opp_runtime_error("FileReader: internal error");
}

FileReader::FileChangedState FileReader::checkFileForChanges()
{
    int64 newFileSize = getFileSizeInternal();
    if (newFileSize == fileSize)
        return UNCHANGED; // NOTE: assuming that the content is not overwritten... :(
    else {
#ifdef TRACE_FILEREADER
        int readBytes =
#endif
            readFileEnd((void *)bufferBegin);
        dataBegin = dataEnd = NULL;
        FileChangedState change;
        if (newFileSize > fileSize && !memcmp(bufferBegin, lastSavedBufferBegin, lastSavedSize))
            change = APPENDED;
        else
            change = OVERWRITTEN;
#ifdef TRACE_FILEREADER
        TRACE("OLDFILESIZE: %"LL"d, NEWFILESIZE: %"LL"d, OLDBUFFERSIZE: %d, NEWBUFFERSIZE: %d, CHANGE: %d\n", fileSize, newFileSize, lastSavedSize, readBytes, change);
#endif
        fileSize = newFileSize;
        lastSavedSize = readFileEnd(lastSavedBufferBegin);
        return change;
    }
}

void FileReader::signalFileChanges(FileChangedState change)
{
    switch (change) {
        case UNCHANGED:
            break;
        case OVERWRITTEN:
            throw FileChangedError(change, "File changed: `%s' has been overwritten", fileName.c_str());
        case APPENDED:
            if (!enableIgnoreAppendChanges)
                throw FileChangedError(change, "File changed: `%s' has been appended", fileName.c_str());
            else
                break;
    }
}

void FileReader::setCurrentDataPointer(char *pointer)
{
    currentDataPointer = pointer;

#ifndef NDEBUG
    checkConsistency();
#endif
}

void FileReader::fillBuffer(bool forward)
{
#ifndef NDEBUG
    checkConsistency();
#endif
#ifdef TRACE_FILEREADER
    TRACE("FileReader::fillBuffer %s", forward ? "forward" : "backward");
#endif

    char *dataPointer;
    int dataLength;

    if (!hasData()) {
        dataPointer = (char *)bufferBegin;
        dataLength = bufferSize;
    }
    else if (forward) {
        Assert(currentDataPointer);

        if (currentDataPointer < dataBegin) {
            dataPointer = currentDataPointer;
            dataLength = dataBegin - currentDataPointer;
        }
        else {
            dataPointer = dataEnd;
            dataLength = std::min((int64)(bufferEnd - dataEnd), getFileSize() - pointerToFileOffset(dataEnd));
        }
    }
    else {
        Assert(currentDataPointer);

        if (currentDataPointer > dataEnd) {
            dataPointer = dataEnd;
            dataLength = currentDataPointer - dataEnd;
        }
        else {
            dataPointer = (char *)bufferBegin;
            dataLength = dataBegin - bufferBegin;
        }
    }

    if (dataLength > 0) {
        // check for changes before reading the file
        if (enableCheckFileForChanges) {
            FileChangedState change = checkFileForChanges();
            if (change != UNCHANGED) {
                signalFileChanges(change);
                dataPointer = (char *)bufferBegin;
                dataLength = bufferSize;
            }
        }

        file_offset_t fileOffset = pointerToFileOffset(dataPointer);
        opp_fseek(file, fileOffset, SEEK_SET);
        if (ferror(file))
            throw opp_runtime_error("Cannot seek in file `%s'", fileName.c_str());
        dataLength = std::min((int64)dataLength, fileSize - fileOffset);
        int bytesRead = fread(dataPointer, 1, dataLength, file);
        if (ferror(file))
            throw opp_runtime_error("Read error in file `%s'", fileName.c_str());

#ifdef TRACE_FILEREADER
        TPRINTF("FileReader::fillBuffer data at file offset: %"LL"d, length: %d", fileOffset, bytesRead);
#endif

        if (!hasData()) {
            dataBegin = dataPointer;
            dataEnd = dataPointer + bytesRead;
        }
        else if (forward) {
            if (currentDataPointer < dataBegin)
                dataBegin = currentDataPointer;
            else
                dataEnd = dataPointer + bytesRead;
        }
        else {
            if (currentDataPointer > dataEnd)
                dataEnd = currentDataPointer;
            else
                dataBegin = (char *)bufferBegin;
        }

        numReadBytes += bytesRead;
    }

#ifndef NDEBUG
    checkConsistency(true);
#endif
}

bool FileReader::isLineStart(char *s) {
    Assert(bufferBegin <= s && s <= bufferEnd);
    if (s == bufferBegin) {
        // first line of file
        if (bufferFileOffset == 0)
            return true;
        else { // slow path
           file_offset_t fileOffset = pointerToFileOffset(s) - 1;
           opp_fseek(file, fileOffset, SEEK_SET);
           if (ferror(file))
               throw opp_runtime_error("Cannot seek in file `%s'", fileName.c_str());
           char previousChar;
           fread(&previousChar, 1, 1, file);  //XXX warning: ignored return value
           if (ferror(file))
               throw opp_runtime_error("Read error in file `%s'", fileName.c_str());
           return previousChar == '\n';
        }
    }
    else if (s - 1 < dataBegin)
        fillBuffer(false);
    else if (s - 1 >= dataEnd)
        fillBuffer(true);

    return *(s - 1) == '\n';
}

char *FileReader::findNextLineStart(char *start, bool bufferFilled)
{
#ifdef TRACE_FILEREADER
	TRACE("FileReader::findNextLineStart(start: %p)", start);
#endif

	char *s = start;

    // find next CR/LF (fast path)
    while (s < dataEnd && *s != '\r' && *s!= '\n')
        s++;

    if (s < dataEnd && *s == '\r')
        s++;

    if (s < dataEnd && *s == '\n')
        s++;

    Assert(s <= dataEnd);
    if (s == dataEnd) // did we reach the end of the data in the buffer? (slow path)
    {
        file_offset_t fileOffset = pointerToFileOffset(start);

#ifdef TRACE_FILEREADER
        TPRINTF("FileReader::findNextLineStart start file offset %"LL"d", fileOffset);
#endif

        if (s != start && isLineStart(s)) // line just ends at the end of data buffer
            ; // void
        else if (fileOffset == getFileSize()) // searching from the end of the file
            return NULL;
        else if (!bufferFilled) { // refill buffer
            seekTo(fileOffset, maxLineSize);

#ifdef TRACE_FILEREADER
            TPRINTF("FileReader::findNextLineStart bufffer file offset before fillBuffer is called %"LL"d", bufferFileOffset);
#endif

            fillBuffer(true);
            s = findNextLineStart(fileOffsetToPointer(fileOffset), true);
        }
        else if (getDataEndFileOffset() == getFileSize()) // searching reached to the end of the file without CR/LF
            return NULL;
        else // line too long
            throw opp_runtime_error("Line too long, should be below %d in file `%s'", maxLineSize, fileName.c_str());
    }

#ifdef TRACE_FILEREADER
    TPRINTF("FileReader::findNextLineStart returns with next line file offset %"LL"d", pointerToFileOffset(s));
#endif

    return s;
}

char *FileReader::findPreviousLineStart(char *start, bool bufferFilled)
{
#ifdef TRACE_FILEREADER
	TRACE("FileReader::findPreviousLineStart(start: %p)", start);
#endif

    char *s = start - 1;

    if (s >= dataBegin && *s == '\n')
        s--;

    if (s >= dataBegin && *s == '\r')
        s--;

    // find previous CR/LF (fast path)
    while (s >= dataBegin && *s != '\r' && *s!= '\n')
        s--;

    s++;

    Assert(s >= dataBegin);
    if (s == dataBegin) // did we reach the beginning of the data in the buffer? (slow path)
    {
        file_offset_t fileOffset = pointerToFileOffset(start);

#ifdef TRACE_FILEREADER
        TPRINTF("FileReader::findPreviousLineStart start file offset %"LL"d", fileOffset);
#endif

        if (s != start && isLineStart(s)) // line starts at the beginning of the data buffer
            ; // void
        else if (fileOffset == 0) // searching from the beginning of the file
            return NULL;
        else if (!bufferFilled) { // refill buffer
            seekTo(fileOffset, maxLineSize);

#ifdef TRACE_FILEREADER
            TPRINTF("FileReader::findPreviousLineStart bufffer file offset before refill %"LL"d", bufferFileOffset);
#endif

            fillBuffer(false);
            s = findPreviousLineStart(fileOffsetToPointer(fileOffset), true);
        }
        else if (getDataBeginFileOffset() == 0) // searching reached to the beginning of the file without CR/LF
            return dataBegin;
        else // line too long
            throw opp_runtime_error("Line too long, should be below %d in file `%s'", maxLineSize, fileName.c_str());
    }

#ifdef TRACE_FILEREADER
    TPRINTF("FileReader::findPreviousLineStart: previous line file offset %"LL"d", pointerToFileOffset(s));
#endif

    return s;
}

char *FileReader::getNextLineBufferPointer()
{
    numReadLines++;
    ensureFileOpen();

    Assert(currentDataPointer);

#ifdef TRACE_FILEREADER
    TRACE("FileReader::Reading in next line at file offset: %"LL"d", pointerToFileOffset(currentDataPointer));
#endif

    // read forward if needed
    fillBuffer(true);

    // when starting in the middle of a line
    if (!isLineStart(currentDataPointer)) {
        char *nextLineDataPointer = findNextLineStart(currentDataPointer);

        if (nextLineDataPointer)
            setCurrentDataPointer(nextLineDataPointer);
        else {
            currentLineStartOffset = currentLineEndOffset = -1;

            return NULL;
        }
    }

    // save offset to avoid problems related to recursive calls from find
    file_offset_t savedCurrentLineStartOffset = pointerToFileOffset(currentDataPointer);
    char *nextLineDataPointer = findNextLineStart(currentDataPointer);

    if (nextLineDataPointer) {
        setCurrentDataPointer(nextLineDataPointer);
        currentLineStartOffset = savedCurrentLineStartOffset;
        currentLineEndOffset = pointerToFileOffset(currentDataPointer);

#ifdef TRACE_FILEREADER
        TPRINTF("FileReader::getNextLineBufferPointer: currentLineStartOffset: %"LL"d, currentLineEndOffset: %"LL"d", currentLineStartOffset, currentLineEndOffset);
#endif

        Assert(currentLineEndOffset >= currentLineStartOffset);
        return fileOffsetToPointer(currentLineStartOffset);
    }
    else {
        currentLineStartOffset = currentLineEndOffset = -1;
        return NULL;
    }
}

char *FileReader::getPreviousLineBufferPointer()
{
    numReadLines++;
    ensureFileOpen();

    Assert(currentDataPointer);

#ifdef TRACE_FILEREADER
    TRACE("FileReader::Reading in previous line at file offset: %"LL"d", pointerToFileOffset(currentDataPointer));
#endif

    // read backward if needed
    fillBuffer(false);

    // when starting in the middle of a line
    if (!isLineStart(currentDataPointer)) {
        char *previousLineDataPointer = findPreviousLineStart(currentDataPointer);

        if (previousLineDataPointer)
            setCurrentDataPointer(previousLineDataPointer);
        else {
            currentLineStartOffset = currentLineEndOffset = -1;

            return NULL;
        }
    }

    // save offset to avoid problems related to recursive calls from find
    file_offset_t savedCurrentLineEndOffset = pointerToFileOffset(currentDataPointer);
    char *previousLineDataPointer = findPreviousLineStart(currentDataPointer);

    if (previousLineDataPointer) {
        setCurrentDataPointer(previousLineDataPointer);
        currentLineStartOffset = pointerToFileOffset(currentDataPointer);
        currentLineEndOffset = savedCurrentLineEndOffset;

#ifdef TRACE_FILEREADER
        TPRINTF("FileReader::getPreviousLineBufferPointer: currentLineStartOffset: %"LL"d, currentLineEndOffset: %"LL"d", currentLineStartOffset, currentLineEndOffset);
#endif

        Assert(currentLineEndOffset >= currentLineStartOffset);
        return fileOffsetToPointer(currentLineStartOffset);
    }
    else {
        currentLineStartOffset = currentLineEndOffset = -1;
        return NULL;
    }
}

char *FileReader::getFirstLineBufferPointer()
{
    seekTo(0);
    return getNextLineBufferPointer();
}

char *FileReader::getLastLineBufferPointer()
{
    seekTo(getFileSize());
    return getPreviousLineBufferPointer();
}

const char *strnistr(const char *haystack, const char *needle, int n, bool caseSensitive)
{
    int needleLen = strlen(needle);
    if (n == 0)
        n = strlen(haystack);

    int slen = n - needleLen;

    for (const char *s = haystack; slen>0 && *s; s++, slen--)
        if (!(caseSensitive ? strncmp(s, needle, needleLen) : strncasecmp(s, needle, needleLen)))
            return s;
    return NULL;
}

char *FileReader::findNextLineBufferPointer(const char *search, bool caseSensitive)
{
    char *line;
    while ((line = getNextLineBufferPointer()) != NULL)
        if (strnistr(line, search, getCurrentLineLength(), caseSensitive))
            return line;
    return NULL;
}

char *FileReader::findPreviousLineBufferPointer(const char *search, bool caseSensitive)
{
    char *line;
    while ((line = getPreviousLineBufferPointer()) != NULL)
        if (strnistr(line, search, getCurrentLineLength(), caseSensitive))
            return line;
    return NULL;
}

int FileReader::getCurrentLineLength() const
{
    int result = currentLineEndOffset - currentLineStartOffset;
    Assert(result >= 0);
    return result;
}

int64 FileReader::getFileSize()
{
    if (fileSize == -1) {
        ensureFileOpen();
        fileSize = getFileSizeInternal();
    }
    return fileSize;
}

int64 FileReader::getFileSizeInternal()
{
    struct opp_stat_t s;
    opp_fstat(fileno(file), &s);
    return s.st_size;
}

void FileReader::seekTo(file_offset_t fileOffset, unsigned int ensureBufferSizeAround)
{
#ifdef TRACE_FILEREADER
    TRACE("FileReader::seekTo seeking to file offset: %"LL"d", fileOffset);
#endif
#ifndef NDEBUG
    checkConsistency();
#endif

    if (fileOffset < 0 || fileOffset > getFileSize())
        throw opp_runtime_error("Invalid file offset: %"LL"d", fileOffset);

    ensureFileOpen();

    // check if requested offset is already in memory
    if (bufferFileOffset != -1 &&
        bufferFileOffset + ensureBufferSizeAround <= fileOffset &&
        fileOffset <= (file_offset_t)(bufferFileOffset + bufferSize - ensureBufferSizeAround))
    {
        setCurrentDataPointer(fileOffsetToPointer(fileOffset));
        Assert(currentDataPointer);
        return;
    }

    file_offset_t newBufferFileOffset = std::min(std::max((int64)0L, getFileSize() - (int64)bufferSize), std::max((int64)0L, fileOffset - (int64)bufferSize / 2));
    setCurrentDataPointer((char *)bufferBegin + fileOffset - newBufferFileOffset);
    Assert(currentDataPointer);

#ifdef TRACE_FILEREADER
    TPRINTF("FileReader::seekTo setting buffer file offset to: %"LL"d", newBufferFileOffset);
#endif

    // try to keep as much data as possible
    if (hasData()) {
        file_offset_t oldDataBeginFileOffset = getDataBeginFileOffset();
        file_offset_t oldDataEndFileOffset = getDataEndFileOffset();

#ifdef TRACE_FILEREADER
        TPRINTF("FileReader::seekTo data before seek: from file offset: %"LL"d to file offset: %"LL"d", oldDataBeginFileOffset, oldDataEndFileOffset);
#endif

        file_offset_t newBufferBeginFileOffset = newBufferFileOffset;
        file_offset_t newBufferEndFileOffset = newBufferFileOffset + bufferSize;
        file_offset_t moveSrcBeginFileOffset = std::min(newBufferEndFileOffset, std::max(newBufferBeginFileOffset, oldDataBeginFileOffset));
        file_offset_t moveSrcEndFileOffset = std::min(newBufferEndFileOffset, std::max(newBufferBeginFileOffset, oldDataEndFileOffset));
        char *moveSrc = fileOffsetToPointer(moveSrcBeginFileOffset);
        char *moveDest = moveSrcBeginFileOffset - newBufferBeginFileOffset + (char *)bufferBegin;
        int moveSize = moveSrcEndFileOffset - moveSrcBeginFileOffset;

        if (moveSize > 0 && moveSrc != moveDest) {
#ifdef TRACE_FILEREADER
        	TPRINTF("FileReader::Keeping data from file offset: %"LL"d with length: %d", pointerToFileOffset(moveSrc), moveSize);
#endif

            fflush(stdout);

            memmove(moveDest, moveSrc, moveSize);
        }

        bufferFileOffset = newBufferFileOffset;
        dataBegin = moveDest;
        dataEnd = moveDest + moveSize;

#ifdef TRACE_FILEREADER
        TPRINTF("FileReader::Data after seek: from file offset: %"LL"d to file offset: %"LL"d", getDataBeginFileOffset(), getDataEndFileOffset());
#endif
    }
    else {
        bufferFileOffset = newBufferFileOffset;
        dataBegin = currentDataPointer;
        dataEnd = currentDataPointer;
    }

#ifndef NDEBUG
    checkConsistency();
#endif
}

/*
Example code:

#include <iostream>
using namespace std;

int main(int argc, char **argv)
{
    if (argc<2)
        return 1;

    try {
        FileReader freader(argv[1],200);
        char *line;
        while ((line = freader.getNextLineBufferPointer()) != NULL)
            cout << line << "\n";
    }
    catch (std::exception& e) {
        cout << "Error: " << e.what() << endl;
    }

    return 0;
}
*/
