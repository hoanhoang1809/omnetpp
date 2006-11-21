//==========================================================================
//  CENUM.H - part of
//
//                     OMNeT++/OMNEST
//            Discrete System Simulation in C++
//
//
//  Declaration of the following classes:
//    cEnum : effective integer-to-string mapping
//
//==========================================================================

/*--------------------------------------------------------------*
  Copyright (C) 1992-2005 Andras Varga

  This file is distributed WITHOUT ANY WARRANTY. See the file
  `license' for details on this and other legal matters.
*--------------------------------------------------------------*/

#ifndef __CENUM_H
#define __CENUM_H

#include "cownedobject.h"


/**
 * Provides string representation for enums. The class basically implements
 * effective integer-to-string mapping. Primary usage is to support displaying
 * symbolic names for integer values that represent some code (such as an enum
 * or #define). To be used mostly from Tkenv and possibly other user interfaces.
 *
 * @ingroup Internals
 * @see sEnumBuilder
 */
//FIXME use std::map<int,std::string> and std::map<std::string,int>
class SIM_API cEnum : public cOwnedObject
{
  private:
     struct sEnum {
          int key;
          char *string;
     };
     sEnum *vect;      // vector of objects
     int size;         // size of vector; always prime
     int items;        // number if items in the vector

  public:
    /** @name Constructors, destructor, assignment. */
    //@{

    /**
     * Copy constructor.
     */
    cEnum(const cEnum& cenum);

    /**
     * Constructor.
     */
    cEnum(const char *name=NULL, int siz=17);

    /**
     * Destructor.
     */
    virtual ~cEnum();

    /**
     * Assignment operator. The name member doesn't get copied;
     * see cOwnedObject's operator=() for more details.
     */
    cEnum& operator=(const cEnum& list);
    //@}

    /** @name Redefined cOwnedObject member functions. */
    //@{

    /**
     * Creates and returns an exact copy of this object.
     * See cOwnedObject for more details.
     */
    virtual cEnum *dup() const  {return new cEnum(*this);}

    /**
     * Produces a one-line description of object contents.
     * See cOwnedObject for more details.
     */
    virtual std::string info() const;
    //@}

    /** @name Insertion and lookup. */
    //@{

    /**
     * Add an item to the enum. If that numeric code exist, overwrite it.
     */
    void insert(int key, const char *str);

    /**
     * Look up key and return string representation. Return
     * NULL if not found.
     */
    const char *stringFor(int key);

    /**
     * Look up string and return numeric code. If not found, return
     * second argument (or -1).
     */
    int lookup(const char *str, int fallback=-1);
    //@}

    /**
     * Finds a registered enum by name.
     */
    static cEnum *find(const char *name);
};

#endif

