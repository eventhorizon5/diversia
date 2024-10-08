/*
 * =====================================================================================
 *
 *       Filename:  BtOgrePG.h
 *
 *    Description:  The part of BtOgre that handles information transfer from Bullet to 
 *                  Ogre (like updating graphics object positions).
 *
 *        Version:  1.0
 *        Created:  27/12/2008 03:40:56 AM
 *
 *         Author:  Nikhilesh (nikki)
 *
 * =====================================================================================
 */

#ifndef _BtOgreGP_H_
#define _BtOgreGP_H_

#include "OgreClient/Platform/Prerequisites.h"

#include "BtOgreExtras.h"

namespace BtOgre {

//A MotionState is Bullet's way of informing you about updates to an object.
//Pass this MotionState to a btRigidBody to have your SceneNode updated automaticaly.
class RigidBodyState : public btMotionState 
{
    protected:
        btTransform mTransform;
        btTransform mCenterOfMassOffset;

        Ogre::SceneNode *mNode;

    public:
        RigidBodyState(Ogre::SceneNode *node, const btTransform &transform, const btTransform &offset = btTransform::getIdentity())
            : mNode(node),
              mTransform(transform),
              mCenterOfMassOffset(offset)
        {
        }

        RigidBodyState(Ogre::SceneNode *node)
            : mNode(node),
              mTransform(((node != NULL) ? BtOgre::Convert::toBullet(node->getOrientation()) : btQuaternion(0,0,0,1)), 
                         ((node != NULL) ? BtOgre::Convert::toBullet(node->getPosition())    : btVector3(0,0,0))),
              mCenterOfMassOffset(btTransform::getIdentity())
        {
        }

        virtual void getWorldTransform(btTransform &ret) const 
        {
            ret = mCenterOfMassOffset.inverse() * mTransform;
        }

        virtual void setWorldTransform(const btTransform &in) 
        {
            if (mNode == NULL)
                return;

            mTransform = in;
            btTransform transform = in * mCenterOfMassOffset;

            btQuaternion rot = transform.getRotation();
            btVector3 pos = transform.getOrigin();
            mNode->setOrientation(rot.w(), rot.x(), rot.y(), rot.z());
            mNode->setPosition(pos.x(), pos.y(), pos.z());
        }

        void setNode(Ogre::SceneNode *node) 
        {
            mNode = node;
        }
};

//Softbody-Ogre connection goes here!

}

#endif
