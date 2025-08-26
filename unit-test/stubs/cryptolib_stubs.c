/*
** CryptoLib Stub Functions for Unit Testing
*/

#include "radio_unit_test_types.h"
#include <string.h>

/*
** Global stub variables for test control
*/
int32 UT_CRYPTO_TC_ProcessSecurity_ReturnValue = CRYPTO_LIB_SUCCESS;
int32 UT_CRYPTO_TM_ApplySecurity_ReturnValue = CRYPTO_LIB_SUCCESS;
bool  UT_CRYPTO_Enable_Stubs = true;

/*
** Stub for Crypto_TC_ProcessSecurity
*/
int32 Crypto_TC_ProcessSecurity(uint8* tc_frame, int32* frame_size, TC_t* tc_struct)
{
    if (!UT_CRYPTO_Enable_Stubs)
        return CRYPTO_LIB_ERROR;
        
    if (tc_struct != NULL && tc_frame != NULL && frame_size != NULL)
    {
        /* Simple stub - just copy input frame to output structure */
        tc_struct->tc_pdu_len = (uint16)*frame_size;
        if (*frame_size > 0 && *frame_size <= 1024)
        {
            memcpy(tc_struct->tc_pdu, tc_frame, *frame_size);
        }
    }
    
    return UT_CRYPTO_TC_ProcessSecurity_ReturnValue;
}

/*
** Stub for Crypto_TM_ApplySecurity
*/
int32 Crypto_TM_ApplySecurity(uint8* tm_frame, int32 frame_len)
{
    if (!UT_CRYPTO_Enable_Stubs)
        return CRYPTO_LIB_ERROR;
        
    /* Simple stub - no actual crypto processing */
    return UT_CRYPTO_TM_ApplySecurity_ReturnValue;
}

/*
** Stub for get_sa_interface_inmemory
*/
SaInterface get_sa_interface_inmemory(void)
{
    if (!UT_CRYPTO_Enable_Stubs)
        return NULL;
        
    /* Return a simple stub interface */
    static struct SaInterfaceStruct stub_interface = {0};
    return &stub_interface;
}
