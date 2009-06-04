/*
 *                         OpenSplice DDS
 *
 *   This software and documentation are Copyright 2006 to 2009 PrismTech 
 *   Limited and its licensees. All rights reserved. See file:
 *
 *                     $OSPL_HOME/LICENSE 
 *
 *   for full copyright notice and license terms. 
 *
 */
/* Interface */
#include "nw_channelUser.h"
#include "nw__channelUser.h"

/* Implementation */
#include "os_heap.h"
#include "v_entity.h"         /* for v_entity() */
#include "v_group.h"          /* for v_group() */
#include "v_topic.h"
#include "v_domain.h"
#include "v_networkReader.h"
#include "nw__confidence.h"
#include "nw_commonTypes.h"
#include "nw_report.h"

/* -------------------------------- AdminMessage ---------------------------- */

NW_CLASS(nw_adminMessage);

typedef enum nw_adminMessageKind_e {
    NW_MESSAGE_NEW_GROUP
} nw_adminMessageKind;

NW_STRUCT(nw_adminMessage) {
    nw_adminMessageKind kind;
    v_networkReaderEntry entry;
};


static nw_adminMessage
nw_adminMessageNew(
    nw_adminMessageKind kind,
    v_networkReaderEntry entry)
{
    nw_adminMessage result = NULL;
    
    result = (nw_adminMessage)os_malloc((os_uint32)sizeof(*result));
    
    if (result != NULL) {
        result->kind = kind;
        result->entry = entry; /* Intentionally no keep, we are in an action routine */
    }
    
    return result;
}


static void
nw_adminMessageFree(
    nw_adminMessage message)
{
    if (message) {
        os_free(message);
    }
}


/* --------------------------------- ChannelUser ---------------------------- */



/* Protected members */
void
nw_channelUserInitialize(
    nw_channelUser channelUser,
    const char *name,
    const char *pathName,
    u_networkReader reader,
    const nw_runnableMainFunc runnableMainFunc,
    const nw_runnableTriggerFunc runnableTriggerFunc,
    const nw_runnableFinalizeFunc runnableFinalizeFunc)
{
    /* Initialize parent */
    nw_runnableInitialize((nw_runnable)channelUser, name, pathName,
                          runnableMainFunc, NULL, runnableTriggerFunc,
                          runnableFinalizeFunc);

    if (channelUser) {
        channelUser->reader = reader;
        channelUser->messageBuffer = c_iterNew(NULL);
        c_mutexInit(&channelUser->messageBufferMutex, PRIVATE_MUTEX);
    }
}


c_bool
nw_channelUserRetrieveNewGroup(
    nw_channelUser channelUser,
    v_networkReaderEntry *entry)
{
    c_bool result = FALSE;
    nw_adminMessage message;
    
    if (channelUser) {
        if ( c_iterLength(channelUser->messageBuffer) == 0 ) {
            c_mutexLock(&channelUser->messageBufferMutex);
            message = (nw_adminMessage)c_iterTakeFirst(channelUser->messageBuffer);
            c_mutexUnlock(&channelUser->messageBufferMutex);
            
            *entry = message->entry;
            result = TRUE;
            nw_adminMessageFree(message);
        }
    }
    
    return result;
}

/* Protected */
                
void
nw_channelUserFinalize(
    nw_channelUser channelUser)
{
    /* Finalize self */
    if (channelUser) {
        c_iterFree(channelUser->messageBuffer); 
        /* c_mutexDestroy(&channelUser->messageBufferMutex);*/

        /* Finalize parent */
        nw_runnableFinalize((nw_runnable)channelUser);
    
    }
}


/* Public members */

struct onNewGroupArg {
    nw_channelUser channelUser;
    v_networkReaderEntry entry;
};

static void
onNewGroup(
    v_entity e,
    c_voidp arg)
{
    v_networkReader reader;
    struct onNewGroupArg *onNewGroupArg;
    nw_channelUser channelUser;
    nw_adminMessage toPost;
    v_networkReaderEntry entry;
    
    reader = v_networkReader(e);
    NW_CONFIDENCE(reader);
    onNewGroupArg = (struct onNewGroupArg *)arg;
    entry = onNewGroupArg->entry;
    NW_CONFIDENCE(entry);
    channelUser = onNewGroupArg->channelUser;
    NW_CONFIDENCE(channelUser);
    
    toPost = nw_adminMessageNew(NW_MESSAGE_NEW_GROUP, entry);
    if (toPost) {
        /* Post the message in the buffer */
        c_mutexLock(&channelUser->messageBufferMutex);
        c_iterAppend(channelUser->messageBuffer, toPost);
        c_mutexUnlock(&channelUser->messageBufferMutex);
        
        /* Wake up the channelUser for processing this message */
        nw_runnableTrigger((nw_runnable)channelUser);
    }
}



void
nw_channelUserNotifyNewGroup(
    nw_channelUser channelUser,
    v_networkReaderEntry entry)
{
    struct onNewGroupArg onNewGroupArg;
    
    if (channelUser) {
        onNewGroupArg.entry = entry;
        onNewGroupArg.channelUser = channelUser;
        u_entityAction(u_entity(channelUser->reader), onNewGroup, &onNewGroupArg);
    }
}
