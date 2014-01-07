/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2011      Zynga Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "CCScene.h"
#include "touch_dispatcher/CCTouchDispatcher.h"
#include "support/CCPointExtension.h"
#include "script_support/CCScriptSupport.h"
#include "CCDirector.h"

NS_CC_BEGIN

CCScene::CCScene()
: m_touchableNodes(NULL)
, m_touchTargets(NULL)
{
    m_touchableNodes = CCArray::createWithCapacity(100);
    m_touchableNodes->retain();
    m_touchTargets = CCArray::createWithCapacity(10);
    m_touchTargets->retain();
    m_bIgnoreAnchorPointForPosition = true;
    setAnchorPoint(ccp(0.5f, 0.5f));
}

CCScene::~CCScene()
{
    CC_SAFE_RELEASE(m_touchableNodes);
    CC_SAFE_RELEASE(m_touchTargets);
}

bool CCScene::init()
{
    bool bRet = false;
     do
     {
         CCDirector * pDirector;
         CC_BREAK_IF( ! (pDirector = CCDirector::sharedDirector()) );
         this->setContentSize(pDirector->getWinSize());
         m_eTouchMode = kCCTouchesAllAtOnce;
         // success
         bRet = true;
     } while (0);
     return bRet;
}

CCScene *CCScene::create()
{
    CCScene *pRet = new CCScene();
    if (pRet && pRet->init())
    {
        pRet->autorelease();
        return pRet;
    }
    else
    {
        CC_SAFE_DELETE(pRet);
        return NULL;
    }
}

void CCScene::registerWithTouchDispatcher()
{
    CCTouchDispatcher* pDispatcher = CCDirector::sharedDirector()->getTouchDispatcher();
    m_eTouchMode = kCCTouchesAllAtOnce;
    if(m_eTouchMode == kCCTouchesAllAtOnce)
    {
        pDispatcher->addStandardDelegate(this, 0);
    }
    else
    {
        pDispatcher->addTargetedDelegate(this, m_nTouchPriority, true);
    }
}

void CCScene::unregisterWithTouchDispatcher()
{
    CCDirector::sharedDirector()->getTouchDispatcher()->removeDelegate(this);
}

void CCScene::addTouchableNode(CCNode *node)
{
    if (!m_touchableNodes->containsObject(node))
    {
        m_touchableNodes->addObject(node);
        //CCLOG("CCSCENE: ADD TOUCHABLE NODE: %p", node);
        if (!isTouchEnabled())
        {
            setTouchEnabled(true);
        }
    }
}

void CCScene::removeTouchableNode(CCNode *node)
{
    m_touchableNodes->removeObject(node);
    //CCLOG("CCSCENE: REMOVE TOUCHABLE NODE: %p", node);
    if (m_touchableNodes->count() == 0 && isTouchEnabled())
    {
        setTouchEnabled(false);
    }
}

int CCScene::ccTouchBegan(CCTouch *pTouch, CCEvent *pEvent)
{
    // remove all touch targets
    m_touchTargets->removeAllObjects();

    // check touch targets
    const CCPoint p = pTouch->getLocation();
    CCObject *node;
    CCNode *touchNode = NULL;
    CCNode *checkVisibleNode = NULL;
    bool visible = true;
    sortAllTouchableNodes(m_touchableNodes);
    CCARRAY_FOREACH(m_touchableNodes, node)
    {
        checkVisibleNode = touchNode = dynamic_cast<CCNode*>(node);

        // check node is visible
        visible = true;
        do
        {
            visible = visible && checkVisibleNode->isVisible();
            checkVisibleNode = checkVisibleNode->getParent();
        } while (checkVisibleNode && visible);
        if (!visible) continue;

        const CCRect boundingBox = touchNode->getCascadeBoundingBox();
        if (touchNode->isRunning() && boundingBox.containsPoint(p))
        {
            touchNode->retain();
            int ret = touchNode->ccTouchBegan(pTouch, pEvent);
            if (ret == kCCTouchBegan || ret == kCCTouchBeganNoSwallows)
            {
                m_touchTargets->addObject(touchNode);
                if (ret == kCCTouchBegan)
                {
                    touchNode->release();
                    break;
                }
            }
            touchNode->release();
        }
    }

    sortAllTouchableNodes(m_touchTargets);
    return kCCTouchBegan;
}

int CCScene::ccTouchMoved(CCTouch *pTouch, CCEvent *pEvent)
{
    CCNode *touchNode = NULL;
    unsigned int count = m_touchTargets->count();
    for (unsigned int i = 0; i < count; ++i)
    {
        touchNode = dynamic_cast<CCNode*>(m_touchTargets->objectAtIndex(i));
        if (touchNode->isRunning())
        {
            int ret = touchNode->ccTouchMoved(pTouch, pEvent);
            if (ret == kCCTouchMovedSwallows) break;
            if (ret == kCCTouchMovedReleaseOthers)
            {
                for (int j = count - 1; j >= 0; --j)
                {
                    if (j != i)
                    {
                        touchNode = dynamic_cast<CCNode*>(m_touchTargets->objectAtIndex(j));
                        touchNode->ccTouchCancelled(pTouch, pEvent);
                        m_touchTargets->removeObjectAtIndex(j);
                    }
                }
                break;
            }
        }
        else
        {
            m_touchTargets->removeObjectAtIndex(i);
            count--;
            i--;
        }
    }
    return kCCTouchMoved;
}

void CCScene::ccTouchEnded(CCTouch *pTouch, CCEvent *pEvent)
{
    CCObject *node;
    CCNode *touchNode = NULL;
    CCARRAY_FOREACH(m_touchTargets, node)
    {
        touchNode = dynamic_cast<CCNode*>(node);
        if (touchNode->isRunning())
        {
            touchNode->ccTouchEnded(pTouch, pEvent);
        }
    }
    m_touchTargets->removeAllObjects();
}

void CCScene::ccTouchCancelled(CCTouch *pTouch, CCEvent *pEvent)
{
    CCObject *node;
    CCNode *touchNode = NULL;
    CCARRAY_FOREACH(m_touchTargets, node)
    {
        touchNode = dynamic_cast<CCNode*>(node);
        if (touchNode->isRunning())
        {
            touchNode->ccTouchCancelled(pTouch, pEvent);
        }
    }
    m_touchTargets->removeAllObjects();
}

// multi-touches

bool less_touch_id(const CCObject *a, const CCObject *b)
{
    return ((CCTouch*)a)->getID() < ((CCTouch*)b)->getID();
}

int CCScene::ccTouchesBegan(CCSet *pTouches, CCEvent *pEvent)
{
    sortAllTouchableNodes(m_touchableNodes);

    // sort touches by id
    vector<CCTouch*> touches;
    for (CCSetIterator it = pTouches->begin(); it != pTouches->end(); ++it)
    {
        touches.push_back((CCTouch*)*it);
    }
    sort(touches.begin(), touches.end(), less_touch_id);

    // make touch id -> CCTouch map
    TouchIdToTouchObjectMap touchIdToTouchObjectMap = makeTouchIdToTouchObjectMap(pTouches);

    // iteration all touches, check touch position in nodes
    m_nodeToTouchIdArrayMap.clear();
    m_touchTargets->removeAllObjects();
    CCArray *tmpTouchTargets = CCArray::createWithCapacity(10);
    for (vector<CCTouch*>::iterator it = touches.begin(); it != touches.end(); ++it)
    {
        // get touch position and id
        CCTouch *touch = *it;
        const CCPoint touchPosition = touch->getLocation();
        const int touchId = touch->getID();

        // check touch position in nodes
        CCObject *node;
        CCNode *touchNode = NULL;
        CCNode *checkVisibleNode = NULL;
        bool visible = true;
        CCARRAY_FOREACH(m_touchableNodes, node)
        {
            checkVisibleNode = touchNode = dynamic_cast<CCNode*>(node);

            // check node is visible
            visible = true;
            do
            {
                visible = visible && checkVisibleNode->isVisible();
                checkVisibleNode = checkVisibleNode->getParent();
            } while (checkVisibleNode && visible);
            if (!visible) continue;

            // check position in node
            const CCRect boundingBox = touchNode->getCascadeBoundingBox();
            if (!touchNode->isRunning() || !boundingBox.containsPoint(touchPosition)) continue;

            // check node touch mode
            bool nodeNotInMap = m_nodeToTouchIdArrayMap.find(touchNode) == m_nodeToTouchIdArrayMap.end();
            if (touchNode->getTouchMode() != kCCTouchesOneByOne || nodeNotInMap)
            {
                // add node to targets
                m_nodeToTouchIdArrayMap[touchNode].push_back(touchId);
                if (nodeNotInMap) tmpTouchTargets->addObject(touchNode);
            }
        }
    }

    CCLOG("-------- BEGAN");
    sortAllTouchableNodes(tmpTouchTargets);
    unsigned int count = tmpTouchTargets->count();
    map<int, bool> ignoredTouchId;
    for (unsigned int i = 0; i < count; ++i)
    {
        CCNode *touchNode = static_cast<CCNode*>(tmpTouchTargets->objectAtIndex(i));
        TouchIdArray &touchIdArray = m_nodeToTouchIdArrayMap[touchNode];
        bool removeTouchNode = false;
        if (touchNode->getTouchMode() == kCCTouchesOneByOne)
        {
            // one by one
//            CCLOG("---- node %p [ONE BY ONE]", touchNode);
            int touchId = *touchIdArray.begin();

            if (ignoredTouchId.find(touchId) == ignoredTouchId.end())
            {
                CCTouch *touch = touchIdToTouchObjectMap[touchId];
//                CCLOG("  touch id %d, touch object %p", touchId, touch);

                // pass event to node
                touchNode->retain();
                int ret = touchNode->ccTouchBegan(touch, pEvent);
                if (ret == kCCTouchBegan || ret == kCCTouchBeganNoSwallows)
                {
                    m_touchTargets->addObject(touchNode);
                    if (ret == kCCTouchBegan)
                    {
                        ignoredTouchId[touchId] = true;
                    }
                }
                else
                {
                    removeTouchNode = true;
                }
                touchNode->release();
            }
            else
            {
                removeTouchNode = true;
            }
        }
        else
        {
            // all at once
//            CCLOG("---- node %p [ALL AT ONCE]", touchNode);
            CCSet *touches = CCSet::create();
            for (TouchIdArrayIterator touchIdIt = touchIdArray.begin();
                 touchIdIt != touchIdArray.end();
                 ++touchIdIt)
            {
                touches->addObject(touchIdToTouchObjectMap[*touchIdIt]);
//                CCLOG("  touch id %d, touch object %p", *touchIdIt, touchIdToTouchObjectMap[*touchIdIt]);
            }

            touchNode->retain();
            int ret = touchNode->ccTouchesBegan(touches, pEvent);
            if (ret == kCCTouchBegan || ret == kCCTouchBeganNoSwallows)
            {
                m_touchTargets->addObject(touchNode);
            }
            else
            {
                removeTouchNode = true;
            }
            touchNode->release();
        }

        if (removeTouchNode)
        {
            m_nodeToTouchIdArrayMap.erase(touchNode);
            tmpTouchTargets->removeObjectAtIndex(i);
            i--;
            count--;
        }
    }

    sortAllTouchableNodes(m_touchTargets);
    return kCCTouchBegan;
}

int CCScene::ccTouchesMoved(CCSet *pTouches, CCEvent *pEvent)
{
    TouchIdToTouchObjectMap touchIdToTouchObjectMap = makeTouchIdToTouchObjectMap(pTouches);

    CCLOG("-------- MOVED");
    unsigned int count = m_touchTargets->count();
    for (unsigned int i = 0; i < count; ++i)
    {
        CCNode *touchNode = static_cast<CCNode*>(m_touchTargets->objectAtIndex(i));
        TouchIdArray &touchIdArray = m_nodeToTouchIdArrayMap[touchNode];
        if (touchNode->getTouchMode() == kCCTouchesOneByOne)
        {
            CCLOG("---- node %p [ONE BY ONE]", touchNode);
            int touchId = *touchIdArray.begin();
            CCLOG("  touch id %d, touch object %p", touchId, touchIdToTouchObjectMap[touchId]);
        }
        else
        {
            CCLOG("---- node %p [ALL AT ONES]", touchNode);
            CCSet *touches = CCSet::create();
            for (TouchIdArrayIterator touchIdIt = touchIdArray.begin();
                 touchIdIt != touchIdArray.end();
                 ++touchIdIt)
            {
                touches->addObject(touchIdToTouchObjectMap[*touchIdIt]);
                CCLOG("  touch id %d, touch object %p", *touchIdIt, touchIdToTouchObjectMap[*touchIdIt]);
            }

            touchNode->retain();
            int ret = touchNode->ccTouchesBegan(touches, pEvent);
            if (ret != kCCTouchBegan && ret != kCCTouchBeganNoSwallows)
            {
                m_nodeToTouchIdArrayMap.erase(touchNode);
                m_touchTargets->removeObjectAtIndex(i);
                i--;
                count--;
            }
            touchNode->release();
        }
    }

    return kCCTouchMoved;
}

void CCScene::ccTouchesEnded(CCSet *pTouches, CCEvent *pEvent)
{

}

void CCScene::ccTouchesCancelled(CCSet *pTouches, CCEvent *pEvent)
{

}

void CCScene::visit()
{
    g_drawOrder = 0;
    CCLayer::visit();
}

void CCScene::sortAllTouchableNodes(CCArray *nodes)
{
    int i, j, length = nodes->data->num;
    CCNode **x = (CCNode**)nodes->data->arr;
    CCNode *tempItem;

    // insertion sort
    for(i = 1; i < length; i++)
    {
        tempItem = x[i];
        j = i - 1;

        while(j >= 0 && (tempItem->m_nTouchPriority <= x[j]->m_nTouchPriority && tempItem->m_drawOrder > x[j]->m_drawOrder))
        {
            x[j + 1] = x[j];
            j = j - 1;
        }
        x[j + 1] = tempItem;
    }

    // debug
//    CCLOG("----------------------------------------");
//    for(i=0; i<length; i++)
//    {
//        tempItem = x[i];
//        CCLOG("[%03d] m_drawOrder = %u, w = %0.2f, h = %0.2f", i, tempItem->m_drawOrder, tempItem->getCascadeBoundingBox().size.width, tempItem->getCascadeBoundingBox().size.height);
//    }
}

TouchIdToTouchObjectMap CCScene::makeTouchIdToTouchObjectMap(CCSet *pTouches)
{
    TouchIdToTouchObjectMap map;
    for (CCSetIterator it = pTouches->begin(); it != pTouches->end(); ++it)
    {
        CCTouch *touch = (CCTouch*)*it;
        map[touch->getID()] = touch;
    }
    return map;
}

NS_CC_END
