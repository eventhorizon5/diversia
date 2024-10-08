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

#include "OgreClient/Platform/StableHeaders.h"

#include "Client/Plugin/ClientPluginManager.h"
#include "Client/Communication/ServerAbstract.h"
#include "OgreClient/Audio/AudioManager.h"
#include "OgreClient/Graphics/GraphicsManager.h"
#include "OgreClient/Resource/ResourceManager.h"
#include "OgreClient/Resource/URLArchive.h"
#include "Shared/Communication/ServerInfo.h"

namespace Diversia
{
namespace OgreClient 
{
//------------------------------------------------------------------------------

boost::filesystem::path naive_uncomplete(boost::filesystem::path const path, 
    boost::filesystem::path const base) {
    if (path.has_root_path()){
        if (path.root_path() != base.root_path()) {
            return path;
        } else {
            return naive_uncomplete(path.relative_path(), base.relative_path());
        }
    } else {
        if (base.has_root_path()) {
            throw "cannot uncomplete a path relative path from a rooted base";
        } else {
            typedef boost::filesystem::path::const_iterator path_iterator;
            path_iterator path_it = path.begin();
            path_iterator base_it = base.begin();
            while ( path_it != path.end() && base_it != base.end() ) {
                if (*path_it != *base_it) break;
                ++path_it; ++base_it;
            }
            boost::filesystem::path result;
            for (; base_it != base.end(); ++base_it) {
                result /= "..";
            }
            for (; path_it != path.end(); ++path_it) {
                result /= *path_it;
            }
            return result;
        }
    }
}

ResourceManager::ResourcesLoader::ResourcesLoader()
{

}

ResourceManager::ResourcesLoader::ResourcesLoader( const SimpleResourceCallback& rCompletedSlot ):
    mCompletedSlot( rCompletedSlot )
{

}

ResourceManager::ResourcesLoader::~ResourcesLoader()
{

}

void ResourceManager::ResourcesLoader::addResource( Ogre::ResourcePtr pResource )
{
    mResources.insert( std::make_pair( pResource->getCreator()->getResourceType(), 
        pResource->getHandle() ) );
}

void ResourceManager::ResourcesLoader::loadingComplete( Ogre::ResourcePtr pResource )
{
    mResources.erase( std::make_pair( pResource->getCreator()->getResourceType(), 
        pResource->getHandle() ) );
    if( !mResources.size() )
    {
        mCompletedSlot();
        delete this;
    }
}

ResourceManager::ResourcesLoader* ResourceManager::ResourcesLoader::create( 
    const sigc::slot<void>& rCompletedSlot )
{
    return new ResourceManager::ResourcesLoader( rCompletedSlot );
}

ResourceManager::ResourceManager( Mode mode, PluginState state, ClientPluginManager& rPluginManager, 
    RakNet::RakPeerInterface& rRakPeer, RakNet::ReplicaManager3& rReplicaManager, 
    RakNet::NetworkIDManager& rNetworkIDManager ):
    ClientPlugin( mode, state, rPluginManager, rRakPeer, rReplicaManager, rNetworkIDManager ),
    mServer( rPluginManager.getServer() ),
    mRGM( Ogre::ResourceGroupManager::getSingleton() ),
    mRBQ( Ogre::ResourceBackgroundQueue::getSingleton() ),
    mBackgroundInitialise( false ),
    mBackgroundLoad( true ),
    mType( RESOURCELOCATIONTYPE_FILESYSTEM ),
    mCreated( false ),
    mInitializing( false )
{
    PropertySynchronization::storeUserObject();
}

ResourceManager::~ResourceManager()
{
    ResourceManager::destroy();
}

void ResourceManager::setResourceLocation( const String& rResourceLocation )
{
    if( mResourceLocation != rResourceLocation && !rResourceLocation.empty() )
    {
        mResourceLocation = rResourceLocation;
        if( mCreated ) 
        {
            ResourceManager::destroy();
            ResourceManager::load();
        }
    }
}

void ResourceManager::setGroup( const String& rGroup )
{
    if( mGroup.empty() ) mGroup = rGroup;
}

Ogre::ResourcePtr ResourceManager::getResource( const ResourceInfo& rResource, 
    bool create /*= false*/ ) const
{
    Ogre::ResourcePtr resource = mRGM._getResourceManager( 
        ResourceManager::getResourceManagerName( rResource.mType ) )->getByName( 
        rResource.mFile.string(), mGroup );
    if( !create ) return resource;

    if( resource.isNull() )
    {
        if( !mRGM.findResourceNames( mGroup, rResource.mFile.string() )->size() )
            return resource;

        resource = mRGM._getResourceManager( ResourceManager::getResourceManagerName( 
            rResource.mType ) )->create( rResource.mFile.string(), mGroup );
    }

    return resource;
}

ResourceList ResourceManager::list()
{
    ResourceList resourceList;

    Ogre::StringVectorPtr resources = mRGM.findResourceNames( mGroup, "*" );
    
    for( Ogre::StringVector::const_iterator i = resources->begin(); i != resources->end(); ++i )
    {
        resourceList.push_back( Path( *i ) );
    }

    return resourceList;
}

Ogre::ResourcePtr ResourceManager::loadResource( const ResourceInfo& rResource, 
    const ResourceCallback& rCallbackSlot )
{
    Ogre::ResourcePtr resource = ResourceManager::getResource( rResource, true );

    if( !resource.isNull() && resource->isLoaded() )
    {
        CLOGD << "Resource " << rResource << " already loaded.";

        // If the resource is already loaded, callback immediately.
        rCallbackSlot( resource );
    }
    else if( !resource.isNull() && mHandleTickets.count( std::make_pair( 
        resource->getCreator()->getResourceType(), resource->getHandle() ) ) )
    {
        // If the resource is loading, add a callback.
        HandleTickets::iterator i = mHandleTickets.find(  std::make_pair( 
            resource->getCreator()->getResourceType(), resource->getHandle() ) );

        if( i != mHandleTickets.end() )
        {
            mTicketCallbacks.insert( TicketCallbacks::value_type( i->second, rCallbackSlot ) );

            CLOGD << "Resource " << rResource << " is loading, ticket callback: " << i->second;
        }
        else
        {
            DIVERSIA_EXCEPT( Exception::ERR_INTERNAL_ERROR, 
                "Could not find ticket for resource that is loading in the background.", 
	            "ResourceManager::loadResource" );
        }
    }
    else if( !resource.isNull() )
    {
        // Load the resource in the background and register the callback.
        resource = ResourceManager::getResource( rResource, true );
        Ogre::BackgroundProcessTicket ticket = 
            mRBQ.load( ResourceManager::getResourceManagerName( rResource.mType ), 
            rResource.mFile.string(), mGroup, false, 0, 0, this );
        mTicketCallbacks.insert( TicketCallbacks::value_type( ticket, rCallbackSlot ) );
        mHandleTickets.insert( std::make_pair( std::make_pair( 
            resource->getCreator()->getResourceType(), resource->getHandle() ), ticket ) );
        mTicketResources.insert( std::make_pair( ticket, resource ) );

        CLOGD << "Resource " << rResource << " will be loaded, ticket: " << ticket;
    }
    else
    {
        DIVERSIA_EXCEPT( Exception::ERR_FILE_NOT_FOUND, "Cannot load resource " + 
            rResource.mFile.string() + ", file does not exist.", 
	        "ResourceManager::loadResource" );
    }

    return resource;
}

void ResourceManager::loadResources( const ResourceList& rResources, 
    const SimpleResourceCallback& rCallbackSlot )
{
    // Convert to set to remove duplicates.
    ResourceSet resources = ResourceInfo::toResourceSet( rResources );

    if( resources.empty() )
    {
        // If there are no resources to be loaded, call the slot and return immediately.
        rCallbackSlot();
        return;
    }

    ResourceManager::ResourcesLoader* resourceLoader = 
        ResourceManager::ResourcesLoader::create( rCallbackSlot );

    unsigned short loaded = 0;
    for( ResourceSet::const_iterator i = resources.begin(); i != resources.end(); ++i )
    {
        // Add resources to resource loader.
        Ogre::ResourcePtr resource = ResourceManager::getResource( *i, true );

        if( resource.isNull() )
        {
            DIVERSIA_EXCEPT( Exception::ERR_FILE_NOT_FOUND, "Cannot load resource " + 
                (*i).mFile.string() + ", file does not exist.", 
                "ResourceManager::loadResource" );
        }

        if( resource->isLoaded() )
            loaded++;
        else
            resourceLoader->addResource( resource );
    }

    // All resources are already loaded.
    if( loaded == resources.size() )
    {
        rCallbackSlot();
        delete resourceLoader;
        return;
    }

    for( ResourceSet::const_iterator j = resources.begin(); j != resources.end(); ++j )
    {
        // Load all resources and callback to the resource loader. If all resources are loaded
        // the rCallbackSlot will be called and the resource loader will be destroyed.
        ResourceManager::loadResource( *j, sigc::mem_fun( resourceLoader, 
            &ResourceManager::ResourcesLoader::loadingComplete ) );
    }
}

sigc::connection ResourceManager::connectInitialized( sigc::slot<void, ResourceManager&> rSlot )
{
    if( !mInitializing && mCreated ) rSlot( *this );
    return mInitializedSignal.connect( rSlot );
}

const char* ResourceManager::getResourceManagerName( ResourceType type )
{
    switch( type )
    {
        case RESOURCETYPE_OGREMESH: return "Mesh";
        case RESOURCETYPE_TEXTURE: return "Texture";
        case RESOURCETYPE_SKELETON: return "Skeleton";
        case RESOURCETYPE_MATERIALSCRIPT: return "Material";
        case RESOURCETYPE_PARTICLESCRIPT: return "Particle";
    }

    return "Generic";
}

void ResourceManager::load()
{
    if( mCreated ) return;

    if( !mResourceLocation.empty() )
    {
        if( mGroup.empty() )
            switch( mType )
            {
                case RESOURCELOCATIONTYPE_FILESYSTEM:
                    // What was this for? 
                    // PropertySynchronization::set( "Group", Path( mResourceLocation ).string() );
                    mGroup = Path( mResourceLocation ).leaf();
                    break;
                case RESOURCELOCATIONTYPE_URL:
                    mGroup = mServer.getServerInfo().getAddressMergedSafe();
                    break;
            }

        // Destroy old resource group.
        ResourceManager::destroy();

        // Create resource group
        mRGM.createResourceGroup( mGroup, false );
        GlobalsBase::mAudio->addResourceGroupLoader( mGroup );

        // Add resource location to group.
        switch( mType )
        {
            case RESOURCELOCATIONTYPE_FILESYSTEM:
            {
                mResourcePath = mResourceLocation;
#if DIVERSIA_PLATFORM == DIVERSIA_PLATFORM_WIN32
                if( !mResourcePath.has_root_name() && !mResourcePath.has_root_directory() )
#else
                if( !mResourcePath.has_root_directory() )
#endif 
                {
                    mResourcePath = GlobalsBase::mGraphics->getRootResourceLocation() / 
                    mResourcePath;
                }

                mResourcePath = naive_uncomplete( mResourcePath, boost::filesystem::current_path() );
                mResourceLocation = mResourcePath.string();

                mRGM.addResourceLocation( mResourceLocation, "FileSystem", mGroup, true );
                break;
            }
            case RESOURCELOCATIONTYPE_URL:
            {
                mRGM.addResourceLocation( mResourceLocation, "URL", mGroup, true );
                URLArchive::setCacheDirectory( mResourceLocation, mGroup );

                // Add cache directory to resource locations.
                Path cachePath = URLArchive::getCachePath() / mGroup;
                mRGM.addResourceLocation( cachePath.directory_string(), "FileSystem", mGroup, true );
                break;
            }
        }

        // Initialize resource group.
        if( mBackgroundInitialise )
        {
            mInitializationTicket = mRBQ.initialiseResourceGroup( mGroup, this );
            mInitializing = true;
        }
        else
        {
            mRGM.initialiseResourceGroup( mGroup );
            mInitializing = false;
            mInitializedSignal( *this );
            ClientPlugin::mLoadingCompletedSignal( *this );
        }

        mCreated = true;
    }
}

void ResourceManager::unload()
{
    mRGM.unloadUnreferencedResourcesInGroup( mGroup );
}

void ResourceManager::destroy()
{
    if( mCreated )
    {
        if( mInitializing && mBackgroundInitialise )
        {
            mRBQ.abortRequest( mInitializationTicket );
        }

        // Remove RTShader techniques
        Ogre::RTShader::ShaderGenerator::getSingletonPtr()->removeAllShaderBasedTechniques();

        mRGM.clearResourceGroup( mGroup );
        mRGM.destroyResourceGroup( mGroup );

        // Particle templates are not destroyed when the resources are destroyed.
        Ogre::ParticleSystemManager::getSingletonPtr()->removeTemplatesByResourceGroup( mGroup );

        // Remote audio resource loader.
        GlobalsBase::mAudio->removeResourceGroupLoader( mGroup );

        mCreated = false;
    }
}

void ResourceManager::operationCompleted( Ogre::BackgroundProcessTicket ticket, 
    const Ogre::BackgroundProcessResult& rResult )
{
    if( rResult.error )
    {
        DIVERSIA_EXCEPT( Exception::ERR_INTERNAL_ERROR, "Resource operation failed.", 
        	"ResourceManager::operationCompleted" );

        CLOGD << "Resource ticket " << ticket << " failed to load: " << rResult.message;
    }
    else
    {
        if( ticket == mInitializationTicket )
        {
            mInitializing = false;
            mInitializedSignal( *this );
            ClientPlugin::mLoadingCompletedSignal( *this );
        }
        else
        {
            // Send a callback to all listeners.
            std::pair<TicketCallbacks::iterator, TicketCallbacks::iterator> callbacks = 
                mTicketCallbacks.get<left>().equal_range( ticket );
            TicketResources::iterator resource = mTicketResources.find( ticket );
            if( resource != mTicketResources.end() )
            {
                for( TicketCallbacks::iterator i = callbacks.first; i != callbacks.second; ++i )
                {
                    i->second( resource->second );
                }

                // Remove ticket from containers.
                mTicketCallbacks.get<left>().erase( ticket );
                mHandleTickets.erase( std::make_pair( 
                    resource->second->getCreator()->getResourceType(), 
                    resource->second->getHandle() ) );
                mTicketResources.erase( ticket );

                CLOGD << "Resource loaded with ticket: " << ticket;
            }
            else
            {
                DIVERSIA_EXCEPT( Exception::ERR_INTERNAL_ERROR, "Resource operation failed.", 
                    "ResourceManager::operationCompleted" );
            }
        }
    }
}

//------------------------------------------------------------------------------
} // Namespace OgreClient
} // Namespace Diversia