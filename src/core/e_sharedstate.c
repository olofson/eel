/*
---------------------------------------------------------------------------
	e_sharedstate.c - EEL process-wide shared state management
---------------------------------------------------------------------------
 * Copyright 2014 David Olofson
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the
 * use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <stdio.h>
#include "e_sharedstate.h"
#include "e_platform.h"
#include "e_exceptions.h"


static EEL_atomic eel_api_users = 0;
static EEL_atomic eel_api_up = 0;
static EEL_xno eel_api_error = EEL_XOK;


int eel_add_api_user(void)
{
	if(eel_atomic_add(&eel_api_users, 1) == 0)
	{
		EEL_xno x;
		/* We could arrive here right when the API is being closed! */
		while(eel_atomic_add(&eel_api_up, 0))
			eel_yield();
		eel_api_error = EEL_XOK;
		if((x = eel_x_open_registry()))
		{
			eel_api_error = x;
			eel_atomic_add(&eel_api_users, -1);
			return x;
		}
		eel_atomic_add(&eel_api_up, 1);
	}
	else
	{
		/* Someone beat us to it. Wait until the API is actually up! */
		while(!eel_atomic_add(&eel_api_up, 0))
		{
			if(eel_api_error)
			{
				/*
				 * Oh... The thread opening the API failed.
				 * We're not likely going to succeed either,
				 * so we return the same error code.
				 */
				eel_atomic_add(&eel_api_users, -1);
				return eel_api_error;
			}
			eel_yield();
		}
	}
	return EEL_XOK;
}


void eel_remove_api_user(void)
{
	int users = eel_atomic_add(&eel_api_users, -1);
	if(users == 1)
	{
		/*
		 * If someone tries to reopen now, eel_add_api_user() will wait
		 * until we're done closing, before opening again.
		 */
		eel_x_close_registry();
		eel_atomic_add(&eel_api_up, -1);
	}
	else if(!users)
	{
		eel_atomic_add(&eel_api_users, 1);
		fprintf(stderr, "EEL INTERNAL ERROR: "
				"eel_remove_api_user() called while "
				"eel_api_users == 0!\n");
	}
}
