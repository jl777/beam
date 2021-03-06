/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include <stdio.h>

#ifdef FROM_BEAM
extern int beam_main(int argc, char* argv[]);

int main(int argc,char *argv[])
{
    return(beam_main(argc,argv));
}
#else

#include <stdlib.h>
#include <string.h>

int beam_main(int argc, char* argv[])
{
    char cmdbuf[512];
    strcpy(cmdbuf,"./beam-node");
    fprintf(stderr,"system(%s)\n",cmdbuf);
    return(system(cmdbuf));
}
#endif

