/*
-----------------------------------------------------------------------------
Copyright (c) 2008-2010 Diversia

This file is part of Diversia.

Diversia is free software: you can redistribute it and/or modify it under the 
terms of the GNU General Public License as published by the Free Software 
Foundation, either version 3 of the License, or (at your option) any later 
version.

Diversia is distributed in the hope that it will be useful, but WITHOUT ANY 
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with 
Diversia. If not, see <http://www.gnu.org/licenses/>.

You may contact the author of Diversia by e-mail at: equabyte@sonologic.nl
-----------------------------------------------------------------------------
*/

#ifndef DIVERSIA_OGRECLIENT_GENERICRESOURCEMANAGER_H
#define DIVERSIA_OGRECLIENT_GENERICRESOURCEMANAGER_H

#include "OgreClient/Platform/Prerequisites.h"

namespace Diversia
{
namespace OgreClient
{
//------------------------------------------------------------------------------

class DIVERSIA_OGRECLIENT_API GenericResourceManager : public Ogre::ResourceManager, 
    public Ogre::Singleton<GenericResourceManager>
{
public:
    /**
    Destructor. 
    **/
    ~GenericResourceManager();
    /**
    Gets the singleton reference.
    **/
    static GenericResourceManager& getSingleton();
    /**
    Gets the singleton pointer. 
    **/
    static GenericResourceManager* getSingletonPtr();
    
protected:
    Ogre::Resource* createImpl( const Ogre::String& rName, Ogre::ResourceHandle handle,
        const Ogre::String& rGroup, bool isManual, Ogre::ManualResourceLoader* pLoader,
        const Ogre::NameValuePairList* pCreateParams );

private:
    /**
    Disallow default construction.
    **/
    GenericResourceManager();

};


//------------------------------------------------------------------------------
} // Namespace OgreClient
} // Namespace Diversia

#endif // DIVERSIA_OGRECLIENT_GENERICRESOURCEMANAGER_H